#include "polygonmesh.h"
#include "context.h"

#include <GA/GA_Names.h>
#include <GT/GT_PrimPolygonMesh.h>
#include <OBJ/OBJ_Node.h>
#include <nsi.hpp>

#include <type_traits>

polygonmesh::polygonmesh(
	const context& i_ctx,
	OBJ_Node *i_object,
	const GT_PrimitiveHandle &i_gt_primitive,
	bool is_subdivision )
:
	exporter( i_ctx, i_object, i_gt_primitive ),
	m_is_subdiv(is_subdivision)
{
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
		static_cast<const GT_PrimPolygonMesh *>(m_gt_primitive.get());

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

	/*
		Prepare the P.indices attribute which, for each vertex on each face,
		points to the "P" vertex.
	*/
	const GT_DataArrayHandle &vertex_list = polygon_mesh->getVertexList();
	GT_DataArrayHandle buffer_in_case_we_need_it;
	const int *vertices = vertex_list->getI32Array( buffer_in_case_we_need_it );

	mesh_args.Add( NSI::Argument::New( "P.indices" )
		->SetType( NSITypeInteger )
		->SetCount( vertex_list->entries() )
		->SetValuePointer( vertices ) );

	if( !m_is_subdiv )
	{
		mesh_args.Add( NSI::Argument::New( "N.indices" )
			->SetType( NSITypeInteger )
			->SetCount( vertex_list->entries() )
			->SetValuePointer( vertices ) );
	}

	m_nsi.SetAttribute( m_handle.c_str(), mesh_args );

	/* output the UVs only once */
	GT_AttributeListHandle attributes[] =
	{
		polygon_mesh->getShared(),
		polygon_mesh->getVertex(),
		polygon_mesh->getUniform()
	};
	std::vector<const char *> to_export{ "uv" };
	exporter::export_attributes(
		attributes,
		sizeof(attributes)/sizeof(attributes[0]),
		m_context.m_start_time,
		to_export );
}

/**
	\brief output P, N

	\ref http://www.sidefx.com/docs/hdk/_g_t___prim_polygon_mesh_8h_source.html
*/
void polygonmesh::set_attributes_at_time( double i_time ) const
{
	const GT_PrimPolygonMesh *polygon_mesh =
		static_cast<const GT_PrimPolygonMesh *>(m_gt_primitive.get());

	const GT_PrimPolygonMesh *with_normals =  nullptr;
	if( !m_is_subdiv )
	{
		with_normals = polygon_mesh->createPointNormalsIfMissing();
	}

	if( with_normals != nullptr )
		polygon_mesh = with_normals;

	GT_AttributeListHandle attributes[] =
	{
		polygon_mesh->getShared(),
		polygon_mesh->getVertex(),
		polygon_mesh->getUniform()
	};

	std::vector<const char *> to_export{ "P" };
	if( !m_is_subdiv )
		to_export.push_back( "N" );

	exporter::export_attributes(
		attributes,
		sizeof(attributes)/sizeof(attributes[0]),
		i_time,
		to_export );

	delete with_normals;
}
