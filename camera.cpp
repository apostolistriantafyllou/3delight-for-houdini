#include "camera.h"

#include "context.h"
#include "shader_library.h"
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

	/// Returns the field of view for a perspective camera.
	float get_fov(OBJ_Camera& i_camera, double i_time)
	{
		if(!i_camera.hasParm("_3dl_fov"))
		{
			return 30.0f;
		}

		return i_camera.evalFloat("_3dl_fov", 0, i_time);
	}

	/// Returns true if depth-of-field must be enabled
	bool get_dof(const context& i_context, OBJ_Camera& i_camera, double i_time)
	{
		if(!i_context.m_dof)
		{
			return false;
		}

		if(!i_camera.hasParm("_3dl_enable_dof"))
		{
			return i_context.m_dof;
		}

		return i_camera.evalInt("_3dl_enable_dof", 0, i_time) != 0;
	}

	const char* k_aux_fov = "_3dl_projection_aux_fov";

	/// Returns the auxiliary FOV used as horizontal FOV for cylindrical cameras
	double get_auxiliary_fov(OBJ_Camera& i_camera, double i_time)
	{
		if(!i_camera.hasParm(k_aux_fov))
		{
			return 180.0;
		}

		return i_camera.evalFloat(k_aux_fov, 0, i_time);
	}

	const char* k_shutter_efficiency = "_3dl_shutter_efficiency";
	const char* k_shutter_center = "_3dl_shutter_center";

	/**
		\brief Computes NSI's "shutteropening" attribute from 3DFH's attributes.

		Instead of the "opening efficiency" and "closing" efficiency used in
		other 3Delight plugins, we use a simpler to use system that presents 2
		completely independent parameters to the user : an "efficiency" that
		represents the portion of the "shutterrange" during which the shutter is
		completely open, and a "center" which skews the opening function left
		(earlier) or right (later).

		Allowing the user to specify NSI's "shutteropening" attribute directly
		would also have involved an awkward UI since the 2 values are
		interdependent.

		This system has the advantage of being able to specify both the opening
		and closing rates using a single parameter, which is all is going to be
		needed for the majority of users.  Any valid trapezoid shape can still
		be specified by tweaking the "center" parameter.
	*/
	void get_fully_open_shutter_range(
		OBJ_Camera& i_camera,
		double i_time,
		double* o_range)
	{
		if(!i_camera.hasParm(k_shutter_efficiency))
		{
			o_range[0] = 1.0/3.0;
			o_range[1] = 2.0/3.0;
			return;
		}

		double efficiency = i_camera.evalFloat(k_shutter_efficiency, 0, i_time);
		double center = i_camera.evalFloat(k_shutter_center, 0, i_time);

		/*
			Now, convert efficiency/center into normalized "shutteropening"
			range.

			In the diagram below (an asymmetrical trapezoid) that represents
			the fraction of shutter opening over normalized time :

				shutteropening = a..b
				center = C
				efficiency = b-a
				(C-a)/(b-a) = C  (in this case C > 0.5)

			 0     a_____C___b   1
			      /           \
			    /              \
			  /_________________\

			When "center" is 0.5, the shape is symmmetrical. Otherwise it's
			skewed towards the left (< 0.5) or skewed towards the right (> 0.5).
			When "efficiency" is 1, the trapezoid is also a rectangle.
			When "efficiency" is 0, the trapezoid has a 0-length top side and
			becomes a triangle.

			"Center" is kind of a misnomer, actually, since it's not equal to
			(a+b)/2. It's more of a "skew" factor.
		*/

		double lag = 1.0 - efficiency;

		// Interpolate between 0 and center with ratio "1-efficiency"
		o_range[0] = lag * center;

		/*
			Interpolate between center and 1 with ratio "efficiency".
			This is slightly more complicated because we want to avoid numerical
			imprecisions so we are sure that an efficiency of 0 will yield
			exactly "center", an efficiency of 1 will yield exactly 1 and
			o_range[0] <= o_range[1].
		*/
		o_range[1] = lag * center + efficiency * 1.0;
	}

	const char* k_projection = "_3dl_projection_type";
	const std::string k_default_camera_type = "perspectivecamera";

	/// Retrieves the NSI camera node type and fisheye mapping from the camera
	void get_projection(
		OBJ_Camera& i_camera,
		double i_time,
		std::string& o_type,
		std::string& o_mapping)
	{
		if(!i_camera.hasParm(k_projection))
		{
			o_type = k_default_camera_type;
			o_mapping.clear();
			return;
		}

		// Retrieve the type/mapping string
		UT_String projection;
		i_camera.evalString(projection, k_projection, 0, i_time);
		std::string std_projection = projection.toStdString();

		// Split the string at the slash character
		size_t split = std_projection.find('/');
		o_type = std_projection.substr(0, split);
		o_mapping =
			split == std::string::npos
			?	std::string()
			:	std_projection.substr(split+1);
	}

	const char* k_distortion_type = "_3dl_distortion_type";
	const char* k_distortion_intensity = "_3dl_distortion_intensity";

	/// Returns the value of the distortion shader's "k2" parameter
	float get_distortion_k2(OBJ_Camera& i_camera, double i_time)
	{
		if(!i_camera.hasParm(k_distortion_type))
		{
			return 0.0f;
		}

		float intensity = i_camera.evalFloat(k_distortion_intensity, 0, i_time);
		intensity *= 0.2f;

		UT_String type;
		i_camera.evalString(type, k_distortion_type, 0, i_time);
		if(type == "pincushion")
		{
			intensity *= -1.0f;
		}
		else if(type != "barrel")
		{
			return 0.0f;
		}

		return intensity;
	}

	const char* k_enable_blades = "_3dl_enable_aperture_blades";
	const char* k_nb_blades = "_3dl_aperture_blades";
	const char* k_blades_orientation = "_3dl_aperture_blades_rotation";

	/// Retrieves the aperture blades configuration
	bool get_aperture_blades(
		OBJ_Camera& i_camera,
		double i_time,
		int& o_number,
		double& o_angle)
	{
		if(!i_camera.hasParm(k_enable_blades))
		{
			o_number = 5;
			o_angle = 0.0f;
			return false;
		}

		bool enable = i_camera.evalInt(k_enable_blades, 0, i_time) != 0;
		o_number = i_camera.evalInt(k_nb_blades, 0, i_time);
		o_angle = i_camera.evalFloat(k_blades_orientation, 0, i_time);

		return enable;
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
	OBJ_Camera* cam = m_object->castToOBJCamera();
	assert(cam);
	get_projection(*cam, i_ctx.m_current_time, m_type, m_mapping);
}

