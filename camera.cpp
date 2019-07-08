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
	assert(i_object);
	m_handle = get_nsi_handle(*i_object);
}

void camera::create( void ) const
{
	m_nsi.Create( m_handle.c_str(), "perspectivecamera" );
}

void camera::set_attributes( void ) const
{
}

void camera::set_attributes_at_time( double i_time ) const
{
	OBJ_Camera* cam = m_object->castToOBJCamera();
	assert(cam);
	if(!cam)
	{
		return;
	}

	fpreal shutter = cam->SHUTTER(i_time) * m_context.m_frame_duration;
	double nsi_shutter[2] = { i_time - shutter/2.0, i_time + shutter/2.0 };

	double nsi_clip[2] = { cam->getNEAR(i_time), cam->getFAR(i_time) };

	m_nsi.SetAttribute(
		m_handle,
		(
			*NSI::Argument::New("shutterrange")
			->SetType(NSITypeDouble)
			->SetCount(2)
			->CopyValue(nsi_shutter, sizeof(nsi_shutter)),
			*NSI::Argument::New("clippingdistance")
			->SetType(NSITypeDouble)
			->SetCount(2)
			->CopyValue(nsi_clip, sizeof(nsi_clip))
		) );

	if(cam->PROJECTION(0.0f) == OBJ_PROJ_PERSPECTIVE)
	{
		m_nsi.SetAttribute(
			m_handle,
			(
				NSI::FloatArg("fov",  cam->APERTURE(i_time)),
				NSI::IntegerArg("depthoffield.enable", m_context.m_dof),
				NSI::DoubleArg("depthoffield.fstop", cam->FSTOP(i_time)),
				NSI::DoubleArg("depthoffield.focallength", cam->FOCAL(i_time)),
				NSI::DoubleArg("depthoffield.focaldistance", cam->FOCUS(i_time))
			) );
	}
}

std::string
camera::get_nsi_handle(OBJ_Node& i_camera)
{
	return std::string(i_camera.getFullPath()) + "|camera";
}
