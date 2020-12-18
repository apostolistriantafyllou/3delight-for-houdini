#include "viewport_hook.h"
#include "camera.h"

#include <DM/DM_VPortAgent.h>
#include <HOM/HOM_Module.h>
#include <OP/OP_Director.h>
#include <SYS/SYS_Version.h>
#include <OBJ/OBJ_Camera.h>

#include <ndspy.h>
#include <nsi_dynamic.hpp>

#include <atomic>
#include <chrono>
#include <thread>


namespace
{
	// 3Delight display driver name
	const char* k_driver_name = "3dfh_viewport";
	// Parameter name containing the viewport hook's id
	const char* k_viewport_hook_name = "viewport_hook_id";
	// 20 FPS
	const std::chrono::milliseconds k_refresh_interval(50);
}


/*
	Image buffer shared between the viewport hook and the display driver.

	It allows various difficult cases to be handled properly :
	- Closing the display driver before disconnecting the hook
	- Disconnecting the hook before the display driver is opened
	- Disconnecting the hook before the display driver is closed
	
	It is allocated by the viewport_hook, then its address is passed to the
	display driver through the "outputdriver" NSI node. Both the viewport_hook
	and the display driver can then disconnect independently and asynchronously
	from the buffer.

	The array of pixels remains valid until the object is destroyed. This avoids
	having to lock a mutex in order to write each incoming bucket (or in order
	to read the image when refreshing the viewport)..

	It also creates a thread which has the responsibilities of requesting
	refreshes of the render hook's viewport when the image has been updated, and
	of deleting the image buffer once it stops being referenced from both the
	display driver and the scene render hook.
*/
struct hook_image_buffer
{
	// Scene Render Hook interface

	/// Constructor : connects the render hook to the image buffer.
	hook_image_buffer(DM_SceneRenderHook* i_hook);

	/**
		\brief Disconnects image buffer from the Scene Render Hook.
		
		By calling this function, the render hook promises to stop using the
		buffer. It must be called only once.
	*/
	void disconnect();

	/// Returns the address of the pixels array
	UT_RGBA* pixels()const { return m_pixels; }
	/**
		Returns the number of bytes separating the beginning of two consecutive
		lines of pixels.
	*/
	size_t line_offset()const { return m_width*sizeof(UT_RGBA); }

	// Display driver interface

	/**
		\brief Allocates image data.
		
		This should be called by the display driver when it's reasy to start
		writing data to the buffer.

		\returns true if the buffer is ready to receive data, false otherwise
		(in which case it should not be referenced again).
	*/
	bool open(int i_width, int i_height);
	/// Adds a bucket to the image and requests a refresh of the viewport.
	void data(int i_x, int i_y, int i_w, int i_h, const void* i_data);
	/**
		\brief Notifies the image buffer that the display driver is being closed.

		It also clears the pixels so no image is displayed, but the render hook
		can continue to access them without synchronization.

		By calling this function, the display driver promises to stop writing
		data to the buffer. It must be called only once.
	*/
	void close();

	/// Returns true if the image buffer is still connected to a render hook.
	bool connected()const { return m_hook != nullptr; }

	unsigned m_width{0};
	unsigned m_height{0};

private:

	/// Destructor. Called by disconnect_once only.
	~hook_image_buffer();

	/**
		\brief Makes all pixels transparent.

		This makes the image disappear from the viewport without having to free
		the pixels memory when they're not needed anymore : since it could still
		be in use by the render hook, it would require synchronization at each
		access.
	*/
	void clear_pixels();

	/**
		\brief Updates the viewport at regular intervals.
		
		This function returns when the reference count becomes 0, so it's meant
		to be called from a separate thread.
	*/
	void update_viewport_loop();
	
	/*
		Pointer to the Scene Render Hook that needs to be refreshed when the
		image is updated.
	*/
	DM_SceneRenderHook* m_hook;

	// Controls access to m_hook
	std::mutex m_mutex;

	// Memory holding pixels data
	std::atomic<UT_RGBA*> m_pixels{nullptr};

	// Update count, incremented each time data() is called
	std::atomic<unsigned> m_timestamp{0};
	/*
		Reference count, used to determine when to stop the update thread and
		delete the object.
	*/
	std::atomic<unsigned> m_reference_count{1};
};


hook_image_buffer::hook_image_buffer(DM_SceneRenderHook* i_hook)
	:	m_hook(i_hook)
{
}


