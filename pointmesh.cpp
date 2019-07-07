#include "pointmesh.h"

#include <GT/GT_PrimPointMesh.h>
#include <OBJ/OBJ_Node.h>
#include <nsi.hpp>

pointmesh::pointmesh(
	NSI::Context &i_nsi,
	OBJ_Node *i_object,
	const GT_PrimitiveHandle &i_gt_primitive )
:
	exporter( i_nsi, i_object, i_gt_primitive )
{
	m_handle = i_object->getFullPath();
	m_handle += "|pointmesh";
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
