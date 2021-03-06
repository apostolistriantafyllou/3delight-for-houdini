#include "primitive.h"

#include "geometry.h"
#include "time_sampler.h"
#include "vop.h"

#include <unordered_map>

#include <OBJ/OBJ_Node.h>
#include <VOP/VOP_Node.h>
#include <OP/OP_Node.h>
#include <GT/GT_PrimPolygonMesh.h>

#include <nsi.hpp>
#include "scene.h"

namespace
{
	const std::string k_position_attribute = "P";
	const std::string k_velocity_attribute = "v";
	const char *k_shader_slot_names[3] =
		{ "surfaceshader", "displacementshader", "volumeshader" };

}

primitive::primitive(
	const context& i_context,
	OBJ_Node* i_object,
	double i_time,
	const GT_PrimitiveHandle& i_gt_primitive,
	unsigned i_primitive_index )
	:	exporter(i_context, i_object)
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
	m_nsi.Connect(
		parent, "",
		geometry::hub_handle(*m_object, m_context), "objects" );

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

	assert(m_gt_primitives.empty() || i_time > m_gt_primitives.back().first);

	/*
		Do not try to output mismatching moving geo. 3DelightNSI will complain
		and refuse to render it. This can happen in simulations and the solution
		is to use a velocity attribute.
	*/
	if( !m_gt_primitives.empty() )
	{
		GT_Owner type1, type2;
		auto P_main =
			m_gt_primitives.back().second->findAttribute( "P", type1, 0 );
		auto P_new = i_primitive->findAttribute( "P", type2, 0 );

		if(!P_main != !P_new)
		{
			// "P" is defined in only one of the primitives
			return false;
		}

		if(P_main && P_new &&
			(P_main->entries() != P_new->entries() || type1 != type2) )
		{
			return false;
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

bool primitive::has_velocity_blur( void ) const
{
	if( !m_context.MotionBlur() )
		return false;

	GT_Owner owner;
	return (bool)default_gt_primitive()->findAttribute(
		k_velocity_attribute, owner, 0);
}

/**
	We scan for all assigned shaders on this primitive. We scan
	both SOP-level and OBJ-level shaders as both could be used
	at the same time.
*/
void primitive::export_bind_attributes( VOP_Node *i_obj_level_material[3] ) const
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
		UT_StringArray mats;
		materials->getStrings(mats);
		for (int i = 0; i < mats.entries(); i++)
		{
			std::string m( mats[i] );
			VOP_Node *mats[3] = { nullptr };
			resolve_material_path( m.c_str(), mats );
			for( int i=0; i<3; i++ )
			{
				if( mats[i] )
					to_scan.push_back( mats[i] );
			}
		}
	}

	/*
		Also scan OBJ-level assignment. It could be used for empty SOP-level
		assignments.
	*/
	if( i_obj_level_material )
	{
		if( i_obj_level_material[0] )
			to_scan.push_back( i_obj_level_material[0] ); // Surface
		if( i_obj_level_material[1] )
			to_scan.push_back( i_obj_level_material[1] ); // Displaement
		if( i_obj_level_material[2] )
			to_scan.push_back( i_obj_level_material[2] ); // Volume
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
			else if( op->getName() == "3Delight::dlUV" )
			{
				UT_String primvar;
				input->evalString(
					primvar, "uvSet", 0, m_context.m_current_time );
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

	UT_StringArray mats;
	materials->getStrings( mats );

	for( int i=0; i<mats.entries() ; i++ )
	{
		VOP_Node* vops[3] = { nullptr };
		resolve_material_path( mats[i].c_str(), vops );

		for( int i=0; i<3; i++ )
		{
			if( vops[i] )
				o_materials.insert( vops[i]->getFullPath().toStdString() );
		}
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

	std::unordered_set<std::string> uniqued;
	for( int i = 0; i<materials->entries(); i++ )
	{
		std::string m( materials->getS(i) );
		uniqued.insert(m );
	}

	bool single_material = uniqued.size() == 1u;

	if( single_material )
	{
		/*
			Could be a detail attribute or just one-faced poly, no need
			to go further as we can just create one attribute node.
		*/
		std::string shop( materials->getS(0) );

		VOP_Node* mats[3] = {nullptr};
		resolve_material_path(m_object, shop.c_str(), mats );
		for( int i=0; i<3; i++ )
		{
			VOP_Node *vop = mats[i];
			if( !vop )
				continue;

			std::string vop_handle = vop::handle(*vop, m_context);
			std::string attribute_handle = m_handle + "|" + vop_handle;
			m_nsi.Create( attribute_handle, "attributes" );

			geometry::update_materials_mapping(vop, m_context, m_object);
			if( vop::is_texture(vop) )
			{
				geometry::connect_texture(vop , m_object, m_context, attribute_handle);
			}
			else
			{
				m_nsi.Connect(
					vop_handle, "",
					attribute_handle, k_shader_slot_names[i],
					(
						NSI::IntegerArg("priority", 1),
						NSI::IntegerArg("strength", 1)
					) );
			}

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
		all_materials[ shop ].push_back( i );
	}

	/* Create the NSI face sets + attributes and connect to geo */
	for( auto material : all_materials )
	{
		const std::string &m = material.first;
		VOP_Node* mats[3];
		resolve_material_path( m_object, m.c_str(), mats );

		for( int i=0; i<3; i++ )
		{
			VOP_Node *V = mats[i];

			if( !V )
			{
				/* Will be dealt with by OBJ-level assignments */
				continue;
			}

			std::string vop_handle = vop::handle(*V, m_context);
			std::string attribute_handle = m_handle + "|" + vop_handle;
			std::string set_handle = attribute_handle + "|set";

			m_nsi.Create( attribute_handle, "attributes" );
			m_nsi.Create( set_handle, "faceset" );

			geometry::update_materials_mapping(V, m_context, m_object);
			if( vop::is_texture(V) )
			{
				geometry::connect_texture(V, m_object, m_context, attribute_handle);
			}
			else
			{
				m_nsi.Connect(
						vop_handle, "",
						attribute_handle, "surfaceshader",
						NSI::IntegerArg("strength", 1));
			}

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
}