void
hook_image_buffer::disconnect()
{
	m_mutex.lock();
	m_hook = nullptr;
	m_mutex.unlock();

	/*
		Decrementing the reference count should be the last action in the
		function since it could trigger the deletion of the object.
	*/
	assert(m_reference_count > 0);
	m_reference_count--;
}


bool
hook_image_buffer::open(int i_width, int i_height)
{
	if(m_reference_count == 0)
	{
		/*
			The render hook has already been disconnected and the display driver
			won't bother us anymore after we return false.
		*/
		delete this;
		return false;
	}

	assert(!m_pixels);

	m_reference_count++;

	m_width = i_width;
	m_height = i_height;

	m_pixels = new UT_RGBA[m_width*m_height];
	clear_pixels();

	/*
		Start a thread that updates the view at regular intervals.
		This avoids refreshing on each bucket, which might be costly, especially
		since requesting a refresh appears to be requiring a big lock.

		It also has the responsibility of deleting the image buffer once neither
		the render hook nor the display driver have a reference to it.
	*/
	std::thread(
		[this]()
		{
			update_viewport_loop();
			delete this;
		} ).detach();

	return true;
}


void
hook_image_buffer::data(int i_x, int i_y, int i_w, int i_h, const void* i_data)
{
	assert(m_pixels);

	size_t pixel_size = 4;
	assert(sizeof(UT_RGBA) == pixel_size);
	size_t source_line_length = i_w*pixel_size;
	for(unsigned y = 0; y < i_h; y++)
	{
		const void* source = (const char*)i_data + y*source_line_length;
		void* target = m_pixels + (m_height-(i_y+y)-1)*m_width + i_x;
		memcpy(target, source, source_line_length);
	}
	
	m_timestamp++;
}


void
hook_image_buffer::close()
{
	clear_pixels();

	/*
		Decrementing the reference count should be the last action in the
		function since it could trigger the deletion of the object.
	*/
	assert(m_reference_count > 0);
	m_reference_count--;
}


hook_image_buffer::~hook_image_buffer()
{
	assert(m_reference_count == 0);
	assert(!m_hook);
	operator delete (m_pixels);
}


void
hook_image_buffer::clear_pixels()
{
	assert(m_pixels);
	memset(m_pixels, 0, m_width*m_height*sizeof(UT_RGBA));
	m_timestamp++;
}

void
hook_image_buffer::update_viewport_loop()
{
	unsigned timestamp = m_timestamp;
	while(m_reference_count > 0)
	{
		if(timestamp < m_timestamp)
		{
			/*
				FIXME : We should find a way to call requestDraw without
				using a global lock.
			*/
			HOM_AutoLock hom_lock;

			m_mutex.lock();
			if(m_hook)
			{
				m_hook->viewport().requestDraw();
				timestamp = m_timestamp;
			}
			m_mutex.unlock();
		}

		std::this_thread::sleep_for(k_refresh_interval);
	}
}


/*
	dm_SetupTask is a friend class of DM_VPortAgent class and makes it
	possible to access it's private members, which in our case it is the
	active camera on viewport which we get. Most of the camera attributes
	are found on GUI_ViewParameter, which we have already used, but the
	shutter time parameter is not part of it, so we are using the OBJ_Camera
	to get that value.
*/
class dm_SetupTask
{
public:
	/// Constructor
	dm_SetupTask(DM_VPortAgent& i_viewport);

	OBJ_Camera* m_active_camera{ nullptr };
};
typedef dm_SetupTask VPortAgentCameraAccessor;

dm_SetupTask::dm_SetupTask(DM_VPortAgent& i_viewport)
{
	OP_Node* cam_node = i_viewport.currentCamera();
	if (cam_node)
	{
		m_active_camera = cam_node->castToOBJNode()->castToOBJCamera();
	}
}

/// Holds the viewport's camera's parameters in a ready-to-export format
class viewport_camera
{
public:

	/**
		\brief Constructor of an invalid camera.
		
		It has a negative FOV, which means it will always be different from any
		valid camera. This is useful to initialize a cache before it's updated
		for the first time.
	*/
	viewport_camera();
	/// Constructor from view parameters
	viewport_camera(
		double i_time,
		GUI_ViewParameter& i_view,
		OBJ_Camera* active_camera);

	/// Inequality operator
	bool operator!=(const viewport_camera& i_vc)const;

