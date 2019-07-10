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
	m_handle = i_object->getFullPath();

	if( is_subdivision )
		m_handle += "|subdivision";
	else
		m_handle += "|polygonmesh";
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
}

/**
	\brief output P, N and all the attributes for this polygon mesh.
*/
void polygonmesh::set_attributes_at_time( double ) const
{
     const GT_PrimPolygonMesh *polygon_mesh =
	    static_cast<const GT_PrimPolygonMesh *>(m_gt_primitive.get());

	 const GT_DataArrayHandle &P = polygon_mesh->getShared()->get( GA_Names::P );

	 NSI::ArgumentList mesh_args;

	 GT_DataArrayHandle buffer_in_case_we_need_it;
	 mesh_args.Add( NSI::Argument::New( "P" )
		 ->SetType( NSITypePoint )
		 ->SetCount( P->entries() )
		 ->SetValuePointer( P->getF32Array(buffer_in_case_we_need_it) ) );

	std::unique_ptr<UT_Vector3> N;
	static_assert( sizeof(UT_Vector3) == sizeof(float)*3,
		"UT_Vector3 is not of the expected size/type" );

	if( !m_is_subdiv )
	{
		N = std::unique_ptr<UT_Vector3>(new UT_Vector3[P->entries()]);
		polygon_mesh->pointNormals( N.get(), P->entries() );

		mesh_args.Add( NSI::Argument::New( "N" )
			->SetType( NSITypeNormal )
			->SetCount( P->entries() )
			->SetValuePointer(N.get()) );
	}

	m_nsi.SetAttribute( m_handle.c_str(), mesh_args );
}
