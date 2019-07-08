#include "camera.h"

#include "context.h"

#include <OBJ/OBJ_Node.h>
#include <OBJ/OBJ_Camera.h>

#include <nsi.hpp>

camera::camera(
	const context& i_ctx,
	OBJ_Node *i_object,
	const GT_PrimitiveHandle &i_gt_primitive )
:
	exporter( i_ctx, i_object, i_gt_primitive )
{
	m_handle = i_object->getFullPath();
	m_handle += "|camera";
}

void camera::create( void ) const
{
	m_nsi.Create( m_handle.c_str(), "perspectivecamera" );
}

void camera::set_attributes( void ) const
{
}

void camera::set_attributes_at_time( double ) const
{
}