	/// Exports the camera with the specified handle to an NSI context.
	void nsi_export(NSI::Context& i_nsi, const std::string& i_handle)const;

private:

	double m_time;
	double m_clip[2];
	double m_focal_length;
	double m_focus_distance{0.0};
	double m_fstop{0.0};
	float m_fov{-1.0f};
	double m_shutter{0.0};
	OBJ_Camera* m_active_camera_obj{nullptr};
	bool m_enable_dof{false};
};


viewport_camera::viewport_camera()
{
}


viewport_camera::viewport_camera(
	double i_time,
	GUI_ViewParameter& i_view,
	OBJ_Camera* active_camera)
{
	m_time = i_time;
	i_view.getLimits(m_clip+0, m_clip+1);
	m_focal_length = i_view.getFocalLength();
#if SYS_VERSION_MAJOR_INT >= 18
	m_focus_distance = i_view.getFocusDistance();
	m_fstop = i_view.getFStop();
#endif

	double resolution[2] =
		{ double(i_view.getViewWidth()), double(i_view.getViewHeight()) };
	double fov_rad =
		2.0 *
		atan((i_view.getAperture() / 2.0) *
			(resolution[1] / resolution[0]) /
			m_focal_length);
	m_fov = float(fov_rad * 180.0 / M_PI);

	if (active_camera)
	{
		double frame_duration = OPgetDirector()->getChannelManager()->getSecsPerSample();
		m_shutter = active_camera->SHUTTER(i_time) * frame_duration;
		m_active_camera_obj = active_camera;
		m_enable_dof = active_camera->evalInt("_3dl_enable_dof", 0, m_time);
	}
	// Focal length seems to be in thousandths of scene units
	m_focal_length /= 1000.0;
}


bool
viewport_camera::operator!=(const viewport_camera& i_vc)const
{
	return
		m_time != i_vc.m_time ||
		m_clip[0] != i_vc.m_clip[0] || m_clip[1] != i_vc.m_clip[1] ||
		m_focal_length != i_vc.m_focal_length ||
		m_focus_distance != i_vc.m_focus_distance ||
		m_fstop != i_vc.m_fstop ||
		m_fov != i_vc.m_fov ||
		m_shutter != i_vc.m_shutter ||
		m_active_camera_obj != i_vc.m_active_camera_obj ||
		m_enable_dof != i_vc.m_enable_dof;
}


void
viewport_camera::nsi_export(
	NSI::Context& i_nsi,
	const std::string& i_handle)const
{
	assert(m_fov >= 0.0f);
	double nsi_shutter[2] = { m_time - m_shutter / 2.0, m_time + m_shutter / 2.0 };
	int enable_dof = false;
	if (m_active_camera_obj)
	{
		enable_dof = m_focus_distance > 0.0 && m_enable_dof;
	}
	i_nsi.SetAttribute(
		i_handle,
		(
			*NSI::Argument("shutterrange")
				.SetType(NSITypeDouble)
				->SetCount(2)
				->CopyValue(nsi_shutter, sizeof(nsi_shutter)),
			*NSI::Argument("clippingdistance")
				.SetType(NSITypeDouble)
				->SetCount(2)
				->CopyValue(m_clip, sizeof(m_clip)),
			NSI::FloatArg("fov", m_fov),
			NSI::IntegerArg("depthoffield.enable", enable_dof)
		) );

	if(enable_dof)
	{
		i_nsi.SetAttribute(
			i_handle,
			(
				NSI::DoubleArg("depthoffield.focallength", m_focal_length),
				NSI::DoubleArg("depthoffield.focaldistance", m_focus_distance),
				NSI::DoubleArg("depthoffield.fstop", m_fstop)
		) );
	}
}



/**
	\brief Per-viewport scene render hook.
	
	It exports Houdini's camera attributes to the NSI context, along with an
	output driver node, then fulfills render requests received from Houdini.
*/
class viewport_hook : public DM_SceneRenderHook
{
public:

	/// Constructor
	viewport_hook(DM_VPortAgent& i_viewport);
	/// Destructor
	virtual ~viewport_hook();

	/// Render method called when the rendering task is required.
	bool render(
		RE_Render* i_render,
		const DM_SceneHookData& i_hook_data) override;

	///	Adds output nodes to the context, including an "outputdriver" node.
	void connect(NSI::Context* io_nsi);
	/// Disconnects from the NSI context and from the image buffer.
	void disconnect();
	OBJ_Camera* get_camera();

