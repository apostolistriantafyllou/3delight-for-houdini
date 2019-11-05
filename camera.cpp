#include "camera.h"

#include "context.h"

#include <OBJ/OBJ_Node.h>
#include <OBJ/OBJ_Camera.h>

#include <nsi.hpp>

camera::camera(
	const context& i_ctx,
	OBJ_Node *i_object )
:
	exporter( i_ctx, i_object )
{
	assert(m_object);
	m_handle = get_nsi_handle(*m_object);
}

void camera::create( void ) const
{
	m_nsi.Create( m_handle.c_str(), "perspectivecamera" );
}

void camera::set_attributes( void ) const
{
	set_attributes_at_time(m_context.m_current_time);
}

void camera::connect( void ) const
{
	std::string parent = m_object->getFullPath().c_str();
	m_nsi.Connect( m_handle, "", parent, "objects" );
}

/**
	\brief Sets the field of view, clipping planes and depth of field parameters
	of this camera.

	NOTE: NSI expects a horizontal field of view and the description we get from
	Houdini is for a vertical FOV.

	TODO:
	depth of field.
*/
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

	/* compute horizontal field of view */
	float V = 2.0f * atan2f( cam->APERTURE(i_time) * 0.5, cam->FOCAL(i_time));
	double ratio = cam->RESX(i_time) / double(cam->RESY(i_time));
	float horizontal_fov = 2.0 * ::atan( ::tanf(V*0.5f) * 1/ratio );
	horizontal_fov *=  360.f / (2.0 * M_PI); // to degrees

	m_nsi.SetAttribute(
		m_handle,
		(
			*NSI::Argument("shutterrange")
				.SetType(NSITypeDouble)
				->SetCount(2)
				->CopyValue(nsi_shutter, sizeof(nsi_shutter)),
			*NSI::Argument("clippingdistance")
				.SetType(NSITypeDouble)
				->SetCount(2)
				->CopyValue(nsi_clip, sizeof(nsi_clip))
		) );

	if(cam->PROJECTION(0.0f) == OBJ_PROJ_PERSPECTIVE)
	{
		m_nsi.SetAttribute(
			m_handle,
			(
				NSI::FloatArg("fov",  horizontal_fov),
				// FIXME : DOF is broken
				NSI::IntegerArg("depthoffield.enable", false),//m_context.m_dof),
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
