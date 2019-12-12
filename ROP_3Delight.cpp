#include "ROP_3Delight.h"

#include "camera.h"
#include "context.h"
#include "exporter.h"
#include "idisplay_port.h"
#include "mplay.h"
#include "object_attributes.h"
#include "scene.h"
#include "shader_library.h"
#include "ui/select_layers_dialog.h"

#include <OBJ/OBJ_Camera.h>
#include <OBJ/OBJ_Light.h>
#include <OP/OP_Director.h>
#include <OP/OP_OperatorTable.h>
#include <RE/RE_Light.h>
#include <ROP/ROP_Templates.h>
#include <UT/UT_Exit.h>
#include <UT/UT_ReadWritePipe.h>
#include <UT/UT_Spawn.h>
#include <UT/UT_TempFileManager.h>
#include <UT/UT_UI.h>

#include <nsi_dynamic.hpp>

#include <iostream>


namespace
{
	NSI::DynamicAPI&
	GetNSIAPI()
	{
		static NSI::DynamicAPI api;
		return api;
	}

	const std::string k_screen_name = "default_screen";

	void ComputePriorityWindow(
		int* o_absolute_window,
		const int* i_res,
		const float* i_relative_window)
	{
		for(unsigned p = 0; p < 4; p++)
		{
			int res = i_res[p%2];
			o_absolute_window[p] = roundf(i_relative_window[p] * float(res));
		}
	}

	void ExitCB(void* i_data)
	{
		ROP_3Delight* rop = (ROP_3Delight*)i_data;
		rop->StopRender();
	}
}

void
ROP_3Delight::Register(OP_OperatorTable* io_table)
{
	io_table->addOperator(
		new OP_Operator(
			"3Delight",
			"3Delight",
			ROP_3Delight::alloc,
			settings::GetTemplatePair(),
			0,
			0,
			settings::GetVariablePair(),
			0u,
			nullptr,
			0,
			"Render"));
	io_table->addOperator(
		new OP_Operator(
			"3DelightCloud",
			"3Delight Cloud",
			ROP_3Delight::cloud_alloc,
			settings::GetTemplatePair(),
			0,
			0,
			settings::GetVariablePair(),
			0u,
			nullptr,
			0,
			"Render"));

	register_mplay_driver( GetNSIAPI() );
}

OP_Node*
ROP_3Delight::alloc(OP_Network* net, const char* name, OP_Operator* op)
{
	return new ROP_3Delight(net, name, op, false);
}

OP_Node*
ROP_3Delight::cloud_alloc(OP_Network* net, const char* name, OP_Operator* op)
{
	return new ROP_3Delight(net, name, op, true);
}

ROP_3Delight::ROP_3Delight(
	OP_Network* net,
	const char* name,
	OP_Operator* entry,
	bool i_cloud)
	:	ROP_Node(net, name, entry),
		m_cloud(i_cloud),
		m_current_render(nullptr),
		m_end_time(0.0),
		m_nsi(GetNSIAPI()),
		m_renderdl(nullptr),
		m_rendering(false),
		m_idisplay_rendering(false),
		m_idisplay_ipr(false),
		m_settings(*this)
{
	UT_Exit::addExitCallback(&ExitCB, this);
}


ROP_3Delight::~ROP_3Delight()
{
	UT_Exit::removeExitCallback(&ExitCB, this);

	idisplay_port::CleanUp();

	StopRender();
}


void ROP_3Delight::onCreated()
{
	ROP_Node::onCreated();
	m_settings.UpdateLights();
	enableParm(settings::k_save_ids_as_cryptomatte, false);
	std::vector<VOP_Node*> custom_aovs;
	scene::find_custom_aovs(custom_aovs);
	aov::updateCustomVariables(custom_aovs);
}

void ROP_3Delight::StartRenderFromIDisplay(
	double i_time,
	bool i_ipr,
	const float* i_window)
{
	assert(!m_idisplay_rendering);

	if(i_window)
	{
		/*
			We can't add parameters to executeSingle, so we send the data
			through private data members instead.
		*/
		memcpy(
			m_idisplay_rendering_window,
			i_window,
			sizeof(m_idisplay_rendering_window));
	}
	else
	{
		m_idisplay_rendering_window[1] = m_idisplay_rendering_window[0] = 0.0f;
		m_idisplay_rendering_window[3] = m_idisplay_rendering_window[2] = 1.0f;
	}

	m_idisplay_ipr = i_ipr;

	/*
		Set m_idisplay_rendering to true only during the call to executeSingle
		so m_idisplay_rendering_window and m_idisplay_ipr are really used only
		as additional function parameters.
	*/
	m_idisplay_rendering = true;
	executeSingle(i_time);
	m_idisplay_rendering = false;
}

void ROP_3Delight::UpdateIDisplayPriorityWindow(const float* i_window)
{
	assert(i_window);

	m_render_end_mutex.lock();
	if(m_rendering && m_current_render && m_current_render->m_ipr)
	{
		memcpy(
			m_idisplay_rendering_window,
			i_window,
			sizeof(m_idisplay_rendering_window));

		int resolution[2];
		if(GetScaledResolution(resolution[0], resolution[1]))
		{
			int priority[4];
			ComputePriorityWindow(priority, resolution, i_window);

			m_nsi.SetAttribute(
				k_screen_name,
				*NSI::Argument::New("prioritywindow")
					->SetArrayType(NSITypeInteger, 2)
					->SetCount(2)
					->SetValuePointer(priority));

			m_nsi.RenderControl(NSI::CStringPArg("action", "synchronize"));
		}
	}
	m_render_end_mutex.unlock();
}

