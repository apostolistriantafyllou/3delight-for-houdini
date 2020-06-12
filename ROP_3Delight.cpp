#include "ROP_3Delight.h"

#include "camera.h"
#include "context.h"
#include "creation_callbacks.h"
#include "exporter.h"
#include "idisplay_port.h"
#include "light.h"
#include "object_attributes.h"
#include "object_visibility_resolver.h"
#include "scene.h"
#include "shader_library.h"
#include "vdb.h"
#include "vop.h"
#include "dl_system.h"
#include "ui/select_layers_dialog.h"

#include <CH/CH_Manager.h>
#include <HOM/HOM_Module.h>
#include <OBJ/OBJ_Camera.h>
#include <OBJ/OBJ_Light.h>
#include <OP/OP_Bundle.h>
#include <OP/OP_BundleList.h>
#include <OP/OP_Director.h>
#include <OP/OP_OperatorTable.h>
#include <MOT/MOT_Director.h>
#include <ROP/ROP_Templates.h>
#include <SYS/SYS_Version.h>
#include <UT/UT_Exit.h>
#include <UT/UT_ReadWritePipe.h>
#include <UT/UT_Spawn.h>
#include <UT/UT_TempFileManager.h>
#include <UT/UT_UI.h>
#include <VOP/VOP_Node.h>

#include <nsi_dynamic.hpp>

#include "delight.h"

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

	/// Opens an NSI file stream for export and outputs header comments
	void InitNSIExport(
		NSI::Context& io_nsi,
		const std::string& i_filename)
	{
		const char* format = i_filename == "stdout" ? "nsi" : "binarynsi";

		// Output NSI commands to the specified file or standard output
		io_nsi.Begin(
		(
			NSI::StringArg("streamfilename", i_filename),
			NSI::CStringPArg("streamformat", format)
		) );

		// Add comments to the NSI stream, useful for debugging
		io_nsi.Evaluate(
		(
			NSI::CStringPArg("1", "Output from 3Delight for Houdini"),
			NSI::CStringPArg(
				"2",
				"Built with HDK " SYS_VERSION_MAJOR "." SYS_VERSION_MINOR
				"." SYS_VERSION_BUILD "." SYS_VERSION_PATCH),
			NSI::StringArg(
				"3",
				"Running on Houdini " + HOM().applicationVersionString())
		) );
	}
}

ROP_3DelightOperator::ROP_3DelightOperator(bool i_cloud)
	:OP_Operator(
		i_cloud ? "3DelightCloud":"3Delight",
		i_cloud ? "3Delight Cloud":"3Delight",
		i_cloud ? ROP_3Delight::cloud_alloc : ROP_3Delight::alloc,
		settings::GetTemplatePair(i_cloud),
		0,
		0,
		settings::GetVariablePair(),
		0u,
		nullptr,
		0,
		"Render"){}

