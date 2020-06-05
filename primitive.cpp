#include "primitive.h"

#include "geometry.h"
#include "time_sampler.h"

#include <unordered_map>

#include <OBJ/OBJ_Node.h>
#include <VOP/VOP_Node.h>
#include <OP/OP_Node.h>
#include <GT/GT_PrimPolygonMesh.h>

#include <nsi.hpp>

namespace
{
	const std::string k_position_attribute = "P";
	const std::string k_velocity_attribute = "v";
}

primitive::primitive(
	const context& i_context,
	OBJ_Node* i_object,
	double i_time,
	const GT_PrimitiveHandle& i_gt_primitive,
	unsigned i_primitive_index,
	bool i_requires_frame_aligned_sample)
	:	exporter(i_context, i_object),
		m_requires_frame_aligned_sample(i_requires_frame_aligned_sample)
{
	add_time_sample(i_time, i_gt_primitive);

	/*
		Geometry uses its full path + a prefix as a handle. So that
		it leaves the full path handle to the parent transform.
	*/
	m_handle += "|object|" + std::to_string(i_primitive_index);
}

void
primitive::connect()const
{
	/*
		The right place to do this as we need all the materials to be
		already exported.
	*/
	assign_sop_materials();

	assert(m_object);

	if( m_instanced )
	{
		/**
			This geometry will be connected to the "sourcemodels" of an NSI
			instances node and doesn't need to be connected to any other
			parent.
		*/
		return;
	}

	/*
		We support the transformation of the GT_Primitive by inserting
		a local NSI transform node between the object and its parent.
	*/
	std::string parent = m_handle + "|transform";
	const GT_TransformHandle &transform =
		default_gt_primitive()->getPrimitiveTransform();
	UT_Matrix4D matrix;
	transform->getMatrix( matrix );

	m_nsi.Create( parent, "transform" );
	m_nsi.SetAttribute( parent,
			NSI::DoubleMatrixArg( "transformationmatrix", matrix.data() ) );
	m_nsi.Connect( parent, "", geometry::hub_handle(*m_object), "objects" );

	m_nsi.Connect( m_handle, "", parent, "objects" );
}

void primitive::set_attributes()const
{
	for(const TimedPrimitive& prim : m_gt_primitives)
	{
		set_attributes_at_time(prim.first, prim.second);
	}
}

bool primitive::add_time_sample(
	double i_time,
	const GT_PrimitiveHandle& i_primitive)
{
	if(!m_gt_primitives.empty() &&
		m_gt_primitives.front().second->getPrimitiveType() !=
			i_primitive->getPrimitiveType())
	{
		// All GT primitives must be of the same type. This is an error.
		return false;
	}

	if(i_time != m_context.m_current_time && m_requires_frame_aligned_sample)
	{
		// We simply don't need this time sample. Not an error.
		return true;
	}

	if(!m_gt_primitives.empty() && i_time < m_gt_primitives.back().first)
	{
		/*
			This sample arrived out of chronological order. This is supposed to
			happen only when creating an additional time sample for primitives
			that require one that is aligned on a frame. It can be safely
			ignored by the others.
		*/
		assert(i_time == m_context.m_current_time);
		if(!m_requires_frame_aligned_sample)
		{
			return true;
		}
	}

	m_gt_primitives.push_back(TimedPrimitive(i_time, i_primitive));

	return true;
}

bool primitive::merge_time_samples(const primitive& i_primitive)
{
	for(const TimedPrimitive& prim : i_primitive.m_gt_primitives)
	{
		if(!add_time_sample(prim.first, prim.second))
		{
			return false;
		}
	}

	return true;
}

bool primitive::is_volume()const
{
	return false;
}

