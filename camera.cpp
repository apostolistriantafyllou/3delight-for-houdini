#include "camera.h"

#include "context.h"
#include "null.h"
#include "shader_library.h"
#include "time_sampler.h"

/*
	Bad include here. But we need it to acces camera realated paramaeters
	that are inside the ROP (oversampling, etc)
*/
#include "ROP_3Delight.h"

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

	const char* k_fov = "_3dl_fov";

	/**
		\brief Returns the field of view for a perspective camera.

		NOTE: this takes the pixel aspect ratio ('apsect') parameter into
		account.
	*/
	float get_fov(OBJ_Camera& i_camera, double i_time)
	{
		if(!i_camera.hasParm(k_fov))
		{
			return 30.0f;
		}

		float fov = i_camera.evalFloat(k_fov, 0, i_time);
		return fov / i_camera.ASPECT( i_time );
	}

	const char* k_focal_distance = "_3dl_focal_distance";

	/// Returns the focal distance for a perspective camera.
	double get_focal_distance(OBJ_Camera& i_camera, double i_time)
	{
		if(!i_camera.hasParm(k_focal_distance))
		{
			return i_camera.FOCUS(i_time);
		}

		return i_camera.evalFloat(k_focal_distance, 0, i_time);
	}

	const char* k_fstop = "_3dl_fstop";

	/// Returns the relative aperture (f-stop) for a perspective camera.
	double get_fstop(OBJ_Camera& i_camera, double i_time)
	{
		if(!i_camera.hasParm(k_fstop))
		{
			return i_camera.FSTOP(i_time);
		}

		return i_camera.evalFloat(k_fstop, 0, i_time);
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
//	const char* k_shutter_center = "_3dl_shutter_center";

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

		LATEST NEWS : For now, we don't present the "center" parameter to the
		user. When one of them asks for it, we might also get an indication for
		a better name.
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
		double center = 0.5;//i_camera.evalFloat(k_shutter_center, 0, i_time);

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
	m_handle = handle(*m_object, i_ctx);
	OBJ_Camera* cam = m_object->castToOBJCamera();
	assert(cam);
	get_projection(*cam, i_ctx.m_current_time, m_type, m_mapping);
}