	/// Returns a numerical ID for the viewport hook
	int id()const { return viewport().getUniqueId(); }

	/// Returns the image buffer displayed by the viewport hook
	hook_image_buffer* buffer() { return m_image_buffer; }

private:

	/// Returns a viewport-specific prefix for NSI handles
	std::string handle_prefix()const;
	/// Returns the NSI handle for the camera node
	std::string camera_handle()const;
	/// Returns the NSI handle for the camera transform node
	std::string camera_transform_handle()const;

	/**
		\bref Exports the NSI camera and its transform.

		\param i_view
			The viewing parameters that describe the camera.
		\param i_synchronize
			True if the function is called once rendering has started, and any
			edit requires sending a "synchronize" action to NSIRenderControl.
	*/
	void export_camera_attributes(
		GUI_ViewParameter& i_view,
		OBJ_Camera* active_camera,
		bool i_synchronize)const;

	// Used to synchronize open(), close(), connect() and disconnect().
	std::mutex m_mutex;

	// NSI context of the associated render
	NSI::Context* m_nsi{nullptr};
	// Image buffer shared with a display driver
	hook_image_buffer* m_image_buffer{nullptr};

	bool m_render_called_once{false};

	// Cache for previously sent camera attributes. Avoids redundant updates.
	mutable viewport_camera m_last_camera;
	mutable UT_Matrix4D m_last_camera_transform{1.0};
};


viewport_hook::viewport_hook(DM_VPortAgent& i_viewport)
	:	DM_SceneRenderHook(i_viewport, DM_VIEWPORT_ALL)
{
}


viewport_hook::~viewport_hook()
{
	// Disconnect from the display driver if this hasn't been done already
	disconnect();
}


bool
viewport_hook::render(
	RE_Render* i_render,
	const DM_SceneHookData& i_hook_data)
{
	m_render_called_once = true;

	DM_VPortAgent& vp = viewport();
	GUI_ViewState& vs = vp.getViewStateRef();
	GUI_ViewParameter& view = vs.getViewParameterRef();

	/*
		Update NSI camera at each refresh, since we don't get notified when it
		changes. If its attributes haven't changed since the last update, it
		will be ignored by 3Delight and the render won't be restarted.
	*/
	m_mutex.lock();
	if(m_nsi)
	{
		VPortAgentCameraAccessor* cam = new VPortAgentCameraAccessor(vp);
		export_camera_attributes(view, cam->m_active_camera, true);
	}
	m_mutex.unlock();

	if(!m_image_buffer || !m_image_buffer->pixels())
	{
		return false;
	}

	// Save some OpenGL state

	i_render->pushDepthState();
	i_render->disableDepthTest();
	i_render->disableDepthBufferWriting();

	i_render->pushBlendState();
	i_render->blend(true);
	i_render->blendAlphaPremult(true);

	/*
		Depending on how the hook was connected, it might already be
		operating in raster space. Otherwise, we have to set the viewing and
		projection properly.
	*/
	bool pixel_space =
		i_hook_data.hook_type == DM_HOOK_BACKGROUND ||
		i_hook_data.hook_type == DM_HOOK_FOREGROUND;
	if(!pixel_space)
	{
		i_render->pushMatrix(false, RE_MATRIX_VIEWING);
		i_render->loadIdentityMatrix(RE_MATRIX_VIEWING);
		i_render->pushMatrix(false, RE_MATRIX_PROJECTION);
		i_render->ortho2DW(0.0f, i_hook_data.view_width, 0.0f, i_hook_data.view_height);
	}

	/*
		The image might not be the same size as the viewport, either because
		the camera its own aspect ratio, or because the viewport has been
		resized. So, we adjust the image so it keeps the same aspect ratio
		and is as big as possible while still fitting entirely in the
		viewport. This leaves a pair of margins.
	*/
	float iw = m_image_buffer->m_width;
	float ih = m_image_buffer->m_height;
	float vw = i_hook_data.view_width;
	float vh = i_hook_data.view_height;
	
	float image_ratio = iw / ih;
	float view_ratio = vw / vh;

	float x = 0.0f;
	float y = 0.0f;
	float z = view.getNearClip();
	float zoom = 1.0f;
	if(image_ratio < view_ratio)
	{
		// Fit height, add left & right margins
		zoom = vh / ih;
		x = (vw - zoom*iw) / 2.0f;
	}
	else
	{
		// Fit width, add top & bottom margins
		zoom = vw / iw;
		y = (vh - zoom*ih) / 2.0f;
	}

	// Render the image
	i_render->displayRasterTexture(
		x, y, z,
		m_image_buffer->m_width, m_image_buffer->m_height,
		m_image_buffer->pixels(),
		m_image_buffer->line_offset(),
		zoom, zoom);

	// Restore OpenGL state
	if(!pixel_space)
	{
		i_render->popMatrix(false, RE_MATRIX_PROJECTION);
		i_render->popMatrix(false, RE_MATRIX_VIEWING);
	}
	i_render->popBlendState();
	i_render->popDepthState();

	return true;
}