bool ROP_3Delight::HasMotionBlur() const
{
	return
		evalInt(settings::k_motion_blur, 0, 0.0f) &&
		!(HasSpeedBoost() && evalInt(settings::k_disable_motion_blur, 0, 0.0f));
}

/**
	Let's use Houdini's Principled Shader as the default material so that
	any changes we make in there will appear by default in our render.
*/
void ROP_3Delight::ExportDefaultMaterial( const context &i_context ) const
{
	const std::string k_shader( "__default__shader__" );
	const std::string k_attributes( "__default__attributes__" );

	NSI::Context &nsi = i_context.m_nsi;
	const shader_library &library = shader_library::get_instance();
	std::string path = library.get_shader_path( "principledshader::2.0" );

	nsi.Create( k_shader, "shader" );
	nsi.Create( k_attributes, "attributes" );

	nsi.SetAttribute( k_shader, NSI::StringArg("shaderfilename", path) );

	nsi.Connect( k_attributes, "", NSI_SCENE_ROOT, "geometryattributes" );
	nsi.Connect( k_shader, "", k_attributes, "surfaceshader" );
}

void
ROP_3Delight::ExportGlobals(const context& i_ctx)const
{
	int shading_samples = evalInt(settings::k_shading_samples, 0, 0.0f);
	shading_samples = int(float(shading_samples) * GetSamplingFactor() + 0.5f);
	int volume_samples = evalInt(settings::k_volume_samples, 0, 0.0f);
	i_ctx.m_nsi.SetAttribute(
		".global",
		(
			NSI::IntegerArg("quality.shadingsamples", shading_samples),
			NSI::IntegerArg("quality.volumesamples", volume_samples)
		) );

	int max_diffuse_depth = evalInt(settings::k_max_diffuse_depth, 0, 0.0f);
	int max_reflection_depth = evalInt(settings::k_max_reflection_depth, 0, 0.0f);
	int max_refraction_depth = evalInt(settings::k_max_refraction_depth, 0, 0.0f);
	int max_hair_depth = evalInt(settings::k_max_hair_depth, 0, 0.0f);
	i_ctx.m_nsi.SetAttribute(
		".global",
		(
			NSI::IntegerArg("maximumraydepth.diffuse", max_diffuse_depth),
			NSI::IntegerArg("maximumraydepth.reflection", max_reflection_depth),
			NSI::IntegerArg("maximumraydepth.refraction", max_refraction_depth),
			NSI::IntegerArg("maximumraydepth.hair", max_hair_depth)
		) );

	float max_distance = evalInt(settings::k_max_distance, 0, 0.0f);
	i_ctx.m_nsi.SetAttribute(
		".global",
		(
			 NSI::DoubleArg( "maximumraylength.specular", max_distance),
			 NSI::DoubleArg( "maximumraylength.diffuse", max_distance ),
			 NSI::DoubleArg( "maximumraylength.reflection", max_distance),
			 NSI::DoubleArg( "maximumraylength.refraction", max_distance),
			 NSI::DoubleArg( "maximumraylength.hair", max_distance)
		) );

	if(HasSpeedBoost())
	{

		if(evalInt(settings::k_disable_displacement, 0, 0.0f))
		{
			i_ctx.m_nsi.SetAttribute(
				".global", NSI::IntegerArg("show.displacement", 0));
		}

		if(evalInt(settings::k_disable_subsurface, 0, 0.0f))
		{
			i_ctx.m_nsi.SetAttribute(
				".global", NSI::IntegerArg("show.osl.subsurface", 0));
		}
	}
}

void ROP_3Delight::StopRender()
{
	m_render_end_mutex.lock();

	if(m_rendering)
	{
		/*
			Mark the render as over so the stopped callback doesn't call NSIEnd,
			which would make m_nsi invalid, possibly preventing a successful
			call to NSIRenderControl "stop".
		*/
		m_rendering = false;

		if(m_renderdl)
		{
			// Terminate the renderdl process
			UTterminate(m_renderdl->getChildPid());

			// Let the m_renderdl_waiter thread finish
			m_render_end_mutex.unlock();
		}
		else
		{
			assert(m_nsi.Handle() != NSI_BAD_CONTEXT);

			// Unlock the mutex so the stopper callback can do its job.
			m_render_end_mutex.unlock();

			/*
				Notify the background rendering thread that it should stop (and,
				implicitly, wait for it to be finished, including the call to
				the stopper callback).
			*/
			m_nsi.RenderControl(NSI::CStringPArg("action", "stop"));

			// It's now safe to call NSIEnd
			m_nsi.End();
		}
	}
	else
	{
		m_render_end_mutex.unlock();
	}

	/*
		Wait for m_renderdl_waiter to finish. This has to be done even if we
		didn't explicitly terminate the renderdl process (ie : if it has
		finished on its own), because we don't want	the thread hanging loose.
	*/
	if(m_renderdl_waiter.joinable())
	{
		m_renderdl_waiter.join();
		assert(!m_renderdl);
	}

	// Notify the UI that rendering has stopped
	m_settings.Rendering(false);
}