void camera::create( void ) const
{
	OBJ_Camera* cam = m_object->castToOBJCamera();
	if( !cam )
	{
		assert( false );
		return;
	}

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

	m_nsi.Create( screen_handle(), "screen");

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

	OBJ_CameraParms camera_parameters;
	cam->getCameraParms( camera_parameters, m_context.m_current_time );
	double clipping_range[2] =
	{
		camera_parameters.mynear,
		camera_parameters.myfar
	};
	m_nsi.SetAttribute(
		m_handle,
		*NSI::Argument("clippingrange")
			.SetType(NSITypeDouble)
			->SetCount(2)
			->CopyValue(clipping_range, sizeof(clipping_range)));

	fpreal t = m_context.m_current_time;
	float scale = m_context.m_rop->GetResolutionFactor();
	int default_resolution[2] =
	{
		int(::roundf(cam->RESX(t)*scale)),
		int(::roundf(cam->RESY(t)*scale))
	};

	m_nsi.SetAttribute(
		screen_handle(),
		(
			*NSI::Argument::New("resolution")
			->SetArrayType(NSITypeInteger, 2)
			->SetCount(1)
			->CopyValue(default_resolution, sizeof(default_resolution)),
			NSI::IntegerArg("oversampling", m_context.m_rop->GetPixelSamples()),
			NSI::FloatArg( "pixelaspectratio", cam->ASPECT(t))
		) );

#if 0
	if( m_idisplay_rendering )
	{
		m_nsi.SetAttribute(
			screen_handle(),
			*NSI::Argument::New("crop")
				->SetArrayType(NSITypeFloat, 2)
				->SetCount(2)
				->SetValuePointer(m_idisplay_rendering_window));
	}
	else
#endif
	{
		float cam_crop[4] =
		{
			float(cam->CROPL(0)), 1.0f - float(cam->CROPT(0)),
			float(cam->CROPR(0)), 1.0f - float(cam->CROPB(0))
		};

		m_nsi.SetAttribute(
			screen_handle(),
			*NSI::Argument::New("crop")
				->SetArrayType(NSITypeFloat, 2)
				->SetCount(2)
				->SetValuePointer(cam_crop));
	}

	/*
		If the camera is not orthographic, use the default screen window.
		Otherwise, define it so it fits the camera's "ortho width" parameter.
	*/
	double sw[4];
	get_screen_window(sw, *cam, m_context.m_current_time);

	m_nsi.SetAttribute(
		screen_handle(),
		*NSI::Argument::New("screenwindow")
		->SetArrayType(NSITypeDouble, 2)
		->SetCount(2)
		->SetValuePointer(sw));

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

/**
	Connect camera to transform and the screen to the camera.
*/
void camera::connect( void ) const
{
	OBJ_Camera* cam = m_object->castToOBJCamera();
	assert(cam);

	m_nsi.Connect(
		m_handle, "",
		null::handle(*m_object, m_context), "objects" );

	m_nsi.Connect(
		screen_handle(), "",
		camera::handle(*cam, m_context), "screens");
}

void camera::changed_cb(
	OP_Node* i_caller,
	void* i_callee,
	OP_EventType i_type,
	void* i_data)
{
	context* ctx = (context*)i_callee;
	OBJ_Node* obj = i_caller->castToOBJNode();
	assert(obj);
	
	if(i_type == OP_NODE_PREDELETE)
	{
		/*
			Avoid deleting lights from the camera exporter. It won't cause any
			actual problem, but NSI will complain that the node doesn't exist
			(because the light exporter has already taken care of deleting it).
		*/
		if(!obj->castToOBJLight())
		{
			Delete(*obj, *ctx);
		}
		return;
	}

	if(i_type != OP_PARM_CHANGED)
	{
		return;
	}

	intptr_t parm_index = reinterpret_cast<intptr_t>(i_data);

	if(null::is_transform_parameter_index(parm_index))
	{
		return;
	}

	camera node(*ctx, obj);

	// Simply re-export all attributes.  It's not that expensive.
	node.set_attributes();
	ctx->m_nsi.RenderControl(NSI::CStringPArg("action", "synchronize"));
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

		double squeeze = 1;
		if( cam->getParmIndex("_3dl_lense_squeeze") != -1 )
		{
			squeeze = cam->evalFloat("_3dl_lense_squeeze", 0, i_time );
		}

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
				NSI::DoubleArg("depthoffield.fstop", get_fstop(*cam, i_time)),
				NSI::DoubleArg("depthoffield.focallength", get_focal_length(*cam, i_time)),
				NSI::DoubleArg("depthoffield.focaldistance", get_focal_distance(*cam, i_time)),
				NSI::IntegerArg("depthoffield.aperture.enable", (int)enable_blades),
				NSI::IntegerArg("depthoffield.aperture.sides", nb_blades),
				NSI::DoubleArg("depthoffield.aperture.angle", blades_angle),
				NSI::DoubleArg("depthoffield.focallengthratio", squeeze )
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
camera::handle(OBJ_Node& i_camera, const context& i_ctx)
{
	return exporter::handle(i_camera, i_ctx) + "|camera";
}

void camera::Delete(OBJ_Node& i_node, const context& i_context)
{
	i_context.m_nsi.Delete(handle(i_node, i_context));
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

void camera::get_screen_window(
	double* o_screen_window,
	OBJ_Camera& i_camera,
	double i_time,
	bool i_use_houdini_projection)
{
	std::string type;
	std::string mapping;
	get_projection(i_camera, i_time, type, mapping);

	if (i_use_houdini_projection)
	{
		/*
			Evaluate projection type from Houdini's native parameter.
			For orthographic camera it's parameter name is "ortho".
		*/
		UT_String projection;
		i_camera.evalString(projection, "projection", 0, i_time);
		type = projection.toStdString();
	}

	if(type == "orthographiccamera" || type == "ortho")
	{
		double half_width = i_camera.ORTHOW(i_time) / 2.0f;
		double half_height =
			half_width * i_camera.RESY(i_time) / i_camera.RESX(i_time);

		o_screen_window[0] = -half_width;
		o_screen_window[1] = -half_height;
		o_screen_window[2] = half_width;
		o_screen_window[3] = half_height;
	}
	else
	{
		double A = i_camera.RESX(i_time) / double(i_camera.RESY(i_time));

		float screen_window_x = i_camera.evalFloat( "win", 0, i_time );
		float screen_window_y = i_camera.evalFloat( "win", 1, i_time );

		float screen_window_size_x = i_camera.evalFloat( "winsize", 0, i_time );
		float screen_window_size_y = i_camera.evalFloat( "winsize", 1, i_time );

		o_screen_window[0] = -A * screen_window_size_x + 2 * screen_window_x*A;
		o_screen_window[1] = -1.f * screen_window_size_y + 2 * screen_window_y;
		o_screen_window[2] = A * screen_window_size_x + 2 * screen_window_x*A;
		o_screen_window[3] = 1.f * screen_window_size_y + 2 * screen_window_y;
	}
}

std::string camera::screen_handle( void ) const
{
	return screen_handle( m_object, m_context );
}

std::string camera::screen_handle( OBJ_Node *i_cam, const context &i_context )
{
	return handle(*i_cam, i_context ) + "|screen";
}