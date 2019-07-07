
#include "light.h"

#include <OBJ/OBJ_Node.h>

#include <GT/GT_Handles.h>

#include <assert.h>
#include <nsi.hpp>
#include <iostream>

light::light(
	NSI::Context &i_nsi,
	OBJ_Node *i_object,
	const GT_PrimitiveHandle &i_gt_primitive )
:
	exporter( i_nsi, i_object, i_gt_primitive )
{
	m_handle = i_object->getFullPath();
	m_handle += "|light";
}

void light::create( void ) const
{
}

void light::set_attributes( void ) const
{
}

void light::set_attributes_at_time( double i_time ) const
{
	return;
}
