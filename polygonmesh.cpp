#include "polygonmesh.h"

#include "context.h"
#include "time_sampler.h"

#include <GA/GA_Names.h>
#include <GT/GT_PrimPolygonMesh.h>
#include <unordered_map>
#include <OBJ/OBJ_Node.h>
#include <nsi.hpp>

#include <type_traits>

polygonmesh::polygonmesh(
	const context& i_ctx,
	OBJ_Node *i_object,
	double i_time,
	const GT_PrimitiveHandle &i_gt_primitive,
	unsigned i_primitive_index,
	bool i_force_subdivision )
:
	primitive(
		i_ctx,
		i_object,
		i_time,
		i_gt_primitive,
		i_primitive_index,
		has_velocity(i_gt_primitive)),
	m_is_subdiv(i_force_subdivision)
{
	if(!m_is_subdiv)
	{
		const char *k_subdiv = "_3dl_render_poly_as_subd";
		m_is_subdiv =
			m_object->hasParm(k_subdiv) &&
			m_object->evalInt(k_subdiv, 0, m_context.m_current_time) != 0;
	}
}

void polygonmesh::create( void ) const
{
	m_nsi.Create( m_handle.c_str(), "mesh" );
}

/**
	\brief Output "P.indices" and "nvertices" for this mesh.

	Those are not time varying and can be output once.
*/
void polygonmesh::set_attributes( void ) const
{
	const GT_PrimPolygonMesh *polygon_mesh =
		static_cast<const GT_PrimPolygonMesh *>(default_gt_primitive().get());

	NSI::ArgumentList mesh_args;

	/* Our dear Houdini friends are RenderMan affectionados */
	mesh_args.Add( new NSI::IntegerArg("clockwisewinding", 1) );

	/*
		Prepare the 'nvertices' attribute which contains, for each face,
		the total number of points.
	*/
	const GT_CountArray &count_array = polygon_mesh->getFaceCountArray();
	std::unique_ptr<int> nvertices( new int[count_array.entries()] );
	for( GT_Size i=0; i<count_array.entries(); i++ )
	{
		nvertices.get()[i] = count_array.getCount(i);
	}

	mesh_args.Add( NSI::Argument::New( "nvertices" )
		->SetType( NSITypeInteger )
		->SetCount( count_array.entries() )
		->SetValuePointer( nvertices.get() ) );

	if (m_is_subdiv)
	{
		mesh_args.Add(
			new NSI::StringArg("subdivision.scheme", "catmull-clark") );
	}

	// Retrieve a context that might redirect the attributes to a shared file
	NSI::Context& nsi = static_attributes_context();
	// Those attributes may already have been exported in a previous frame
	if(nsi.Handle() != NSI_BAD_CONTEXT)
	{
		nsi.SetAttribute( m_handle, mesh_args );
	}

	const GT_DataArrayHandle& vertices_list = polygon_mesh->getVertexList();

	std::vector< std::string > to_export{ "uv" };
	exporter::export_attributes(
		to_export, *polygon_mesh, m_context.m_current_time, vertices_list );

	exporter::export_bind_attributes(*polygon_mesh, vertices_list );

	export_creases(
		polygon_mesh->getVertexList(),
		nvertices.get(),
		count_array.entries() );

	if(!requires_frame_aligned_sample() ||
		time_sampler(
			m_context,
			*m_object, time_sampler::e_deformation).nb_samples() <= 1 ||
		!export_extrapolated_P(polygon_mesh->getVertexList()))
	{
		// Export attributes at each time sample
		primitive::set_attributes();
		return;
	}

	// Export normals at a single time sample
	if(!m_is_subdiv)
	{
		std::vector<std::string> to_export(1, "N");
		exporter::export_attributes(
			to_export,
			*polygon_mesh,
			m_context.m_current_time,
			polygon_mesh->getVertexList());
	}
}

/**
	\brief output P, N

	\ref http://www.sidefx.com/docs/hdk/_g_t___prim_polygon_mesh_8h_source.html
*/
void polygonmesh::set_attributes_at_time(
	double i_time,
	const GT_PrimitiveHandle i_gt_primitive) const
{
	const GT_PrimPolygonMesh *polygon_mesh =
		static_cast<const GT_PrimPolygonMesh *>(i_gt_primitive.get());

	std::vector< std::string > to_export{ "P", "rest" };
	if( !m_is_subdiv )
	{
		to_export.push_back( "N" );
		to_export.push_back( "rnml" );
	}

	exporter::export_attributes(
		to_export, *polygon_mesh, i_time, polygon_mesh->getVertexList() );
}

void polygonmesh::connect( void ) const
{
	/*
		The right place to do this as we need all the materials to be
		already exported.
	*/
	assign_primitive_materials();

	primitive::connect();
}