bool primitive::export_extrapolated_P(GT_DataArrayHandle i_vertices_list)const
{
	GT_Owner owner;
	GT_DataArrayHandle velocity_data =
		default_gt_primitive()->findAttribute(
			k_velocity_attribute, owner, 0 /* segment */ );

	if( !velocity_data || velocity_data->getTupleSize()!=3 )
	{
		// Velocity is not available
		return false;
	}

	unsigned nb_velocities = velocity_data->entries();

	// Retrieve position data handle
	GT_Owner p_owner;
	GT_DataArrayHandle position_data =
		default_gt_primitive()->findAttribute(
			k_position_attribute, p_owner, 0 /* segment */ );

	if( !position_data || position_data->getTupleSize()!=3 )
	{
		/*
			Position is not available. This is very weird, most likely
			impossible.
		*/
		assert( false );
		return false;
	}

	unsigned nb_points = position_data->entries();

	if(nb_velocities != 1 && nb_points != nb_velocities)
	{
		/*
			The number of velocities don't match the number of points. This
			is very weird, most likely impossible.
		*/
		assert(false);
		return false;
	}

	/*
		Generate positions at 2 time samples using the position and velocity of
		each particle.
	*/

	float time = m_context.m_current_time;

	// Increment for velocity indexing. Allows support for uniform velocity.
	unsigned v_inc = nb_velocities == 1 ? 0 : 1;

	// Retrieve position data in a writable buffer
	float* nsi_position_data = new float[nb_points*3];
	position_data->fillArray(nsi_position_data, 0, nb_points, 3);

	// Retrieve velocity data
	GT_DataArrayHandle velocity_buffer;
	const float* nsi_velocity_data = velocity_data->getF32Array(velocity_buffer);

	/*
		Compute pre-frame position from frame position (typically 1 half-shutter
		earlier).
	*/
	double velocity_weight = m_context.ShutterOpen() - time;
	for(unsigned p = 0, v = 0; p < 3*nb_points; p++, v += v_inc)
	{
		nsi_position_data[p] += velocity_weight * nsi_velocity_data[v];
	}

	// Output pre-frame position
	m_nsi.SetAttributeAtTime(
		m_handle,
		m_context.ShutterOpen(),
		NSI::PointsArg("P", nsi_position_data, nb_points));

	// Compute post-frame position from the pre-frame position (1 shutter later)
	velocity_weight = m_context.ShutterClose() - m_context.ShutterOpen();
	for(unsigned p = 0, v = 0; p < 3*nb_points; p++, v += v_inc)
	{
		nsi_position_data[p] += velocity_weight * nsi_velocity_data[v];
	}

	// Output post-frame position
	m_nsi.SetAttributeAtTime(
		m_handle,
		m_context.ShutterClose(),
		NSI::PointsArg("P", nsi_position_data, nb_points));

	delete[] nsi_position_data;

	// Output P.indices if necessary
	if( p_owner==GT_OWNER_POINT && i_vertices_list)
	{
		GT_DataArrayHandle vertices_buffer;
		const int *vertices = i_vertices_list->getI32Array(vertices_buffer);
		m_nsi.SetAttribute(
			m_handle,
			NSI::IntegersArg("P.indices", vertices, i_vertices_list->entries()));
	}

	return true;
}

bool primitive::has_velocity(const GT_PrimitiveHandle& i_gt_prim)
{
	GT_Owner owner;
	return (bool)i_gt_prim->findAttribute(k_velocity_attribute, owner, 0);
}

/**
	We scan for all assigned shaders on this primitive. If we don't
	have attribute-level assignments (SOP assignment) then  we scan
	the OBJ-level shader.
*/
void primitive::export_bind_attributes( OP_Node *i_obj_level_material ) const
{
	GT_DataArrayHandle i_vertices_list;
	GT_Primitive *primitive = default_gt_primitive().get();
	int type = primitive->getPrimitiveType();
	if( type == GT_PRIM_POLYGON_MESH ||
		type == GT_PRIM_SUBDIVISION_MESH )
	{
		const GT_PrimPolygonMesh *polygon_mesh =
			static_cast<const GT_PrimPolygonMesh *>(primitive);

		i_vertices_list = polygon_mesh->getVertexList();
	}

	GT_Owner owner;
	GT_DataArrayHandle materials = default_gt_primitive().get()->findAttribute(
		"shop_materialpath", owner, 0);

	std::vector< OP_Node * > to_scan;
	if( materials )
	{
		for( int i=0; i<materials->entries(); i++ )
		{
			std::string resolved;
			VOP_Node *vop = exporter::resolve_material_path(
				(const char *)materials->getS(i), resolved );
			to_scan.push_back( vop );
		}
	}
	else
	{
		/* Revert to object level */
		to_scan.push_back( i_obj_level_material );
	}

	std::vector< std::string > binds;
	get_bind_attributes( to_scan, binds );

	std::sort( binds.begin(), binds.end() );
	binds.erase( std::unique(binds.begin(), binds.end()), binds.end() );

	/*
		Remove attributes that are exported anyway.
	*/
	binds.erase(
		std::remove_if(
			binds.begin(),
			binds.end(),
			[](const std::string &a)
			{
				return a == "P" || a == "N" || a == "uv" || a == "width" ||
				a == "id" || a == "pscale" || a == "rest" || a == "rnml";
			} ),
		binds.end() );

	export_attributes(
		binds,
		*default_gt_primitive().get(),
		m_context.m_current_time, i_vertices_list );
}