OBJ_Camera*
viewport_hook::get_camera()
{
	DM_VPortAgent& vp = viewport();
	VPortAgentCameraAccessor* cam = new VPortAgentCameraAccessor(vp);
	return cam->m_active_camera;
}

double
viewport_hook_builder::active_vport_camera_shutter()
{
	OBJ_Camera* cam = nullptr;
	float max_shutter_duration = 0;

	/*
		Get the the largest shutter duration value of active cameras.
	*/
	for (auto hook : m_hooks)
	{
		cam = hook->get_camera();
		if (cam != nullptr)
		{
			max_shutter_duration =
				std::fmax(max_shutter_duration, cam->SHUTTER(0));
		}
	}
	return max_shutter_duration;
}


void
viewport_hook::connect(NSI::Context* io_nsi)
{
	m_mutex.lock();

	if(!m_render_called_once)
	{
		m_mutex.unlock();
		return;
	}

	assert(io_nsi);
	assert(!m_nsi);
	m_nsi = io_nsi;
	
	assert(!m_image_buffer);
	m_image_buffer = new hook_image_buffer(this);

	m_mutex.unlock();

	DM_VPortAgent& vp = viewport();
	GUI_ViewState& vs = vp.getViewStateRef();
	GUI_ViewParameter& view = vs.getViewParameterRef();
	
	std::string prefix = handle_prefix();
	std::string camera_type = "perspectivecamera";

	double time =
		OPgetDirector()->getChannelManager()->getEvaluateTime(SYSgetSTID());
	double screen_w[4];

	OBJ_Camera* active_camera = get_camera();
	if (active_camera)
	{
		UT_String projection;
		active_camera->evalString(projection, "projection", 0, time);
		if (projection.toStdString() == "ortho")
		{
			camera_type = "orthographiccamera";
		}
		camera::get_screen_window(screen_w, *active_camera, time, true);
	}

	// Export camera
	std::string camera_trs = camera_transform_handle();
	m_nsi->Create(camera_trs, "transform");
	m_nsi->Connect(camera_trs, "", NSI_SCENE_ROOT, "objects");

	std::string camera = camera_handle();
	m_nsi->Create(camera, camera_type);
	m_nsi->Connect(camera, "", camera_trs, "objects");

	VPortAgentCameraAccessor* cam = new VPortAgentCameraAccessor(vp);
	export_camera_attributes(view, cam->m_active_camera, false);

	// Export screen
	int resolution[2] = { vs.getViewWidth(), vs.getViewHeight() };
	std::string screen = prefix + "screen";
	m_nsi->Create(screen, "screen");
	m_nsi->SetAttribute(
		screen,
		(
			*NSI::Argument("resolution").
				SetArrayType(NSITypeInteger, 2)->
				SetValuePointer(resolution),
			NSI::IntegerArg("oversampling", 16)
		) );

	if (active_camera)
	{
		m_nsi->SetAttribute(
			screen,
			*NSI::Argument("screenwindow").
			SetArrayType(NSITypeDouble, 2)
			->SetCount(2)
			->SetValuePointer(screen_w));
	}

	m_nsi->Connect(screen, "", camera, "screens");

	// Export 8-bit Ci layer
	std::string layer = prefix + "Ci_layer";
	m_nsi->Create(layer, "outputlayer");
	m_nsi->SetAttribute(
		layer,
		(
			NSI::CStringPArg("variablename", "Ci"),
			NSI::CStringPArg("scalarformat", "uint8"),
			NSI::CStringPArg("layertype", "color"),
			NSI::IntegerArg("withalpha", 1)
		) );
	m_nsi->Connect(layer, "", screen, "outputlayers");

	// Export the viewport driver
	std::string driver = prefix + "driver";
	m_nsi->Create(driver, "outputdriver");
	m_nsi->SetAttribute(
		driver,
		(
			NSI::CStringPArg("drivername", k_driver_name),
			NSI::IntegerArg(k_viewport_hook_name, id())
		) );
	m_nsi->Connect(driver, "", layer, "outputdrivers");
}