void polygonmesh::assign_primitive_materials( void ) const
{
	GT_AttributeListHandle uniforms =
		default_gt_primitive().get()->getUniformAttributes();
	GT_AttributeListHandle details =
		default_gt_primitive().get()->getDetailAttributes();

	GT_DataArrayHandle uniform_materials(
		uniforms ? uniforms->get("shop_materialpath") : nullptr);
	GT_DataArrayHandle detail_materials(
		details ? details->get("shop_materialpath") : nullptr);

	/* Priority to uniform/primitive materials */
	GT_DataArrayHandle materials =
		uniform_materials ? uniform_materials : detail_materials;

	if( !materials || materials->getStorage()!=GT_STORE_STRING )
	{
		return;
	}

	if( materials->entries() == 1 )
	{
		/*
			This could be a single faced geo OR a detail material assignment.
			Deal with it using the usual attribute assignment.
		*/
		std::string shop;
		resolve_material_path( materials->getS(0), shop );

		if( shop.empty() )
			return;

		std::string attribute_handle = m_handle + shop;

		NSI::ArgumentList priority;
		priority.Add(new NSI::IntegerArg("priority", 1));

		m_nsi.Create( attribute_handle, "attributes" );
		m_nsi.Connect( shop, "", attribute_handle, "surfaceshader", priority );
		m_nsi.Connect( attribute_handle, "", m_handle, "geometryattributes" );

		return;
	}

	// Retrieve a context that might redirect the attributes to a shared file
	NSI::Context& static_nsi = static_attributes_context();

	/*
		We will need per-face assignments.  Build a material -> uniform/face map
	*/
	std::unordered_map< std::string, std::vector<int> > all_materials;
	for( GT_Offset i=0; i<materials->entries(); i++ )
	{
		std::string shop;
		resolve_material_path( materials->getS(i), shop );

		if( shop.empty() )
			continue;
		all_materials[ shop ].push_back( i );
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

/**
	We export creases even when using polygon meshes as these could be usefull
	for dlToon shaders.

	We don't have an access to edge information here so we must build it
	ourselves from the face structure.

	FIXME: I have no idea if there is "corners" in Houdini specs as we have in
	Maya.
*/
void polygonmesh::export_creases(
	GT_DataArrayHandle i_indices, int *i_nvertices, size_t i_n ) const
{
	// Retrieve a context that might redirect the attributes to a shared file
	NSI::Context& nsi = static_attributes_context();
	if(nsi.Handle() == NSI_BAD_CONTEXT)
	{
		// Those attributes have already been exported in a previous frame
		return;
	}

	GT_AttributeListHandle vertex =
		default_gt_primitive().get()->getVertexAttributes();

	if( !vertex )
		return;

	const GT_DataArrayHandle &creaseweight = vertex->get("creaseweight");

	if( !creaseweight )
		return;

	GT_DataArrayHandle buffer_in_case_we_need_it;
	float *weights = (float *)creaseweight->getF32Array(buffer_in_case_we_need_it);

	if( weights == nullptr )
	{
		assert( false );
		return;
	}

	GT_DataArrayHandle buffer_in_case_we_need_it_2;
	const int *indices = i_indices->getI32Array(buffer_in_case_we_need_it_2);

	/** Build edges from consecutive vertices that have weights > 0 */
	std::vector<int> crease_indices;
	std::vector<float> crease_sharpness;

	int current_vertex = 0;
	for( size_t i=0; i<i_n; i++ )
	{
		for( int j=0; j<i_nvertices[i]; j++ )
		{
			int next_j = j+1;
			if( next_j == i_nvertices[i] )
				next_j = 0;

			if( weights[current_vertex + j] != 0.0f &&
				weights[current_vertex + next_j] != 0.0f )
			{
				crease_indices.push_back( indices[current_vertex+j] );
				crease_indices.push_back( indices[current_vertex+next_j] );

				crease_sharpness.push_back( weights[current_vertex+j] );
			}
		}

		current_vertex += i_nvertices[i];
	}

	if( !crease_indices.empty() )
	{
		NSI::ArgumentList mesh_args;
		mesh_args.Add( NSI::Argument::New( "subdivision.creasevertices" )
			->SetType( NSITypeInteger )
			->SetCount( crease_indices.size() )
			->SetValuePointer( &crease_indices[0] ) );

		mesh_args.Add( NSI::Argument::New( "subdivision.creasesharpness" )
			->SetType( NSITypeFloat )
			->SetCount( crease_sharpness.size() )
			->SetValuePointer( &crease_sharpness[0] ) );

		nsi.SetAttribute( m_handle, mesh_args );
	}
}