void primitive::get_bind_attributes(
	std::vector< OP_Node * > &i_materials,
	std::vector< std::string > &o_to_export ) const
{
	std::vector<VOP_Node*> aov_export_nodes;

	std::vector<OP_Node*> traversal = i_materials;
	while( traversal.size() )
	{
		OP_Node* node = traversal.back();
		traversal.pop_back();

		if( !node )
			continue;

		int ninputs = node->nInputs();
		for( int i = 0; i < ninputs; i++ )
		{
			VOP_Node *input = CAST_VOPNODE( node->getInput(i) );
			if( !input )
				continue;

			OP_Operator* op = input->getOperator();
			if( !op )
				continue;

			if( op->getName() == "3Delight::dlPrimitiveAttribute" ||
				op->getName() == "3Delight::dlAttributeRead")
			{
				UT_String primvar;
				input->evalString(
					primvar, "attribute_name", 0, m_context.m_current_time );
				o_to_export.push_back( primvar.toStdString() );
			}
			else
			{
				traversal.push_back( input );
			}
		}
	}
}

void primitive::get_all_material_paths(
	std::unordered_set< std::string > &o_materials ) const
{
	GT_Owner owner;
	GT_DataArrayHandle materials = default_gt_primitive().get()->findAttribute(
		"shop_materialpath", owner, 0);

	if( !materials )
		return;

	for( int i=0; i<materials->entries() ; i++ )
	{
		const char *path = materials->getS( i );
		std::string resolved;
		exporter::resolve_material_path( path, resolved );

		if( !resolved.empty() )
			o_materials.insert( resolved );
	}
}

void primitive::assign_sop_materials( void ) const
{
	/* Priority to uniform/primitive materials */
	GT_Owner type;
	GT_DataArrayHandle materials = default_gt_primitive().get()->findAttribute(
		"shop_materialpath", type, 0);

	if( !materials || materials->getStorage()!=GT_STORE_STRING )
	{
		return;
	}

	if( materials->entries() == 1u )
	{
		/*
			Could be a detail attribute or just one-faced poly, no need
			to go further as we can just create one attribute node.
		*/
		std::string shop( materials->getS(0) );


		if( resolve_material_path(m_object, shop.c_str(), shop) )
		{
			std::string attribute_handle = m_handle + shop;

			m_nsi.Create( attribute_handle, "attributes" );
			m_nsi.Connect(
				shop, "",
				 attribute_handle, "surfaceshader",
				 (
					NSI::IntegerArg("priority", 1),
					NSI::IntegerArg("strength", 1)
				) );
			m_nsi.Connect(attribute_handle, "", m_handle, "geometryattributes" );
		}

		return;
	}

	// Retrieve a context that might redirect the attributes to a shared file
	NSI::Context& static_nsi = attributes_context();

	/*
		We will need per-face assignments.  Build a material -> uniform/face map
	*/
	std::unordered_map< std::string, std::vector<int> > all_materials;
	for( int i=0; i<materials->entries(); i++ )
	{
		std::string shop( materials->getS(i) );
		if( resolve_material_path(m_object, shop.c_str(), shop) )
		{
			all_materials[ shop ].push_back( i );
		}
	}

	/* Create the NSI face sets + attributes and connect to geo */
	for( auto material : all_materials )
	{
		std::string attribute_handle = m_handle + material.first;
		std::string set_handle = m_handle + material.first + "|set";

		m_nsi.Create( attribute_handle, "attributes" );
		m_nsi.Create( set_handle, "faceset" );
		m_nsi.Connect( material.first, "", attribute_handle, "surfaceshader" );
		m_nsi.Connect( attribute_handle, "", set_handle, "geometryattributes" );
		m_nsi.Connect( set_handle, "", m_handle, "facesets" );

		// This attribute may already have been exported in a previous frame
		if(static_nsi.Handle() != NSI_BAD_CONTEXT)
		{
			static_nsi.SetAttribute( set_handle,
				*NSI::Argument( "faces" )
					.SetType( NSITypeInteger )
					->SetCount( material.second.size() )
					->SetValuePointer( &material.second[0] ) );
		}
	}
}