int ROP_3Delight::startRender(int, fpreal tstart, fpreal tend)
{
	// If we still have a render going on, kill it first.
	StopRender();

	bool render = m_idisplay_rendering || GetNSIExportFilename(tstart).empty();
	fpreal fps = OPgetDirector()->getChannelManager()->getSamplesPerSec();
	bool batch = !UTisUIAvailable();
	bool ipr =
		m_idisplay_rendering
		?	m_idisplay_ipr
		:	render && evalInt(settings::k_ipr, 0, 0.0f);

	m_current_render = new context(
		m_nsi,
		tstart,
		tend,
		GetShutterInterval(tstart),
		fps,
		HasDepthOfField(),
		batch,
		ipr,
		!render,
		m_cloud,
		getFullPath().toStdString(),
		m_settings.GetObjectsToRender(),
		m_settings.GetLightsToRender());

	m_end_time = tend;

	m_rendering = render;

	// Notify the UI that a new render might have started
	m_settings.Rendering(render);

	if(m_current_render->BackgroundProcessRendering())
	{
		/*
			Start a renderdl process that will start rendering as soon as we
			have exported the first frame of the sequence.
		*/
		m_renderdl = new UT_ReadWritePipe;
		std::string renderdl_command = "renderdl -stdinfiles";

		if(m_cloud)
		{
			renderdl_command += " -cloud -cloudtag HOUDINI";
			if(batch)
			{
				renderdl_command += "_BATCH";
			}
		}

		if(!m_renderdl->open(renderdl_command.c_str()))
		{
			delete m_renderdl;
			return 0;
		}

		/*
			Start a thread that will wait for the renderdl process to finish and
			then delete the m_renderdl pipe object.
		*/
		m_renderdl_waiter =
			std::thread(
				[this]()
				{
					UTwait(m_renderdl->getChildPid());

					m_render_end_mutex.lock();

					m_rendering = false;

					// Notify the UI that rendering has stopped
					m_settings.Rendering(false);

					delete m_renderdl; m_renderdl = nullptr;

					m_render_end_mutex.unlock();
				} );
	}

	if(error() < UT_ERROR_ABORT)
	{
		executePreRenderScript(tstart);
	}

	return 1;
}

ROP_RENDER_CODE
ROP_3Delight::renderFrame(fpreal time, UT_Interrupt*)
{
	assert(m_current_render);
	assert(m_nsi.Handle() == NSI_BAD_CONTEXT);

	m_current_render->m_current_time = time;

	std::string frame_nsi_file;
	if(m_current_render->m_export_nsi)
	{
		std::string export_file = GetNSIExportFilename(time);
		assert(!export_file.empty());

		// Output NSI commands to the specified file or standard output
		m_nsi.Begin(
		(
			NSI::StringArg("streamfilename", export_file),
			NSI::CStringPArg("streamformat", "nsi")
		) );
	}
	else if(m_renderdl)
	{
		/*
			Output NSI commands to a temporary file that will be rendered by a
			separate renderdl process.
		*/
		frame_nsi_file = UT_TempFileManager::getTempFilename();
		m_nsi.Begin(
		(
			NSI::StringArg("streamfilename", frame_nsi_file),
			NSI::CStringPArg("streamformat", "binarynsi")
		) );
	}
	else
	{
		// Render directly from the current process
		m_nsi.Begin();
	}

	executePreFrameScript(time);

	object_attributes::export_object_attribute_nodes(*m_current_render);
	ExportTransparentSurface(*m_current_render);

	scene::convert_to_nsi( *m_current_render, m_interests );

	ExportAtmosphere(*m_current_render);
	ExportOutputs(*m_current_render);

	ExportGlobals(*m_current_render);
	ExportDefaultMaterial(*m_current_render);

	if(m_current_render->BackgroundThreadRendering())
	{
		// Define a callback to be called when the rendering thread is finished
		void (*render_stopped)(void*, NSIContext_t, int) =
			[](void* i_data, NSIContext_t i_ctx, int i_status)
			{
				if(i_status != NSIRenderCompleted &&
					i_status != NSIRenderAborted)
				{
					return;
				}

				ROP_3Delight* rop = (ROP_3Delight*)i_data;

				rop->m_render_end_mutex.lock();

				rop->m_interests.clear();
				delete rop->m_current_render; rop->m_current_render = nullptr;

				/*
					If m_rendering is still true, it means that the render
					wasn't stopped from ROP_3Delight::StopRender, so it's safe
					to (and we have to) close the context. Otherwise, we leave
					the context intact so it can be used by StopRender to wait
					for the end of the render.
				*/
				if(rop->m_rendering)
				{
					rop->m_rendering = false;

					// Notify the UI that rendering has stopped
					rop->m_settings.Rendering(false);

					rop->m_nsi.End();
				}

				rop->m_render_end_mutex.unlock();
			};

		/*
			Start rendering in a background thread of the current process.
			NSIEnd will be called once rendering is finished.
		*/
		m_nsi.RenderControl(
		(
			NSI::CStringPArg("action", "start"),
			NSI::IntegerArg("interactive", m_current_render->m_ipr),
			NSI::PointerArg("stoppedcallback", (const void*)render_stopped),
			NSI::PointerArg("stoppedcallbackdata", this)
		) );
		// In that case, the "m_nsi" NSI::Context is closed by RenderStoppedCB
	}
	else
	{
		// Export an NSIRenderControl "start" command at the end of the frame
		m_nsi.RenderControl(NSI::CStringPArg("action", "start"));

		/*
			If we're rendering in batch mode from the current process, then we
			must wait for the render to finish (as if we were rendering "in
			foreground", from the current thread).
		*/
		if(m_current_render->m_batch && !m_renderdl)
		{
			m_nsi.RenderControl(NSI::CStringPArg("action", "wait"));
		}

		// The frame has finished exporting or rendering, close the context
		m_nsi.End();
	}

	if(m_renderdl)
	{
		// Communicate the name of the exported NSI file to the renderdl process
		fprintf(m_renderdl->getWriteFile(), "%s\n", frame_nsi_file.c_str());
		fflush(m_renderdl->getWriteFile());
	}

	if(error() < UT_ERROR_ABORT)
	{
		executePostFrameScript(time);
	}

	return ROP_CONTINUE_RENDER;
}

