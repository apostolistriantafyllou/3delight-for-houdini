
#include "mesh.h"

#include <nsi.hpp>

#include <iostream>
mesh::mesh(
	NSI::Context &i_nsi,
	OBJ_Node *i_object,
	const GT_PrimitiveHandle &i_gt_primitive )
:
	exporter( i_nsi, i_object, i_gt_primitive )
{
}

void mesh::create( void ) const
{
	m_nsi.Create( m_handle.c_str(), "mesh" );
}

void mesh::set_attributes( void ) const
{
}

void mesh::set_attributes_at_time( double ) const
{
}
