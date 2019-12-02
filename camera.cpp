#include "camera.h"

#include "context.h"
#include "time_sampler.h"

#include <OBJ/OBJ_Node.h>
#include <OBJ/OBJ_Camera.h>

#include <nsi.hpp>

namespace
{
	/**
		\brief Returns the focal length of the camera, in scene units.

		It uses the "focalunits" string (!) attribute to choose a scaling factor
		to apply to the focal length returned by OBJ_Camera::FOCAL.

		This system seems to assume that scene units are meters, even though
		there is a global scene scale factor in Edit > Preferences > Hip File
		Options named "Unit Length". It should probably play a role in the
		computation, but since it doesn't seem to be taken into account by
		Mantra, we'll also ignore it for the moment.
	*/
	double get_focal_length(OBJ_Camera& i_camera, double i_time)
	{
		double scale = 1.0;

		/*
			light sources, which are also cameras in Houdini, don't have this
			parameter.
		*/
		if(i_camera.hasParm("focalunits"))
		{
			UT_String focalunits;
			i_camera.evalString(focalunits, "focalunits", 0, i_time);
			if(focalunits == "mm")
			{
				scale = 0.001;
			}
			else if(focalunits == "m")
			{
				scale = 1.0;
			}
			else if(focalunits == "nm")
			{
				scale = 0.000001;
			}
			else if(focalunits == "in")
			{
				scale = 0.0254;
			}
			else if(focalunits == "ft")
			{
				scale = 0.3048;
			}
		}

		return scale * i_camera.FOCAL(i_time);
	}
}

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
	double nsi_shutter[2] = { m_context.ShutterOpen(), m_context.ShutterClose() };
	m_nsi.SetAttribute(
		m_handle,
		*NSI::Argument("shutterrange")
			.SetType(NSITypeDouble)
			->SetCount(2)
			->CopyValue(nsi_shutter, sizeof(nsi_shutter)));

	for(time_sampler t(m_context, *m_object, time_sampler::e_deformation); t; t++)
	{
		set_attributes_at_time(*t);
	}
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
*/
void camera::set_attributes_at_time( double i_time ) const
{
	OBJ_Camera* cam = m_object->castToOBJCamera();
	assert(cam);
	if(!cam)
	{
		return;
	}

	double nsi_clip[2] = { cam->getNEAR(i_time), cam->getFAR(i_time) };

	/* compute horizontal field of view */
	float V = 2.0f * atan2f( cam->APERTURE(i_time) * 0.5, cam->FOCAL(i_time));
	double ratio = cam->RESX(i_time) / double(cam->RESY(i_time));
	float horizontal_fov = 2.0 * ::atan( ::tanf(V*0.5f) * 1/ratio );
	horizontal_fov *=  360.f / (2.0 * M_PI); // to degrees

	m_nsi.SetAttributeAtTime(
		m_handle,
		i_time,
		*NSI::Argument("clippingdistance")
			.SetType(NSITypeDouble)
			->SetCount(2)
			->CopyValue(nsi_clip, sizeof(nsi_clip)));

	if(cam->PROJECTION(0.0f) == OBJ_PROJ_PERSPECTIVE)
	{
		bool dof = m_context.m_dof;
		if(cam->hasParm("_3dl_enable_dof"))
		{
			dof = dof && cam->evalInt("_3dl_enable_dof", 0, i_time) != 0;
		}

		/*
			We use SetAttributeAtTime even though 3Delight doesn't support
			motion-blurring those attributes, yet.
		*/
		m_nsi.SetAttributeAtTime(
			m_handle,
			i_time,
			(
				NSI::FloatArg("fov",  horizontal_fov),
				NSI::IntegerArg("depthoffield.enable", dof),
				NSI::DoubleArg("depthoffield.fstop", cam->FSTOP(i_time)),
				NSI::DoubleArg("depthoffield.focallength", get_focal_length(*cam, i_time)),
				NSI::DoubleArg("depthoffield.focaldistance", cam->FOCUS(i_time))
			) );
	}
}

std::string
camera::get_nsi_handle(OBJ_Node& i_camera)
{
	return std::string(i_camera.getFullPath()) + "|camera";
}

double
camera::get_shutter_duration(OBJ_Camera& i_camera, double i_time)
{
	return i_camera.SHUTTER(i_time);
}