ROP_RENDER_CODE
ROP_3Delight::endRender()
{
	if(error() < UT_ERROR_ABORT)
	{
		executePostRenderScript(m_end_time);
	}

	// Close the renderdl NSI file names pipe if necessary
	if(m_renderdl)
	{
		/*
			Send an empty file name to signal the end of the list. The renderdl
			process will exit and close the pipe from its end.
		*/
		fprintf(m_renderdl->getWriteFile(), "\n");
		fflush(m_renderdl->getWriteFile());
	}

	/*
		If we render in a background thread, the interests and rendering context
		will be destroyed by this thread's stoppedcallback.
	*/
	m_render_end_mutex.lock();
	if(m_current_render && !m_current_render->BackgroundThreadRendering())
	{
		m_interests.clear();
		delete m_current_render; m_current_render = nullptr;
	}
	m_render_end_mutex.unlock();

	return ROP_CONTINUE_RENDER;
}

bool
ROP_3Delight::updateParmsFlags()
{
	bool changed = OP_Network::updateParmsFlags();

	PRM_Parm& parm = getParm(settings::k_aov);
	int size = parm.getMultiParmNumItems();

	if (size > 0) changed |= enableParm("aov_clear_1", size > 1);

	for (int i = 0; i < size; i++)
	{
		changed |= enableParm(aov::getAovStrToken(i), false);
	}

	changed |= enableParm(settings::k_view_layer, false);
	changed |= enableParm(settings::k_display_all_lights, false);

	return changed;
}

void
ROP_3Delight::loadFinished()
{
	ROP_Node::loadFinished();
	m_settings.UpdateLights();
}

void
ROP_3Delight::ExportTransparentSurface(const context& i_ctx) const
{
	std::string shaderHandle = exporter::TransparentSurfaceHandle();
	shaderHandle =+ "|shader";

	NSI::Context& nsi = i_ctx.m_nsi;
	nsi.Create(exporter::TransparentSurfaceHandle(), "attributes");
	nsi.Create(shaderHandle.c_str(), "shader");

	const shader_library& library = shader_library::get_instance();
	std::string path = library.get_shader_path( "transparent" );

	nsi.SetAttribute(
		shaderHandle.c_str(),
		NSI::CStringPArg("shaderfilename", path.c_str()));

	NSI::ArgumentList argList;
	argList.Add(new NSI::IntegerArg("priority", 60));

	nsi.Connect(
		shaderHandle.c_str(), "",
		exporter::TransparentSurfaceHandle(), "surfaceshader", argList );
}

std::string
ROP_3Delight::AtmosphereAttributesHandle() const
{
	return "3delight_render_pass_atmosphere_attributes";
}

void
ROP_3Delight::ExportAtmosphere(const context& i_ctx)const
{
	UT_String atmHandle = m_settings.GetAtmosphere();

	if (atmHandle.length() == 0) return;

	// This is the shader handle which has been created elsewhere
	std::string shaderHandle = atmHandle.toStdString();

	// Test if the node still exist
	OP_Node* op_node = OPgetDirector()->findNode(shaderHandle.c_str());
	if (!op_node)
		return;

	atmHandle += "_environment";

	i_ctx.m_nsi.Create( atmHandle.c_str(), "environment" );
	i_ctx.m_nsi.Connect( atmHandle.c_str(), "", NSI_SCENE_ROOT, "objects" );

	i_ctx.m_nsi.Create( AtmosphereAttributesHandle().c_str(), "attributes" );

	i_ctx.m_nsi.Connect(
		shaderHandle.c_str(), "",
		AtmosphereAttributesHandle().c_str(), "volumeshader" );

	i_ctx.m_nsi.Connect(
		AtmosphereAttributesHandle().c_str(), "",
		atmHandle.c_str(), "geometryattributes" );
}

