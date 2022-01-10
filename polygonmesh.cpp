#include "polygonmesh.h"

#include "context.h"
#include "time_sampler.h"

#include <GA/GA_Names.h>
#include <GT/GT_PrimPolygonMesh.h>
#include <OBJ/OBJ_Node.h>
#include <nsi.hpp>

#include <map>
#include <tuple>
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
		i_primitive_index ),
	m_is_subdiv(i_force_subdivision)
{
	if(!m_is_subdiv)
	{
		const char *k_subdiv = "_3dl_render_poly_as_subd";
		m_is_subdiv =
			m_object->hasParm(k_subdiv) &&
			m_object->evalInt(k_subdiv, 0, m_context.m_current_time) != 0;

		/* Also check for SOP-level detail special symbol for subdivs. */
		GT_Owner type;
		auto flag = i_gt_primitive->findAttribute(
			"_3dl_render_poly_as_subd", type, 0 );
		if( flag )
		{
			m_is_subdiv = flag->getI32( 0 );
		}
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
		generate_uv_connectivity(*polygon_mesh, mesh_args);
	}

	// Retrieve a context that might redirect the attributes to a shared file
	NSI::Context& nsi = attributes_context();
	// Those attributes may already have been exported in a previous frame
	if(nsi.Handle() != NSI_BAD_CONTEXT)
	{
		nsi.SetAttribute( m_handle, mesh_args );
	}

	const GT_DataArrayHandle& vertices_list = polygon_mesh->getVertexList();

	std::vector< std::string > to_export{ "uv" };
	exporter::export_attributes(
		to_export, *polygon_mesh, m_context.m_current_time, vertices_list );

	export_creases(
		polygon_mesh->getVertexList(),
		nvertices.get(),
		count_array.entries() );

	/*
		Export rest attributes. Note that we don't output them per time-sample
		as this goes against the logic of rest attibutes.
	*/
	std::vector< std::string > rest{ "rest" };
	rest.push_back( "rnml" );
	exporter::export_attributes(
		rest, *polygon_mesh,
		m_context.ShutterOpen(), polygon_mesh->getVertexList() );

	/*
		Now export actual geo (P,N). This could be either interpolated ("v") or
		sampled.
	*/
	if( has_velocity_blur() )
	{
		export_extrapolated_P(polygon_mesh->getVertexList());
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
	else
	{
		// Export attributes at each time sample
		primitive::set_attributes();
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

	std::vector< std::string > to_export{ "P" };
	if( !m_is_subdiv )
	{
		to_export.push_back( "N" );
	}

	exporter::export_attributes(
		to_export, *polygon_mesh, i_time, polygon_mesh->getVertexList() );
}

void polygonmesh::connect( void ) const
{
	primitive::connect();
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
	NSI::Context& nsi = attributes_context();
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

/**
	Generate connectivity for uv coordinates which is needed for proper
	interpolation on subdivision surfaces. Houdini does not track this natively
	so we have to make it up from the values.

	Note that "uv" was exported as "st" by exporter::export_attributes.

	For now, to keep the code simple, we don't bother writing a shorter version
	of that attribute without the duplicate values.
*/
void polygonmesh::generate_uv_connectivity(
	const GT_Primitive &i_primitive,
	NSI::ArgumentList &io_mesh_args) const
{
	GT_Owner owner;
	GT_DataArrayHandle data = i_primitive.findAttribute("uv", owner, 0);
	if( !data || !data->entries() || owner != GT_OWNER_VERTEX )
		return;
	if( data->getTypeInfo() != GT_TYPE_TEXTURE )
		return;
	GT_Size n = data->entries();
	NSI::Argument *index_arg = new NSI::Argument("st.indices");
	index_arg->SetCount(n);
	index_arg->SetType(NSITypeInteger);
	unsigned *index_buffer = reinterpret_cast<unsigned*>(
		index_arg->AllocValue(sizeof(unsigned) * n));
	io_mesh_args.Add(index_arg);

	GT_DataArrayHandle buffer;
	const float *uv_data = data->getF32Array(buffer);
	typedef std::tuple<float, float, float> uv_t;
	std::map<uv_t, unsigned> uv_map;
	for( GT_Size i = 0; i < n; ++i )
	{
		index_buffer[i] = uv_map.emplace(
			uv_t{uv_data[i*3+0], uv_data[i*3+1], uv_data[i*3+2]}, unsigned(i))
			.first->second;
	}
}