void
viewport_hook::disconnect()
{
	m_mutex.lock();

	m_render_called_once = false;

	if(!m_image_buffer)
	{
		// Already disconnected
		m_mutex.unlock();
		return;
	}

	/*
		Disconnect from the hook_image_buffer to ensure that we won't receive any
		more buckets from the display driver.
	*/
	m_image_buffer->disconnect();
	m_image_buffer = nullptr;

	m_nsi = nullptr;

	m_last_camera = viewport_camera();
	m_last_camera_transform = UT_Matrix4D(1.0);
	
	m_mutex.unlock();
	
	// Restore the viewport to its regular appearance
	viewport().requestDraw();
}


std::string
viewport_hook::handle_prefix()const
{
	return "view" + std::to_string(viewport().getUniqueId()) + "_";
}


std::string
viewport_hook::camera_handle()const
{
	return handle_prefix() + "_camera";
}


std::string
viewport_hook::camera_transform_handle()const
{
	return handle_prefix() + "_camera_transform";
}


void
viewport_hook::export_camera_attributes(
	GUI_ViewParameter& i_view,
	OBJ_Camera* active_camera,
	bool i_synchronize)const
{
	assert(m_nsi);

	bool updated = false;
	
	/*
		For both camera transform and camera attributes, we avoid sending
		redundant updates to 3Delight. Sending them would still give the correct
		image without restarting the render, but it would still incur a small
		performance penalty which, unfortunately, becomes non-negligeable at ~20
		updates per second.
	*/

	UT_Matrix4D transform = i_view.getItransformMatrix();
	if(transform != m_last_camera_transform)
	{
		m_nsi->SetAttribute(
			camera_transform_handle(),
			NSI::DoubleMatrixArg("transformationmatrix", transform.data()));
		m_last_camera_transform = transform;
		updated = true;
	}

	double time =
		OPgetDirector()->getChannelManager()->getEvaluateTime(SYSgetSTID());
	viewport_camera cam(time, i_view, active_camera);
	if(cam != m_last_camera)
	{
		cam.nsi_export(*m_nsi, camera_handle());
		m_last_camera = cam;
		updated = true;
	}

	if(updated && i_synchronize)
	{
		m_nsi->RenderControl(NSI::CStringPArg("action", "synchronize"));
	}
}



// 3Delight display driver implementation.
namespace
{
	PtDspyError dpy_open(
		PtDspyImageHandle* o_image,
		const char*,
		const char*,
		int i_width,
		int i_height,
		int i_nb_parameters,
		const UserParameter* i_parameters,
		int,
		PtDspyDevFormat*,
		PtFlagStuff*)
	{
		// Find the image buffer address
		hook_image_buffer* buf = nullptr;
		for(unsigned p = 0; p < i_nb_parameters; p++)
		{
			if(strcmp(i_parameters[p].name, k_viewport_hook_name) == 0)
			{
				int id = *(int*)i_parameters[p].value;
				buf = viewport_hook_builder::instance().open(id);
				break;
			}
		}

		if(!buf)
		{
			// No image buffer
			return PkDspyErrorBadParams;
		}

		if(!buf->open(i_width, i_height))
		{
			/*
				Image buffer is not ready to receive data (the render hook might
				already have been disconnected).
			*/
			return PkDspyErrorStop;
		}

		*o_image = buf;
		return PkDspyErrorNone;
	}

	PtDspyError dpy_data(
		PtDspyImageHandle i_image,
		int xmin,
		int xmax,
		int ymin,
		int ymax,
		int,
		const unsigned char* i_data)
	{
		hook_image_buffer* buf = (hook_image_buffer*)i_image;
		assert(buf);
		buf->data(xmin, ymin, xmax-xmin, ymax-ymin, i_data);
		return PkDspyErrorNone;
	}
	
	PtDspyError dpy_close(PtDspyImageHandle i_image)
	{
		hook_image_buffer* buf = (hook_image_buffer*)i_image;
		assert(buf);
		buf->close();

		return PkDspyErrorNone;
	}
	