void
ROP_3Delight::ExportOutputs(const context& i_ctx)const
{
	OBJ_Camera* cam = GetCamera();

	if( !cam )
	{
		return;
	}

	fpreal current_time = i_ctx.m_current_time;

	int default_resolution[2];
	GetScaledResolution(default_resolution[0], default_resolution[1]);

	i_ctx.m_nsi.Create(k_screen_name, "screen");
	i_ctx.m_nsi.SetAttribute(
		k_screen_name,
		(
			*NSI::Argument::New("resolution")
			->SetArrayType(NSITypeInteger, 2)
			->SetCount(1)
			->CopyValue(default_resolution, sizeof(default_resolution)),
			NSI::IntegerArg("oversampling", GetPixelSamples())
		) );

	// Set the crop window or priority window

	if(m_idisplay_rendering)
	{
		if(m_current_render->m_ipr)
		{
			int priority[4];
			ComputePriorityWindow(
				priority,
				default_resolution,
				m_idisplay_rendering_window);
			i_ctx.m_nsi.SetAttribute(
				k_screen_name,
				*NSI::Argument::New("prioritywindow")
					->SetArrayType(NSITypeInteger, 2)
					->SetCount(2)
					->SetValuePointer(priority));
		}
		else
		{
			i_ctx.m_nsi.SetAttribute(
				k_screen_name,
				*NSI::Argument::New("crop")
					->SetArrayType(NSITypeFloat, 2)
					->SetCount(2)
					->SetValuePointer(m_idisplay_rendering_window));
		}
	}
	else
	{
		float cam_crop[4] =
		{
			float(cam->CROPL(0)), float(cam->CROPB(0)),
			float(cam->CROPR(0)), float(cam->CROPT(0))
		};
		i_ctx.m_nsi.SetAttribute(
			k_screen_name,
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
	if(camera::get_ortho_screen_window(sw, *cam, current_time))
	{
		i_ctx.m_nsi.SetAttribute(
			k_screen_name,
			*NSI::Argument::New("screenwindow")
			->SetArrayType(NSITypeDouble, 2)
			->SetCount(2)
			->SetValuePointer(sw));
	}

	i_ctx.m_nsi.Connect(
		k_screen_name, "",
		camera::get_nsi_handle(*cam), "screens");

	UT_String idisplay_driver = "idisplay";
	UT_String file_driver;
	evalString(
		file_driver,
		settings::k_default_image_format, 0,
		current_time );

	UT_String png_driver = "png";
	UT_String jpeg_driver = "jpeg";

	UT_String image_file_name;
	evalString(
		image_file_name,
		settings::k_default_image_filename, 0,
		current_time );

	UT_String image_display_name =
		image_file_name.replaceExtension(idisplay_driver);
	image_file_name = image_file_name.replaceExtension(file_driver);

	std::string idisplay_driver_name;
	std::string file_driver_name;
	std::string png_driver_name;
	std::string jpeg_driver_name;

	e_fileOutputMode output_mode = e_disabled;

	if (i_ctx.m_export_nsi || i_ctx.m_batch)
	{
		int mode = evalInt(settings::k_batch_output_mode, 0, 0.0f);
		if (mode == 0) output_mode = e_useToggleStates;
		else output_mode = e_allFilesAndSelectedJpeg;
	}
	else
	{
		int mode = evalInt(settings::k_interactive_output_mode, 0, 0.0f);
		if (mode == 0) output_mode = e_useToggleStates;
		else if (mode == 1) output_mode = e_useToggleAndFramebufferStates;
	}

	int nb_aovs = evalInt(settings::k_aov, 0, 0.0f);
	unsigned sort_key = 0;

	UT_String scalar_format;
	evalString(
		scalar_format,
		settings::k_default_image_bits, 0, current_time );

	UT_String filter;
	evalString(filter, settings::k_pixel_filter, 0, 0.0f);
	double filter_width = evalFloat(settings::k_filter_width, 0, 0.0f);

	std::vector<std::string> light_names;
	light_names.push_back(""); // no light for the first one
	m_settings.GetSelectedLights(light_names);

	bool has_frame_buffer = false;

	for (int i = 0; i < nb_aovs; i++)
	{
		UT_String label;
		evalString(label, aov::getAovStrToken(i), 0, current_time );

		const aov::description& desc = aov::getDescription(label.toStdString());

		bool idisplay_output = evalInt(aov::getAovFrameBufferOutputToken(i), 0, 0.0f);
		bool file_output = evalInt(aov::getAovFileOutputToken(i), 0, 0.0f);
		bool png_output = file_output;
		file_output = file_output && file_driver.toStdString() != "png";
		png_output = png_output && file_driver.toStdString() == "png";
		bool jpeg_output = evalInt(aov::getAovJpegOutputToken(i), 0, 0.0f);
		idisplay_output = idisplay_output && !i_ctx.m_batch;

		if (output_mode == e_disabled)
		{
			file_output = false;
			png_output = false;
			jpeg_output = false;
		}
		else if (output_mode == e_allFilesAndSelectedJpeg)
		{
			// Ignore toggle state for file_output/png_output
			file_output = file_driver.toStdString() != "png";
			png_output = file_driver.toStdString() == "png";
		}
		else if (output_mode == e_useToggleAndFramebufferStates)
		{
			// Files output depends of toggle state idisplay_output
			file_output = file_output && idisplay_output;
			png_output = png_output && idisplay_output;
			jpeg_output = jpeg_output && idisplay_output;
		}

		if (!idisplay_output && !file_output && !png_output && !jpeg_output)
		{
			continue;
		}

		char prefix[12] = "";
		::sprintf(prefix, "%d", i+1);

		unsigned nb_light_categories = 1;
		if (desc.m_support_multilight)
		{
			nb_light_categories = light_names.size();
		}

		for (unsigned j = 0; j < nb_light_categories; j++)
		{
			std::string layer_name = prefix;
			layer_name += "-";
			layer_name += desc.m_filename_token;
			layer_name += "-";
			layer_name += light_names[j];

			if (idisplay_output)
			{
				has_frame_buffer = true;

				if (idisplay_driver_name.empty())
				{
					idisplay_driver_name = "idisplay_driver";
					i_ctx.m_nsi.Create(
						idisplay_driver_name.c_str(), "outputdriver");
					i_ctx.m_nsi.SetAttribute(
						idisplay_driver_name.c_str(),
					(
						NSI::CStringPArg("drivername", idisplay_driver.c_str()),
						NSI::CStringPArg("imagefilename", image_display_name.c_str()),
						NSI::CStringPArg("ropname", getFullPath().c_str())
					) );
				}

				ExportOneOutputLayer(
					i_ctx, layer_name, desc, scalar_format,
					filter, filter_width,
					k_screen_name, light_names[j],
					idisplay_driver_name, sort_key);

				if( !m_current_render->m_export_nsi &&
					!m_current_render->m_batch )
				{
					/*
						Only export this for Preview renders.
					*/
					ExportLayerFeedbackData(
						i_ctx, layer_name, light_names[j] );
				}
			}

			if (file_output)
			{
				std::string file_layer_name = layer_name + "_file";

				if (file_driver_name.empty())
				{
					file_driver_name = "file_driver";
					i_ctx.m_nsi.Create(
						file_driver_name.c_str(), "outputdriver");
					i_ctx.m_nsi.SetAttribute(
						file_driver_name.c_str(),
					(
						NSI::CStringPArg("drivername", file_driver.c_str()),
						NSI::CStringPArg("imagefilename", image_file_name.c_str())
					) );
				}

				ExportOneOutputLayer(
					i_ctx, file_layer_name, desc, scalar_format,
					filter, filter_width,
					k_screen_name, light_names[j],
					file_driver_name, sort_key);
			}

			if (png_output)
			{
				std::string png_layer_name = layer_name + "_png";

				char suffix[12] = "";
				::sprintf(suffix, "%u", i*nb_light_categories+j+1);
				png_driver_name = "png_driver_";
				png_driver_name += suffix;

				UT_String image_png_name;
				BuildImageUniqueName(
					image_file_name, light_names[j],
					desc.m_filename_token, ".png", image_png_name);

				i_ctx.m_nsi.Create(
					png_driver_name.c_str(), "outputdriver");
				i_ctx.m_nsi.SetAttribute(
					png_driver_name.c_str(),
				(
					NSI::CStringPArg("drivername", png_driver.c_str()),
					NSI::CStringPArg("imagefilename", image_png_name.c_str())
				) );

				ExportOneOutputLayer(
					i_ctx, png_layer_name, desc, "uint8",
					filter, filter_width,
					k_screen_name, light_names[j],
					png_driver_name, sort_key);
			}

			if (jpeg_output)
			{
				std::string jpeg_layer_name = layer_name + "_jpeg";

				char suffix[12] = "";
				::sprintf(suffix, "%u", i*nb_light_categories+j+1);
				jpeg_driver_name = "jpeg_driver_";
				jpeg_driver_name += suffix;

				UT_String image_jpeg_name;
				BuildImageUniqueName(
					image_file_name, light_names[j],
					desc.m_filename_token, ".jpg", image_jpeg_name);

				i_ctx.m_nsi.Create(
					jpeg_driver_name.c_str(), "outputdriver");
				i_ctx.m_nsi.SetAttribute(
					jpeg_driver_name.c_str(),
				(
					NSI::CStringPArg("drivername", jpeg_driver.c_str()),
					NSI::CStringPArg("imagefilename", image_jpeg_name.c_str())
				) );

				ExportOneOutputLayer(
					i_ctx, jpeg_layer_name, desc, "uint8",
					filter, filter_width,
					k_screen_name, light_names[j],
					jpeg_driver_name, sort_key);
			}
		}
	}

	/*
		If we have at least one frame buffer output, we go for the circular
		pattern. When only file output is request, we will use the more
		efficient (at least, memory wise) scanline pattern.
	*/
	i_ctx.m_nsi.SetAttribute( NSI_SCENE_GLOBAL,
	(
		NSI::CStringPArg(
			"bucketorder",
			has_frame_buffer ? "circle" : "horizontal")
	) );
}

void
ROP_3Delight::ExportOneOutputLayer(
	const context& i_ctx,
	const std::string& i_layer_handle,
	const aov::description& i_desc,
	const UT_String& i_scalar_format,
	const UT_String& i_filter,
	double i_filter_width,
	const std::string& i_screen_handle,
	const std::string& i_light_handle,
	const std::string& i_driver_handle,
	unsigned& io_sort_key) const
{
	i_ctx.m_nsi.Create(i_layer_handle.c_str(), "outputlayer");
	i_ctx.m_nsi.SetAttribute(
		i_layer_handle.c_str(),
	(
		NSI::CStringPArg("variablename", i_desc.m_variable_name.c_str()),
		NSI::CStringPArg("variablesource", i_desc.m_variable_source.c_str()),
		NSI::CStringPArg("scalarformat", i_scalar_format.c_str()),
		NSI::CStringPArg("layertype", i_desc.m_layer_type.c_str()),
		NSI::IntegerArg("withalpha", (int)i_desc.m_with_alpha),
		NSI::CStringPArg("filter", i_filter.c_str()),
		NSI::DoubleArg("filterwidth", i_filter_width),
		NSI::IntegerArg("sortkey", io_sort_key++)
	) );

	if (i_scalar_format == "uint8")
	{
		i_ctx.m_nsi.SetAttribute(i_layer_handle.c_str(),
			NSI::StringArg("colorprofile", "srgb"));
	}

	i_ctx.m_nsi.Connect(
		i_layer_handle.c_str(), "",
		i_screen_handle.c_str(), "outputlayers");

	if (!i_light_handle.empty())
	{
		i_ctx.m_nsi.Connect(
			i_light_handle.c_str(), "", i_layer_handle.c_str(), "lightset");
	}

	i_ctx.m_nsi.Connect(
		i_driver_handle.c_str(), "",
		i_layer_handle.c_str(), "outputdrivers");
}

void
ROP_3Delight::ExportLayerFeedbackData(
	const context& i_ctx,
	const std::string& i_layer_handle,
	const std::string& i_light_handle) const
{
	idisplay_port *idp = idisplay_port::get_instance();

	std::string host = idp->GetServerHost();
	int port = idp->GetServerPort();

	if( i_light_handle.empty() )
	{
		i_ctx.m_nsi.SetAttribute( i_layer_handle.c_str(),
			(
				NSI::StringArg( "sourceapp", "Houdini" ),
				NSI::StringArg( "feedbackhost", host.c_str() ),
				NSI::IntegerArg( "feedbackport", port )
			));
		return;
	}

	OBJ_Node* obj_node = OPgetDirector()->findOBJNode(i_light_handle.c_str());
	assert(obj_node);

	fpreal r, g, b;
	fpreal dimmer;
	RE_Light* re_light = 0;
	OBJ_Light* obj_light = obj_node->castToOBJLight();
	if (obj_light)
	{
		dimmer = obj_light->DIMMER(0);
		r = obj_light->CR(0);
		g = obj_light->CG(0);
		b = obj_light->CB(0);
		re_light = obj_light->getLightValue();
		assert(re_light);
	}
	else
	{
		OBJ_Ambient* obj_ambient = obj_node->castToOBJAmbient();
		if (obj_ambient)
		{
			dimmer = obj_ambient->DIMMER(0);
			r = obj_ambient->CR(0);
			g = obj_ambient->CG(0);
			b = obj_ambient->CB(0);
			re_light = obj_light->getLightValue();
			assert(re_light);
		}
	}

	if( re_light == 0 )
	{
		i_ctx.m_nsi.SetAttribute( i_layer_handle.c_str(),
			(
				NSI::StringArg( "sourceapp", "Houdini" ),
				NSI::StringArg( "feedbackhost", host.c_str() ),
				NSI::IntegerArg( "feedbackport", port )
			));
		return;
	}

	std::string values;
	std::stringstream ss1;
	ss1 << dimmer;
	values = ss1.str();

	std::string color_values;
	std::stringstream ss2;
	ss2 << r;
	ss2 << ",";
	ss2 << g;
	ss2 << ",";
	ss2 << b;
	color_values = ss2.str();

	RE_Light::RE_HQLightType enum_type = re_light->hqLightType();
	std::string light_type;
	switch (enum_type)
	{
		case RE_Light::HQLIGHT_AMBIENT:
			light_type = "ambient";
			break;
		case RE_Light::HQLIGHT_DIR:
			light_type = "directional";
			break;
		case RE_Light::HQLIGHT_ENV:
			light_type = "environment";
			break;
		case RE_Light::HQLIGHT_POINT:
			light_type = "point";
			break;
		case RE_Light::HQLIGHT_SPOT:
			light_type = "spot";
			break;
		case RE_Light::HQLIGHT_AREA:
			light_type = "area";
			break;
		case RE_Light::HQLIGHT_AREA_SPOT:
			light_type = "area spot";
			break;
		case RE_Light::NUM_HQLIGHT_TYPES:
			light_type = "unknown";
			break;
		default:
			light_type = "unknown";
			break;
	}

	std::string feedback_data;

	/* Make the feedback message */
	/* Name */
	feedback_data = "{\"name\":\"";
	feedback_data += i_light_handle;
	feedback_data += "\",";

	/* Type */
	feedback_data += "\"type\":\"";
	feedback_data += light_type;
	feedback_data += "\",";

	/* Value */
	feedback_data += "\"values\":[";
	feedback_data += values;
	feedback_data += "],";

	/* Color values */
	feedback_data += "\"color_values\":[";
	feedback_data += color_values;
	feedback_data += "]}";

	i_ctx.m_nsi.SetAttribute( i_layer_handle.c_str(),
		(
			NSI::StringArg( "sourceapp", "Houdini" ),
			NSI::StringArg( "feedbackhost", host.c_str() ),
			NSI::IntegerArg( "feedbackport", port ),
			NSI::StringArg( "feedbackdata", feedback_data.c_str() )
		));
}

void
ROP_3Delight::BuildImageUniqueName(
	const UT_String& i_image_file_name,
	const std::string& i_light_name,
	const std::string& i_aov_token,
	const char* i_extension,
	UT_String& o_image_unique_name) const
{
	o_image_unique_name = i_image_file_name.pathUpToExtension();

	std::string fn = o_image_unique_name.toStdString();
	size_t pos = fn.find_last_of('_');
	UT_String frame_number = fn.assign(fn.begin()+pos+1, fn.end()).c_str();

	o_image_unique_name.removeTrailingDigits();

	UT_String light = i_light_name.c_str();
	UT_String dir_name, light_name;
	light.splitPath(dir_name, light_name);

	if (o_image_unique_name.toStdString().back() != '_')
	{
		o_image_unique_name += "_";
	}
	o_image_unique_name += i_aov_token;
	if (light_name.isstring())
	{
		o_image_unique_name += "_";
		o_image_unique_name += light_name;
	}
	if (frame_number.isInteger())
	{
		o_image_unique_name += "_";
		o_image_unique_name += frame_number;
	}
	o_image_unique_name += i_extension;
}
bool
ROP_3Delight::HasSpeedBoost()const
{
	bool batch = !UTisUIAvailable();
	if(batch)
	{
		return false;
	}

	int speed_boost = evalInt(settings::k_speed_boost, 0, 0.0f);
	return speed_boost;
}

bool
ROP_3Delight::GetScaledResolution(int& o_x, int& o_y)const
{
	OBJ_Camera* cam = GetCamera();
	if(!cam)
	{
		return false;
	}

	float scale = GetResolutionFactor();

	o_x = int(::roundf(cam->RESX(0)*scale));
	o_y = int(::roundf(cam->RESY(0)*scale));

	return true;
}

float
ROP_3Delight::GetResolutionFactor()const
{
	if(!HasSpeedBoost())
	{
		return 1.0f;
	}

	int resolution_factor = evalInt(settings::k_resolution_factor, 0, 0.0f);

	float factors[] = { 1.0f, 0.5f, 0.25f, 0.125f };
	if(resolution_factor < 0 ||
		resolution_factor >= sizeof(factors) / sizeof(factors[0]))
	{
		return 1.0f;
	}

	return factors[resolution_factor];
}

float
ROP_3Delight::GetSamplingFactor()const
{
	if(!HasSpeedBoost())
	{
		return 1.0f;
	}

	int sampling_factor = evalInt(settings::k_sampling_factor, 0, 0.0f);

	float factors[] = { 1.0f, 0.25f, 0.1f, 0.04f, 0.01f };
	if(sampling_factor < 0 ||
		sampling_factor >= sizeof(factors) / sizeof(factors[0]))
	{
		return 1.0f;
	}

	return factors[sampling_factor];
}

int
ROP_3Delight::GetPixelSamples()const
{
	int pixel_samples = evalInt(settings::k_pixel_samples, 0, 0.0f);
	return pixel_samples;
}

OBJ_Camera*
ROP_3Delight::GetCamera()const
{
	UT_String cam_path;
	evalString(cam_path, settings::k_camera, 0, 0.0f);

	OBJ_Node* obj_node = OPgetDirector()->findOBJNode(cam_path);
	if(!obj_node)
	{
		return nullptr;
	}

	return obj_node->castToOBJCamera();
}

double
ROP_3Delight::GetShutterInterval(double i_time)const
{
	if(!HasMotionBlur())
	{
		return 0.0;
	}

	OBJ_Camera* cam = ROP_3Delight::GetCamera();
	return cam ? camera::get_shutter_duration(*cam, i_time) : 1.0;
}

bool
ROP_3Delight::HasDepthOfField()const
{
	return !(HasSpeedBoost() && evalInt(settings::k_disable_depth_of_field, 0, 0.0f));
}

std::string
ROP_3Delight::GetNSIExportFilename(double i_time)const
{
	UT_String export_mode;
	evalString(export_mode, settings::k_export_nsi, 0, i_time);

	if(export_mode == "off")
	{
		return {};
	}

	if(export_mode == "stdout")
	{
		// A filename of "stdout" actually makes NSI output to standard output
		return export_mode.toStdString();
	}

	UT_String export_file;
	evalString(export_file, settings::k_default_export_nsi_filename, 0, i_time);

	if(export_file.length() == 0)
	{
		// When no file is specified, we output to standard output by default
		return std::string("stdout");
	}

	return export_file.toStdString();
}

void ROP_3Delight::register_mplay_driver( NSI::DynamicAPI &i_api )
{
	decltype( &DspyRegisterDriverTable) register_driver = nullptr;
	i_api.LoadFunction(register_driver, "DspyRegisterDriverTable" );

	if( !register_driver )
	{
		assert( !"Unable to register mplay driver" );
		return;
	}

	PtDspyDriverFunctionTable table;
	memset( &table, 0, sizeof(table) );

	table.Version = k_PtDriverCurrentVersion;
	table.pOpen = &MPlayDspyImageOpen;
	table.pQuery = &MPlayDspyImageQuery;
	table.pWrite = &MPlayDspyImageData;
	table.pClose = &MPlayDspyImageClose;

	register_driver( "mplay", &table );
}
