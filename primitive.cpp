#include "primitive.h"
#include "time_sampler.h"

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
	m_nsi.Connect( parent, "", m_object->getFullPath().c_str(), "objects" );

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

bool primitive::is_volume()const
{
	return false;
}

bool primitive::export_extrapolated_P(GT_DataArrayHandle i_vertices_list)const
{
	// Retrieve velocity data handle
	GT_DataArrayHandle velocity_data;
	NSIType_t velocity_nsi_type;
	int velocity_nsi_flags;
	bool velocity_point_attribute;
	if(!find_attribute(
			*default_gt_primitive().get(),
			k_velocity_attribute,
			velocity_data,
			velocity_nsi_type,
			velocity_nsi_flags,
			velocity_point_attribute) ||
		velocity_nsi_type != NSITypeVector)
	{
		// Velocity is not available
		return false;
	}

	unsigned nb_velocities = velocity_data->entries();

	// Retrieve position data handle
	GT_DataArrayHandle position_data;
	NSIType_t position_nsi_type;
	int position_nsi_flags;
	bool position_point_attribute;
	if(!find_attribute(
			*default_gt_primitive().get(),
			k_position_attribute,
			position_data,
			position_nsi_type,
			position_nsi_flags,
			position_point_attribute) ||
		position_nsi_type != NSITypePoint)
	{
		// Position is not available
		// This is very weird, most likely impossible
		assert(false);
		return false;
	}

	unsigned nb_points = position_data->entries();

	if(nb_velocities != 1 && nb_points != nb_velocities)
	{
		// The number of velocities don't match the number of points
		// This is very weird, most likely impossible
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

	// Compute pre-frame position from frame position (1 second earlier)
	for(unsigned p = 0, v = 0; p < 3*nb_points; p++, v += v_inc)
	{
		nsi_position_data[p] -= nsi_velocity_data[v];
	}

	// Output pre-frame position
	m_nsi.SetAttributeAtTime(
		m_handle,
		time - 1.0,
		NSI::PointsArg("P", nsi_position_data, nb_points));

	// Compute post-frame position from the pre-frame position (2 seconds later)
	for(unsigned p = 0, v = 0; p < 3*nb_points; p++, v += v_inc)
	{
		nsi_position_data[p] += 2.0f * nsi_velocity_data[v];
	}

	// Output post-frame position
	m_nsi.SetAttributeAtTime(
		m_handle,
		time + 1.0,
		NSI::PointsArg("P", nsi_position_data, nb_points));

	delete[] nsi_position_data;

	// Output P.indices if necessary
	if(position_point_attribute && i_vertices_list)
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
