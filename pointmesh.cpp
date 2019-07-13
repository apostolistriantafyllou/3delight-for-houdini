#include "pointmesh.h"

#include "context.h"

#include <GT/GT_PrimPointMesh.h>
#include <OBJ/OBJ_Node.h>
#include <nsi.hpp>

pointmesh::pointmesh(
	const context& i_ctx,
	OBJ_Node *i_object,
	const GT_PrimitiveHandle &i_gt_primitive )
:
	exporter( i_ctx, i_object, i_gt_primitive )
{
}

void pointmesh::create( void ) const
{
	m_nsi.Create( m_handle.c_str(), "particles" );
}

void pointmesh::set_attributes( void ) const
{
     const GT_PrimPointMesh *pointmesh =
	    static_cast<const GT_PrimPointMesh *>(m_gt_primitive.get());

	 pointmesh->dumpAttributeLists( m_handle.c_str(), true );
}

void pointmesh::set_attributes_at_time( double ) const
{
}