void camera::create( void ) const
{
	m_nsi.Create(m_handle, m_type);

	m_nsi.Create(distortion_shader_handle(), "shader");

	const shader_library& shaders = shader_library::get_instance();
	std::string distortion_shader = shaders.get_shader_path("dlLensDistortion");
	m_nsi.SetAttribute(
		distortion_shader_handle(),
		(
			NSI::StringArg("shaderfilename", distortion_shader),
			NSI::FloatArg("k1", 0.0f),
			NSI::FloatArg("k3", 0.0f)
		) );
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

	OBJ_Camera* cam = m_object->castToOBJCamera();
	assert(cam);

	double shutter_opening[2];
	get_fully_open_shutter_range(
		*cam,
		m_context.m_current_time,
		shutter_opening);
	m_nsi.SetAttribute(
		m_handle,
		*NSI::Argument("shutteropening")
			.SetType(NSITypeDouble)
			->SetCount(2)
			->CopyValue(shutter_opening, sizeof(shutter_opening)));

	if(m_type == "fisheyecamera")
	{
		m_nsi.SetAttribute(m_handle, NSI::StringArg("mapping", m_mapping));
	}

	if(get_distortion_k2(*cam, m_context.m_current_time) == 0.0f)
	{
		m_nsi.Disconnect(distortion_shader_handle(), "", m_handle, "lensshader");
	}
	else
	{
		m_nsi.Connect(distortion_shader_handle(), "", m_handle, "lensshader");
	}

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

	m_nsi.SetAttributeAtTime(
		m_handle,
		i_time,
		*NSI::Argument("clippingdistance")
			.SetType(NSITypeDouble)
			->SetCount(2)
			->CopyValue(nsi_clip, sizeof(nsi_clip)));

	if(m_type == "perspectivecamera")
	{
		int nb_blades = 0;
		double blades_angle = 0.0f;
		bool enable_blades =
			get_aperture_blades(*cam, i_time, nb_blades, blades_angle);

		/*
			We use SetAttributeAtTime even though 3Delight doesn't support
			motion-blurring those attributes, yet.
		*/
		m_nsi.SetAttributeAtTime(
			m_handle,
			i_time,
			(
				NSI::FloatArg("fov", get_fov(*cam, i_time)),
				NSI::IntegerArg("depthoffield.enable", get_dof(m_context, *cam, i_time)),
				NSI::DoubleArg("depthoffield.fstop", cam->FSTOP(i_time)),
				NSI::DoubleArg("depthoffield.focallength", get_focal_length(*cam, i_time)),
				NSI::DoubleArg("depthoffield.focaldistance", cam->FOCUS(i_time)),
				NSI::IntegerArg("depthoffield.aperture.enable", (int)enable_blades),
				NSI::IntegerArg("depthoffield.aperture.sides", nb_blades),
				NSI::DoubleArg("depthoffield.aperture.angle", blades_angle)
			) );
	}
	else if(m_type == "fisheyecamera")
	{
		m_nsi.SetAttributeAtTime(
			m_handle,
			i_time,
			NSI::FloatArg("fov", get_fov(*cam, i_time)));
	}
	else if(m_type == "cylindricalcamera")
	{
		m_nsi.SetAttributeAtTime(
			m_handle,
			i_time,
			(
				NSI::FloatArg("fov", get_fov(*cam, i_time)),
				NSI::FloatArg("horizontalfov", get_auxiliary_fov(*cam, i_time))
			) );
	}

	m_nsi.SetAttributeAtTime(
		distortion_shader_handle(),
		i_time,
		NSI::FloatArg("k2", get_distortion_k2(*cam, i_time)));
}

std::string
camera::get_nsi_handle(OBJ_Node& i_camera)
{
	return std::string(i_camera.getFullPath()) + "|camera";
}

double
camera::get_shutter_duration(OBJ_Camera& i_camera, double i_time)
{
	const char* k_shutter_duration = "_3dl_shutter_duration";

	if(i_camera.hasParm(k_shutter_duration))
	{
		return i_camera.evalFloat(k_shutter_duration, 0, i_time);
	}

	return i_camera.SHUTTER(i_time);
}

bool camera::get_ortho_screen_window(
	double* o_screen_window,
	OBJ_Camera& i_camera,
	double i_time)
{
	std::string type;
	std::string mapping;
	get_projection(i_camera, i_time, type, mapping);
	if(type != "orthographiccamera")
	{
		return false;
	}

	double half_width = i_camera.ORTHOW(i_time) / 2.0f;
	double half_height =
		half_width * i_camera.RESY(i_time) / i_camera.RESX(i_time);

	o_screen_window[0] = -half_width;
	o_screen_window[1] = -half_height;
	o_screen_window[2] = half_width;
	o_screen_window[3] = half_height;

	return true;
}