void
ROP_3Delight::Register(OP_OperatorTable* io_table)
{
	ROP_3DelightOperator* rop =
		new ROP_3DelightOperator(false);
	rop->setObsoleteTemplates(settings::GetObsoleteParameters());
	io_table->addOperator(rop);

	ROP_3DelightOperator* cloud_rop =
		new ROP_3DelightOperator(true);
	cloud_rop->setObsoleteTemplates(settings::GetObsoleteParameters());
	io_table->addOperator(cloud_rop);
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
		m_static_nsi(GetNSIAPI()),
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

bool ROP_3Delight::HasMotionBlur( double t) const
{
	return
		evalInt(settings::k_motion_blur, 0, t) &&
		!(HasSpeedBoost(t) && evalInt(settings::k_disable_motion_blur, 0, t));
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
	nsi.Connect(
		k_shader, "",
		k_attributes, "surfaceshader",
		NSI::IntegerArg("strength", 1) );
}

void
ROP_3Delight::ExportGlobals(const context& i_ctx)const
{
	fpreal t = m_current_render->m_current_time;
	int shading_samples = evalInt(settings::k_shading_samples, 0, t);
	shading_samples = int(float(shading_samples) * GetSamplingFactor() + 0.5f);
	int volume_samples = evalInt(settings::k_volume_samples, 0, t);
	i_ctx.m_nsi.SetAttribute(
		".global",
		(
			NSI::IntegerArg("quality.shadingsamples", shading_samples),
			NSI::IntegerArg("quality.volumesamples", volume_samples)
		) );

	int max_diffuse_depth = evalInt(settings::k_max_diffuse_depth, 0, t);
	int max_reflection_depth = evalInt(settings::k_max_reflection_depth, 0, t);
	int max_refraction_depth = evalInt(settings::k_max_refraction_depth, 0, t);
	int max_hair_depth = evalInt(settings::k_max_hair_depth, 0, t);
	i_ctx.m_nsi.SetAttribute(
		".global",
		(
			NSI::IntegerArg("maximumraydepth.diffuse", max_diffuse_depth),
			NSI::IntegerArg("maximumraydepth.reflection", max_reflection_depth),
			NSI::IntegerArg("maximumraydepth.refraction", max_refraction_depth),
			NSI::IntegerArg("maximumraydepth.hair", max_hair_depth)
		) );

	float max_distance = evalInt(settings::k_max_distance, 0, t);
	i_ctx.m_nsi.SetAttribute(
		".global",
		(
			 NSI::DoubleArg( "maximumraylength.specular", max_distance),
			 NSI::DoubleArg( "maximumraylength.diffuse", max_distance ),
			 NSI::DoubleArg( "maximumraylength.reflection", max_distance),
			 NSI::DoubleArg( "maximumraylength.refraction", max_distance),
			 NSI::DoubleArg( "maximumraylength.hair", max_distance)
		) );

	int boost = HasSpeedBoost( i_ctx.m_current_time );

	int toggle = evalInt(settings::k_disable_displacement, 0, t);
	i_ctx.m_nsi.SetAttribute(
		".global", NSI::IntegerArg("show.displacement", !(toggle && boost)));

	toggle = evalInt(settings::k_disable_subsurface, 0, t);
	i_ctx.m_nsi.SetAttribute(
		".global", NSI::IntegerArg("show.osl.subsurface", !(toggle && boost)));

	toggle = evalInt(settings::k_disable_atmosphere, 0, t);
	i_ctx.m_nsi.SetAttribute(
		".global", NSI::IntegerArg("show.atmosphere", !(toggle && boost)));

	toggle = evalFloat(settings::k_disable_multiple_scattering, 0, t);
	i_ctx.m_nsi.SetAttribute(
		".global",
		NSI::DoubleArg("show.multiplescattering",
			(toggle && boost) ? 0.0 : 1.0));
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

unsigned ROP_3Delight::maxInputs() const
{
	return OP_MAX_INDIRECT_INPUTS;
}

unsigned ROP_3Delight::getNumVisibleInputs() const
{
	return OP_MAX_INDIRECT_INPUTS;
}

unsigned ROP_3Delight::maxOutputs() const
{
	return 1;
}

unsigned ROP_3Delight::getNumVisibleOutputs() const
{
	return 1;
}

int ROP_3Delight::startRender(int, fpreal tstart, fpreal tend)
{
	// If we still have a render going on, kill it first.
	StopRender();

	bool render = m_idisplay_rendering || GetNSIExportFilename(tstart).empty();

	/*
		Get the number of frames per second. This is equivalent to	
		OPgetDirector()->getChannelManager()->getSamplesPerSec(), except it's
		not inline, and so doesn't suffer from CH_Manager class layout changes
		between versions.
	*/
	fpreal fps = HOM().fps();

	bool batch = !UTisUIAvailable();
	bool ipr =
		m_idisplay_rendering
		?	m_idisplay_ipr
		:	m_settings.get_render_mode(tstart).toStdString() ==
			settings::k_rm_live_render;

	m_current_render = new context(
		m_settings,
		m_nsi,
		m_static_nsi,
		tstart,
		tend,
		GetShutterInterval(tstart),
		fps,
		HasDepthOfField(tstart),
		batch,
		ipr,
		!render,
		m_cloud,
		getFullPath().toStdString() );

	m_end_time = tend;

	m_rendering = render;

	// Notify the UI that a new render might have started
	m_settings.Rendering(render);

	if(m_current_render->BackgroundProcessRendering())
	{
		// Find the directory containing the renderdl executable
		decltype(&DlGetInstallRoot) get_install_root = nullptr;
		GetNSIAPI().LoadFunction(get_install_root, "DlGetInstallRoot" );
		std::string bin_dir;
		if(get_install_root)
		{
			bin_dir = std::string(get_install_root()) + "/bin/";
		}

		/*
			Start a renderdl process that will start rendering as soon as we
			have exported the first frame of the sequence.
		*/
		m_renderdl = new UT_ReadWritePipe;
		std::string renderdl_command = bin_dir + "renderdl -stdinfiles";

		if(m_cloud)
		{
			renderdl_command += " -cloud -cloudtag HOUDINI";
			if( batch || !m_current_render->SingleFrame() )
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

					delete m_current_render; m_current_render = nullptr;

					m_rendering = false;

					// Notify the UI that rendering has stopped
					m_settings.Rendering(false);

					delete m_renderdl; m_renderdl = nullptr;

					m_render_end_mutex.unlock();
				} );
	}

	/*
		Initialize a file name for the NSI stream receiving non-animated
		attributes.
	*/
	m_static_nsi_file.clear();
	if(m_current_render->m_export_nsi)
	{
		std::string first_frame = GetNSIExportFilename(0.0);
		if(first_frame != "stdout")
		{
			m_static_nsi_file = first_frame + ".static";
		}
	}
	else if(m_renderdl)
	{
		m_static_nsi_file = UT_TempFileManager::getTempFilename();
		m_current_render->m_temp_filenames.push_back( m_static_nsi_file );
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
		InitNSIExport(m_nsi, export_file);
	}
	else if(m_renderdl)
	{
		/*
			Output NSI commands to a temporary file that will be rendered by a
			separate renderdl process.
		*/
		frame_nsi_file = UT_TempFileManager::getTempFilename();
		m_current_render->m_temp_filenames.push_back( frame_nsi_file );

		InitNSIExport(m_nsi, frame_nsi_file);
	}
	else
	{
		// Render directly from the current process
		m_nsi.Begin();
	}

	if(m_static_nsi_file.empty())
	{
		/*
			Redirect static attributes into the main NSI stream if no separate
			stream was opened for them.
		*/
		m_static_nsi.SetHandle(m_nsi.Handle());
	}

	/*
		When exporting the first frame of a sequence, initialize a second
		context for non-animated attributes.
		Note that the file name is always computed when exporting, because it
		will have to be read for all frames.
	*/
	if(!m_static_nsi_file.empty() && time == m_current_render->m_start_time)
	{
		InitNSIExport(m_static_nsi, m_static_nsi_file);
	}

	executePreFrameScript(time);

	object_attributes::export_object_attribute_nodes(*m_current_render);
	ExportTransparentSurface(*m_current_render);

	scene::convert_to_nsi( *m_current_render );

	ExportAtmosphere(*m_current_render);
	ExportOutputs(*m_current_render);

	ExportGlobals(*m_current_render);
	ExportDefaultMaterial(*m_current_render);

	export_render_notes( *m_current_render );

	if(m_current_render->m_ipr)
	{
		// Get notifications for newly created nodes
		creation_callbacks::register_ROP(this);
		// Get notifications for changes to this ROP
		m_current_render->register_interest(this, &ROP_3Delight::changed_cb);
	}

	// Close the static attributes file if one was opened
	if(!m_static_nsi_file.empty() && m_static_nsi.Handle() != NSI_BAD_CONTEXT)
	{
		assert(m_static_nsi.Handle() != m_nsi.Handle());
		m_static_nsi.End();
	}

	// Read the static attributes NSI file from the main stream
	if(!m_static_nsi_file.empty())
	{
		m_nsi.Evaluate(
		(
			NSI::CStringPArg("type", "apistream"),
			NSI::StringArg("filename", m_static_nsi_file)
		) );
	}

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

				if(rop->m_current_render->m_ipr)
				{
					creation_callbacks::unregister_ROP(rop);
				}

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
					// Avoid keeping a reference to a soon invalid context
					rop->m_static_nsi.SetHandle(NSI_BAD_CONTEXT);
					// Close the main rendering context
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

		// The frame has finished exporting or rendering, close the contexts
		
		/*
			This will either close the static NSI file context or simply prevent
			m_static_nsi from keeping a reference to a soon invalid context
			(depending on whether m_static_nsi owns its context or not).
		*/
		m_static_nsi.SetHandle(NSI_BAD_CONTEXT);
		// Close the main rendering/exporting context
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

		// In batch mode, wait for the renderdl process to finish before exiting
		if(!UTisUIAvailable())
		{
			assert(m_renderdl_waiter.joinable());
			m_renderdl_waiter.join();
			assert(!m_renderdl);
		}
	}

	/*
		Destroy the rendering context if rendering/exporting is over.
		If we render in a background thread, the rendering context will be
		destroyed by that thread's stoppedcallback.
		If we render in a separate renderdl process, it will be destroyed by the
		m_renderdl_waiter thread.
		So, we actually only destroy it when exporting an NSI file or rendering
		a single frame in batch mode, which doesn't occur in a separate thread.
	*/
	m_render_end_mutex.lock();
	if(m_current_render &&
		!m_current_render->BackgroundThreadRendering() &&
		!m_current_render->BackgroundProcessRendering())
	{
		delete m_current_render; m_current_render = nullptr;
		m_rendering = false;
		m_settings.Rendering(false);
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

	return changed;
}

void
ROP_3Delight::loadFinished()
{
	ROP_Node::loadFinished();

	// Ensure that the ROP's initial rendering state is reflected in the UI
	m_settings.Rendering(false);
}

void
ROP_3Delight::resolveObsoleteParms(PRM_ParmList* i_old_parms)
{
	resolve_obsolete_render_mode(i_old_parms);
}

void
ROP_3Delight::resolve_obsolete_render_mode(PRM_ParmList* i_old_parms)
{
	PRM_Parm* export_nsi = i_old_parms->getParmPtr(settings::k_old_export_nsi);
	if(export_nsi)
	{
		UT_String export_mode;
		i_old_parms->evalString(export_mode, settings::k_old_export_nsi, 0, 0.0);
		if(export_mode == "stdout")
		{
			setString(
				settings::k_rm_export_stdout,
				CH_STRING_LITERAL,
				settings::k_render_mode,
				0, 0.0f);
			return;
		}
		if(export_mode != "off")
		{
			setString(
				settings::k_rm_export_file,
				CH_STRING_LITERAL,
				settings::k_render_mode,
				0, 0.0f);
			return;
		}
	}

	PRM_Parm* ipr = i_old_parms->getParmPtr(settings::k_old_ipr);
	if(ipr)
	{
		if(i_old_parms->evalInt(settings::k_old_ipr, 0, 0.0))
		{
			setString(
				settings::k_rm_live_render,
				CH_STRING_LITERAL,
				settings::k_render_mode,
				0, 0.0f);
			return;
		}
	}
}

void ROP_3Delight::changed_cb(
	OP_Node* i_caller,
	void* i_callee,
	OP_EventType i_type,
	void* i_data)
{
	if(i_type != OP_PARM_CHANGED)
	{
		return;
	}

	ROP_3Delight* rop = dynamic_cast<ROP_3Delight*>(i_caller);
	assert(rop);

	int parm_index = reinterpret_cast<intptr_t>(i_data);
	if( parm_index == -1 )
	{
		/* Happens with File -> Save AS and Auto-Save */
		return;
	}

	PRM_Parm& parm = rop->getParm(parm_index);
	std::string name = parm.getToken();

	if(name == settings::k_atmosphere)
	{
		context* ctx = (context*)i_callee;
		rop->ExportAtmosphere(*ctx, true);
		ctx->m_nsi.RenderControl(
			NSI::CStringPArg("action", "synchronize"));
	}
}

/**
	This is needed by the Spatial Override feature to make the overriding
	object transparent.
*/
void ROP_3Delight::ExportTransparentSurface(const context& i_ctx) const
{
	std::string shaderHandle = exporter::transparent_surface_handle();
	shaderHandle += "|shader";

	NSI::Context& nsi = i_ctx.m_nsi;
	nsi.Create(exporter::transparent_surface_handle(), "attributes");
	nsi.Create(shaderHandle, "shader");

	const shader_library& library = shader_library::get_instance();
	std::string path = library.get_shader_path( "transparent" );

	nsi.SetAttribute(
		shaderHandle,
		NSI::StringArg("shaderfilename", path) );

	nsi.Connect(
		shaderHandle, "",
		exporter::transparent_surface_handle(), "surfaceshader",
		(
			NSI::IntegerArg("priority", 60),
			NSI::IntegerArg("strength", 1)
		) );
}

void
ROP_3Delight::ExportAtmosphere(const context& i_ctx, bool ipr_update)
{
	VOP_Node* atmo_vop =
		exporter::resolve_material_path(
			this,
			m_settings.GetAtmosphere(i_ctx.m_current_time).c_str());

	std::string env_handle = "atmosphere|environment";
	
	if(!atmo_vop)
	{
		if(ipr_update)
		{
			i_ctx.m_nsi.Delete(env_handle, NSI::IntegerArg("recursive", 1));
		}
		return;
	}

	if(ipr_update)
	{
		// Ensure that the shader exists before connecting to it
		std::unordered_set<std::string> mat;
		mat.insert(atmo_vop->getFullPath().toStdString());
		scene::export_materials(mat, i_ctx);
	}

	std::string attr_handle = "atmosphere|attributes";

	i_ctx.m_nsi.Create( env_handle, "environment" );
	i_ctx.m_nsi.Connect( env_handle, "", NSI_SCENE_ROOT, "objects" );

	i_ctx.m_nsi.Create( attr_handle, "attributes" );

	i_ctx.m_nsi.Connect(
		vop::handle(*atmo_vop, i_ctx), "",
		attr_handle, "volumeshader",
		NSI::IntegerArg("strength", 1) );

	i_ctx.m_nsi.Connect(
		attr_handle, "",
		env_handle, "geometryattributes" );
}

void
ROP_3Delight::ExportOutputs(const context& i_ctx)const
{
	OBJ_Camera* cam = GetCamera( i_ctx.m_current_time );

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
			NSI::IntegerArg("oversampling", GetPixelSamples()),
			NSI::FloatArg( "pixelaspectratio", cam->ASPECT(current_time))
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
			float(cam->CROPL(0)), 1.0f - float(cam->CROPT(0)),
			float(cam->CROPR(0)), 1.0f - float(cam->CROPB(0))
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
		camera::handle(*cam, i_ctx), "screens");

	UT_String idisplay_driver = "idisplay";
	UT_String file_driver;
	evalString(
		file_driver,
		settings::k_default_image_format, 0,
		current_time );

	std::string png_driver = "png";
	std::string jpeg_driver = "jpeg";

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
		int mode = evalInt(settings::k_batch_output_mode, 0, current_time);
		if (mode == 0) output_mode = e_useToggleStates;
		else output_mode = e_allFilesAndSelectedJpeg;
	}
	else
	{
		int mode = evalInt(settings::k_interactive_output_mode, 0, current_time);
		if (mode == 0) output_mode = e_useToggleStates;
		else if (mode == 1) output_mode = e_useToggleAndFramebufferStates;
	}

	int nb_aovs = evalInt(settings::k_aov, 0, current_time);
	unsigned sort_key = 0;

	UT_String scalar_format;
	evalString(
		scalar_format,
		settings::k_default_image_bits, 0, current_time );

	UT_String filter;
	evalString(filter, settings::k_pixel_filter, 0, current_time);
	double filter_width = evalFloat(settings::k_filter_width, 0, current_time);

	std::map<std::string, std::vector<OBJ_Node*>> light_categories;

	// Create a category with empty name and empty list (which means ALL lights)
	light_categories[std::string()];

	BuildLightCategories( light_categories, current_time );

	bool has_frame_buffer = false;
	for (int i = 0; i < nb_aovs; i++)
	{
		UT_String label;
		evalString(label, aov::getAovStrToken(i), 0, current_time );

		const aov::description& desc = aov::getDescription(label.toStdString());

		bool idisplay_output =
			!i_ctx.m_batch && !i_ctx.m_export_nsi &&
			evalInt(aov::getAovFrameBufferOutputToken(i), 0, current_time) != 0;
		bool file_output = evalInt(aov::getAovFileOutputToken(i), 0, current_time);
		bool png_output = file_output;
		file_output = file_output && file_driver.toStdString() != "png";
		png_output = png_output && file_driver.toStdString() == "png";
		bool jpeg_output = evalInt(aov::getAovJpegOutputToken(i), 0, current_time);

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
			nb_light_categories = light_categories.size();
		}

		assert( nb_light_categories>0 );

		int j = -1;
		for( auto &category : light_categories )
		{
			j++;
			std::string layer_name = prefix;
			layer_name += "-";
			layer_name += desc.m_filename_token;

			if( !category.first.empty() )
			{
				layer_name += "-";
				layer_name += category.first;
			}

			if (idisplay_output)
			{
				has_frame_buffer = true;

				if (idisplay_driver_name.empty())
				{
					idisplay_driver_name = "idisplay_driver";
					i_ctx.m_nsi.Create(idisplay_driver_name, "outputdriver");
					i_ctx.m_nsi.SetAttribute(
						idisplay_driver_name,
						(
							NSI::StringArg("drivername", idisplay_driver),
							NSI::StringArg("imagefilename", image_display_name),
							NSI::CStringPArg("ropname", getFullPath().c_str())
						) );
				}

				ExportOneOutputLayer(
					i_ctx, layer_name, desc, scalar_format,
					filter, filter_width,
					k_screen_name, category.first,
					idisplay_driver_name, idisplay_driver.toStdString(),
					sort_key );

				if( m_current_render->m_export_nsi ||
					m_current_render->m_batch )
				{
					continue;
				}

				/*
					3Delight Display's Multi-Light tool needs some information,
					called "feedback data" to communicate back the values.
				*/
				if( category.second.size() > 1 ||
					category.second.size() == 1 &&
						light::handle(*category.second[0], i_ctx) != category.first)
				{
					/*
						We use a set to group lights together under the same
						layer. This is also useful when the light category name
						is different than a single light's handle (this occurs
						with bundles containing only one light or single lights
						in IPR mode, which uses different handles).
					*/
					/*
						FIXME : exporting the lightsets should always be done,
						even if idisplay_output, m_current_render->m_export_nsi
						or m_current_render->m_batch is true. Only the layer
						feedback data could be omitted when not sending the
						render to i-display. 
					*/
					/*
						FIXME : there is no reason to export those sets set once
						for each AOV. Also, they should be exported *before* we
						try to connect them to the layers in ExportOneOutputLayer.
						This part could be moved out of the loop. 
					*/
					i_ctx.m_nsi.Create( category.first, "set");
					for( auto &light_source : category.second )
					{
						std::string light_handle = light::handle(*light_source, i_ctx);
						/*
							FIXME : calling ExportLayerFeedbackData in a loop
							with the same layer_name will simply overwrite the
							"feedbackdata" attribute of the layer.  Only the
							last light will end up in the attribute.
						*/
						ExportLayerFeedbackData(
							i_ctx, layer_name, light_handle );
						i_ctx.m_nsi.Connect( light_handle, "",
							category.first, "members" );
					}
				}
				else
				{
					std::string light_handle =
						category.second.empty()
						?	std::string()
						:	light::handle(*category.second.back(), i_ctx);
					ExportLayerFeedbackData(
						i_ctx, layer_name, light_handle );
				}
			}

			if (file_output)
			{
				std::string file_layer_name = layer_name + "_file";

				if (file_driver_name.empty())
				{
					file_driver_name = "file_driver";
					i_ctx.m_nsi.Create(file_driver_name, "outputdriver");
					i_ctx.m_nsi.SetAttribute(
						file_driver_name,
					(
						NSI::CStringPArg("drivername", file_driver.c_str()),
						NSI::StringArg("imagefilename", image_file_name)
					) );
				}

				ExportOneOutputLayer(
					i_ctx, file_layer_name, desc, scalar_format,
					filter, filter_width,
					k_screen_name, category.first,
					file_driver_name, file_driver.toStdString(),
					sort_key);
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
					image_file_name, category.first,
					desc.m_filename_token, ".png", image_png_name);

				i_ctx.m_nsi.Create(png_driver_name, "outputdriver");
				i_ctx.m_nsi.SetAttribute(
					png_driver_name,
					(
						NSI::StringArg("drivername", png_driver),
						NSI::StringArg("imagefilename", image_png_name)
					) );

				ExportOneOutputLayer(
					i_ctx, png_layer_name, desc, "uint8",
					filter, filter_width,
					k_screen_name, category.first,
					png_driver_name, png_driver,
					sort_key);
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
					image_file_name, category.first,
					desc.m_filename_token, ".jpg", image_jpeg_name);

				i_ctx.m_nsi.Create(jpeg_driver_name, "outputdriver");
				i_ctx.m_nsi.SetAttribute(
					jpeg_driver_name,
				(
					NSI::StringArg("drivername", jpeg_driver),
					NSI::StringArg("imagefilename", image_jpeg_name)
				) );

				ExportOneOutputLayer(
					i_ctx, jpeg_layer_name, desc, "uint8",
					filter, filter_width,
					k_screen_name, category.first,
					jpeg_driver_name, jpeg_driver,
					sort_key);
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

	/* Don't take too much CPU if the Houdini UI is present */
	if( UTisUIAvailable() )
	{
		i_ctx.m_nsi.SetAttribute(
			NSI_SCENE_GLOBAL, NSI::IntegerArg("renderatlowpriority", 1) );
	}

	/* Set frame, it is reponsible of per frame noise patterns. */
	i_ctx.m_nsi.SetAttribute(
		NSI_SCENE_GLOBAL, NSI::DoubleArg("frame", i_ctx.m_current_time) );
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
	const std::string& i_light_category,
	const std::string& i_driver_handle,
	const std::string& i_driver_name,
	unsigned& io_sort_key) const
{
	i_ctx.m_nsi.Create(i_layer_handle, "outputlayer");
	i_ctx.m_nsi.SetAttribute(
		i_layer_handle,
		(
			NSI::StringArg("variablename", i_desc.m_variable_name),
			NSI::StringArg("variablesource", i_desc.m_variable_source),
			NSI::CStringPArg("scalarformat", i_scalar_format.c_str()),
			NSI::StringArg("layertype", i_desc.m_layer_type),
			NSI::IntegerArg("withalpha", (int)i_desc.m_with_alpha),
			NSI::IntegerArg("sortkey", io_sort_key++)
		) );

	if(i_desc.m_variable_name == "relighting_multiplier")
	{
		i_ctx.m_nsi.SetAttribute(
			i_layer_handle,
			(
				NSI::CStringPArg("filter", "box"),
				NSI::DoubleArg("filterwidth", 1.0)
			) );
		// Setting "maximumvalue" is probably not a good idea in this case
	}
	else
	{
		i_ctx.m_nsi.SetAttribute(
			i_layer_handle,
			(
				NSI::CStringPArg("filter", i_filter.c_str()),
				NSI::DoubleArg("filterwidth", i_filter_width)
			) );

		if(i_desc.m_layer_type == "color")
		{
			/*
				Use 3Delight's fancy tonemapping technique. This allows
				rendering of very high dynamic range images with no ugly
				aliasing on the edges (think about area lights).
				The value here can really be very high and things will still
				work fine. Note that this also eliminates some fireflies.
			*/
			i_ctx.m_nsi.SetAttribute(
				i_layer_handle,
				NSI::DoubleArg("maximumvalue", 50));
		}
	}

	if( i_desc.m_variable_name == "Ci" )
	{
		/* We only draw outlines on "Ci" */
		i_ctx.m_nsi.SetAttribute( i_layer_handle,
			NSI::IntegerArg( "drawoutlines", 1 ) );
	}

	if (i_scalar_format == "uint8")
	{
		i_ctx.m_nsi.SetAttribute(i_layer_handle,
			NSI::StringArg("colorprofile", "srgb"));
	}

	// Decide whether to output ID AOVs in Cryptomatte format.
	unsigned cryptomatte_layers = 0;
	if(i_desc.m_variable_name.substr(0, 3) == "id." &&
		i_desc.m_variable_source == "builtin" &&
		i_driver_name == "exr")
	{
		cryptomatte_layers = 2;
	}

	i_ctx.m_nsi.Connect(
		i_layer_handle, "",
		i_screen_handle, "outputlayers");

	if (!i_light_category.empty())
	{
		i_ctx.m_nsi.Connect(
			i_light_category, "", i_layer_handle, "lightset");
	}

	i_ctx.m_nsi.Connect(
		i_driver_handle, "",
		i_layer_handle, "outputdrivers");

	if(cryptomatte_layers > 0)
	{
		// Change the filter and output type to fit the Cryptomatte format
		i_ctx.m_nsi.SetAttribute(
			i_layer_handle,
			(
				NSI::StringArg( "layertype", "color" ),
				NSI::StringArg( "scalarformat", "float" ),
				NSI::IntegerArg( "withalpha", 0 ),
				NSI::StringArg( "filter", "cryptomatteheader" )
			) );

		/*
			Export one additional layer per Cryptomatte level. Each will
			output 2 values from those present in each pixel's samples.
		*/
		for(unsigned cl = 0; cl < cryptomatte_layers; cl++)
		{
			std::string cl_handle = i_layer_handle + std::to_string(cl);
			i_ctx.m_nsi.Create(cl_handle, "outputlayer");

			std::string cl_filter = "cryptomattelayer" + std::to_string(cl*2);
			i_ctx.m_nsi.SetAttribute(
				cl_handle,
				(
					NSI::StringArg( "variablename", i_desc.m_variable_name ),
					NSI::StringArg( "layertype", "quad" ),
					NSI::StringArg( "scalarformat", "float" ),
					NSI::IntegerArg( "withalpha", 0 ),
					NSI::StringArg( "filter", cl_filter ),
					NSI::DoubleArg( "filterwidth", i_filter_width ),
					NSI::IntegerArg( "sortkey", io_sort_key++ ),
					NSI::StringArg( "variablesource", "builtin" )
				) );

			i_ctx.m_nsi.Connect(cl_handle, "", i_screen_handle, "outputlayers");

			i_ctx.m_nsi.Connect(i_driver_handle, "", cl_handle, "outputdrivers");
		}
	}
}

