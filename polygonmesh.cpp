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
	unsigned i_primitive_index,
	bool is_subdivision )
:
	primitive( i_ctx, i_object, i_gt_primitive, i_primitive_index ),
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
			new NSI::StringArg("subdivision.scheme",
				"catmull-clark"));
	}

	m_nsi.SetAttribute( m_handle, mesh_args );

	std::vector< std::string > to_export{ "uv" };
	exporter::export_attributes(
		*polygon_mesh, m_context.m_current_time, to_export );

	exporter::export_bind_attributes( *polygon_mesh );

	primitive::set_attributes();
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
		to_export.push_back( "N" );

	exporter::export_attributes( *polygon_mesh, i_time, to_export );
}