	PtDspyError
	dpy_query(
		PtDspyImageHandle i_image,
		PtDspyQueryType i_type,
		int i_data_size,
		void* i_data)
	{
		switch(i_type)
		{
			case PkThreadQuery:
			{
				if(i_data_size < (int)sizeof(PtDspyThreadInfo))
				{
					return PkDspyErrorBadParams;
				}

				((PtDspyThreadInfo*)i_data)->multithread = 1;
				break;
			}
			case PkProgressiveQuery:
			{
				if(i_data_size < (int)sizeof(PtDspyProgressiveInfo))
				{
					return PkDspyErrorBadParams;
				}

				((PtDspyProgressiveInfo*)i_data)->acceptProgressive = 1;
				break;
			}
			case PkStopQuery:
			{
				hook_image_buffer* buf = (hook_image_buffer*)i_image;
				if(!buf || !buf->connected())
				{
					return PkDspyErrorStop;
				}
				break;
			}
			default:
				return PkDspyErrorUnsupported;
		}

		return PkDspyErrorNone;
	}

}



viewport_hook_builder::~viewport_hook_builder()
{
	assert(m_hooks.empty());
}


viewport_hook_builder&
viewport_hook_builder::instance()
{
	static viewport_hook_builder builder;
	return builder;
}


DM_SceneRenderHook*
viewport_hook_builder::newSceneRender(
	DM_VPortAgent& i_viewport,
	DM_SceneHookType i_type,
	DM_SceneHookPolicy i_policy)
{
	m_hooks_mutex.lock();
	viewport_hook* hook = new viewport_hook(i_viewport);
	m_hooks.push_back(hook);
	m_hooks_mutex.unlock();

	return hook;
}


void
viewport_hook_builder::retireSceneRender(
	DM_VPortAgent& i_viewport,
	DM_SceneRenderHook* io_hook)
{
	m_hooks_mutex.lock();
	auto hook = std::find(m_hooks.begin(), m_hooks.end(), io_hook);
	assert(hook != m_hooks.end());
	m_hooks.erase(hook);
	m_hooks_mutex.unlock();

	delete io_hook;
}


void
viewport_hook_builder::connect(NSI::Context* io_nsi)
{
	if(m_nsi)
	{
		/*
			Disconnect previous render. This will disconnect the render hooks
			from their respective image buffer, which will eventually make the
			associated display drivers' "stop query" interrupt the render. We
			can re-connect to the render hooks immediately, though, since they
			will create new image buffers to read from.
		*/
		disconnect();
	}

	m_nsi = io_nsi;

	// Export camera, screen and driver for each viewport hook
	m_hooks_mutex.lock();
	for(auto hook : m_hooks)
	{
		hook->connect(m_nsi);
	}
	m_hooks_mutex.unlock();
}


void
viewport_hook_builder::disconnect()
{
	if(!m_nsi)
	{
		// Already disconnected
		return;
	}
	
	m_hooks_mutex.lock();
	for(auto hook : m_hooks)
	{
		hook->disconnect();
	}
	m_hooks_mutex.unlock();

	m_nsi = nullptr;
}


hook_image_buffer*
viewport_hook_builder::open(int i_viewport_id)
{
	m_hooks_mutex.lock();

	auto hook =
		std::find_if(
			m_hooks.begin(),
			m_hooks.end(),
			[i_viewport_id](viewport_hook* h){ return h && (h->id() == i_viewport_id); } );

	hook_image_buffer* buffer = nullptr;
	if(hook != m_hooks.end())
	{
		buffer = (*hook)->buffer();
	}
	
	m_hooks_mutex.unlock();

	return buffer;
}


viewport_hook_builder::viewport_hook_builder()
	:	DM_SceneHook(
			"3Delight Viewport Renderer", 0
#if SYS_VERSION_MAJOR_INT >= 18
			, DM_HOOK_ALL_VIEWS
#endif
		)
{
	// Register our custom display driver

	PtDspyDriverFunctionTable fcts;
	memset(&fcts, 0, sizeof(fcts));
	fcts.Version = k_PtDriverCurrentVersion;
	fcts.pOpen = &dpy_open;
	fcts.pWrite = &dpy_data;
	fcts.pClose = &dpy_close;
	fcts.pQuery = &dpy_query;

	NSI::DynamicAPI api;
	decltype(&DspyRegisterDriverTable) register_table = nullptr;
	api.LoadFunction(register_table, "DspyRegisterDriverTable" );

	register_table(k_driver_name, &fcts);
}
