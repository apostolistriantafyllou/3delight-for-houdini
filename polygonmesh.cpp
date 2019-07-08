#include "polygonmesh.h"

#include "context.h"

#include <GT/GT_PrimPolygonMesh.h>
#include <OBJ/OBJ_Node.h>
#include <nsi.hpp>

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

void polygonmesh::set_attributes( void ) const
{
     const GT_PrimPolygonMesh *polygon_mesh =
	    static_cast<const GT_PrimPolygonMesh *>(m_gt_primitive.get());
}

void polygonmesh::set_attributes_at_time( double ) const
{
     const GT_PrimPolygonMesh *polygon_mesh =
	    static_cast<const GT_PrimPolygonMesh *>(m_gt_primitive.get());

	 const GT_DataArrayHandle &P = polygon_mesh->getShared()->get("P");
}