void
ROP_3Delight::ExportLayerFeedbackData(
	const context& i_ctx,
	const std::string& i_layer_handle,
	const std::string& i_light_path) const
{
	idisplay_port *idp = idisplay_port::get_instance();

	std::string host = idp->GetServerHost();
	int port = idp->GetServerPort();

	if( i_light_path.empty() )
	{
		i_ctx.m_nsi.SetAttribute( i_layer_handle,
			(
				NSI::StringArg( "sourceapp", "Houdini" ),
				NSI::StringArg( "feedbackhost", host ),
				NSI::IntegerArg( "feedbackport", port )
			));
		return;
	}

	OBJ_Node* obj_node = OPgetDirector()->findOBJNode(i_light_path.c_str());
	assert(obj_node);

	if( !obj_node )
		return;

	fpreal r = 1.0f;
	fpreal g = 1.0f;
	fpreal b = 1.0f;
	fpreal intensity = 1.0f;

	double time = i_ctx.m_current_time;
	if( obj_node->getParmIndex("light_color")>=0 )
	{
		r = obj_node->evalFloat( "light_color", 0, time );
		g = obj_node->evalFloat( "light_color", 1, time );
		b = obj_node->evalFloat( "light_color", 2, time );
	}

	if( obj_node->getParmIndex("light_intensity")>=0 )
	{
		intensity = obj_node->evalFloat( "light_intensity", 0, time );
	}

	std::string values;
	std::stringstream ss1;
	ss1 << intensity;
	values = ss1.str();

	std::string color_values;
	std::stringstream ss2;
	ss2 << r << "," << g << "," << b;
	color_values = ss2.str();

	std::string light_type = "don't care";
	std::string feedback_data;

	/* Make the feedback message */
	/* Name */
	feedback_data = "{\"name\":\"";
	feedback_data += i_light_path;
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

	i_ctx.m_nsi.SetAttribute( i_layer_handle,
		(
			NSI::StringArg( "sourceapp", "Houdini" ),
			NSI::StringArg( "feedbackhost", host ),
			NSI::IntegerArg( "feedbackport", port ),
			NSI::StringArg( "feedbackdata", feedback_data )
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

/**
	\brief Output light categories for light bundles and single lights.

	We output a light category for each bundle of lights. Lights that are alone
	will be in their own category. Incandescence lights and VDBs cannot be put
	into bundles. Doesn't make sense for Incandescent and 3Delight NSI doesn't
	support grouping them yet.
*/
void ROP_3Delight::BuildLightCategories(
	std::map<std::string, std::vector<OBJ_Node*>>& o_light_categories,
	fpreal t ) const
{
	std::vector<OBJ_Node*> i_lights;
	m_settings.GetLights( i_lights, t );

	if( i_lights.empty() )
		return;

	OP_BundleList* blist = OPgetDirector()->getBundles();
	assert(blist);
	int numBundles = blist->entries();

	for( auto light_source : i_lights )
	{
		bool foundInBundle = false;
		bool incand =
			light_source->getOperator()->getName() ==
			"3Delight::IncandescenceLight";
		bool isvdb = light_source->castToOBJLight() == nullptr;

		for (int i = 0; !incand && !isvdb && i < numBundles; i++)
		{
			OP_Bundle* bundle = blist->getBundle(i);
			assert(bundle);
			if( bundle && bundle->contains(light_source, false) )
			{
				std::string bundle_name = bundle->getName();
				o_light_categories[bundle_name].push_back(light_source);
				foundInBundle = true;
			}
		}

		if( !foundInBundle )
		{
			/* Make a category for this single light */
			std::string category = light_source->getFullPath().toStdString();
			o_light_categories[category].push_back(light_source);
		}
	}
}

/*
	\brief returns true if speed boost is enabled for this render and this
	particular rendering mode.

	Speed boost is only active for an interactive render. We also make an
	exception when outputting to stdout as this output is a purely debugging
	output and is only done interactively.
*/
bool ROP_3Delight::HasSpeedBoost( double t )const
{
	bool batch = !UTisUIAvailable();
	if(batch)
	{
		return false;
	}

	std::string render_mode = m_settings.get_render_mode(t).toStdString();
	if(render_mode == settings::k_rm_export_file )
		return false;

	return evalInt(settings::k_speed_boost, 0, t);
}

bool
ROP_3Delight::GetScaledResolution(int& o_x, int& o_y)const
{
	fpreal t = m_current_render->m_current_time;
	OBJ_Camera* cam = GetCamera( t );

	if(!cam)
	{
		return false;
	}

	float scale = GetResolutionFactor();

	o_x = int(::roundf(cam->RESX(t)*scale));
	o_y = int(::roundf(cam->RESY(t)*scale));

	return true;
}

float
ROP_3Delight::GetResolutionFactor()const
{
	fpreal t = m_current_render->m_current_time;

	if(!HasSpeedBoost(t))
	{
		return 1.0f;
	}

	int resolution_factor = evalInt(settings::k_resolution_factor, 0, t);

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
	fpreal t = m_current_render->m_current_time;
	if( !HasSpeedBoost(t) )
	{
		return 1.0f;
	}

	int sampling_factor = evalInt(settings::k_sampling_factor, 0, t);

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
	fpreal t = m_current_render->m_current_time;
	int pixel_samples = evalInt(settings::k_pixel_samples, 0, t);
	return pixel_samples;
}

OBJ_Camera*
ROP_3Delight::GetCamera( double t )const
{
	UT_String cam_path;
	evalString(cam_path, settings::k_camera, 0, t);

	OBJ_Node* obj_node = OPgetDirector()->findOBJNode(cam_path);
	if(!obj_node)
	{
		obj_node = findOBJNode( cam_path );
		if( !obj_node )
			return nullptr;
	}

	return obj_node->castToOBJCamera();
}

double
ROP_3Delight::GetShutterInterval(double i_time)const
{
	if(!HasMotionBlur(i_time))
	{
		return 0.0;
	}

	OBJ_Camera* cam = ROP_3Delight::GetCamera(i_time);
	return cam ? camera::get_shutter_duration(*cam, i_time) : 1.0;
}

bool
ROP_3Delight::HasDepthOfField( double t )const
{
	return !(HasSpeedBoost(t) && evalInt(settings::k_disable_depth_of_field, 0, t));
}

std::string
ROP_3Delight::GetNSIExportFilename(double i_time)const
{
	std::string render_mode = m_settings.get_render_mode(i_time).toStdString();

	if(render_mode == settings::k_rm_render ||
		render_mode == settings::k_rm_live_render)
	{
		return {};
	}

	if(render_mode == settings::k_rm_export_stdout)
	{
		// A filename of "stdout" actually makes NSI output to standard output
		return std::string("stdout");
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

void ROP_3Delight::NewOBJNode(OBJ_Node& i_node)
{
	scene::insert_obj_node(i_node, *m_current_render);
	m_nsi.RenderControl(NSI::CStringPArg("action", "synchronize"));
}

/**
	\brief Sets the differents IDs needed for 3Delight Cloud.

	notes
		Is needed on 3delight.com -> 3Delight Cloud -> Activities

	frameid and project
		Both are passed back to 3Delight Display and shown in the 3Delight
		Cloud Dashboard.

	ATTENTION: do not do arbitrary changes to formatting as this might affect
	the layout of the UI and website.
*/
void ROP_3Delight::export_render_notes( const context &i_context ) const
{
	MOT_Director *mot = dynamic_cast<MOT_Director *>( OPgetDirector() );

	std::string scene_name = mot->getFileName().toStdString();
	std::string project{ mot->getFileName().fileName() };

	int frame = OPgetDirector()->getChannelManager()->getFrame(
		i_context.m_current_time );

	char frameid[5] = {0};
	snprintf( frameid, 5, "%04d", frame );

	i_context.m_nsi.SetAttribute(
		NSI_SCENE_GLOBAL,
		(
			NSI::CStringPArg("statistics.frameid", frameid),
			NSI::StringArg("statistics.project", project)
		) );

	if( i_context.m_export_nsi )
	{
		/* renderdl will inject its own notes. */
		return;
	}

	std::string notes = scene_name.empty() ? "No scene name" : scene_name;
	notes += "\n";
	notes += i_context.m_rop_path;
	notes += "  |  ";
	notes += frameid;

	i_context.m_nsi.SetAttribute(
		NSI_SCENE_GLOBAL,
		(
			NSI::StringArg("statistics.notes", notes)
		) );
}

double ROP_3Delight::current_time( void ) const
{
	return m_current_render ? m_current_render->m_current_time : 0.0;
}

bool ROP_3DelightOperator::getOpHelpURL(UT_String& url)
{
	std::string url_name = dl_system::delight_doc_url()+"The+3Delight+ROP";
	url.hardenIfNeeded(url_name.c_str());
	return true;
}
