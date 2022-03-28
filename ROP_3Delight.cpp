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
#include "time_notifier.h"
#include "vdb.h"
#include "viewport_hook.h"
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

	const std::string k_stdout = "stdout";

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
		const char* format = i_filename == k_stdout ? "nsi" : "autonsi";

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

ROP_3DelightOperator::ROP_3DelightOperator(rop_type i_rop_type)
	:OP_Operator(
		i_rop_type == rop_type::cloud ? "3DelightCloud":
		i_rop_type == rop_type::stand_in ? "3DelightStandin":
		i_rop_type == rop_type::standard ? "3Delight":"3DelightViewport",

		i_rop_type == rop_type::cloud ? "3Delight Cloud":
		i_rop_type == rop_type::stand_in ? "3Delight Standin":
		i_rop_type == rop_type::standard ? "3Delight":"3Delight Viewport",

		i_rop_type == rop_type::cloud ? ROP_3Delight::cloud_alloc :
		i_rop_type == rop_type::stand_in ? ROP_3Delight::standin_alloc:
		i_rop_type == rop_type::standard ? ROP_3Delight::alloc : ROP_3Delight::viewport_alloc,
		settings::GetTemplatePair(i_rop_type),
		0,
		OP_MULTI_INPUT_MAX,
		settings::GetVariablePair(),
		0u,
		nullptr,
		1,
		"Render"){}

void
ROP_3Delight::Register(OP_OperatorTable* io_table)
{
	ROP_3DelightOperator* rop =
		new ROP_3DelightOperator(rop_type::standard);
	rop->setObsoleteTemplates(settings::GetObsoleteParameters());
	io_table->addOperator(rop);

	ROP_3DelightOperator* cloud_rop =
		new ROP_3DelightOperator(rop_type::cloud);
	cloud_rop->setObsoleteTemplates(settings::GetObsoleteParameters());
	io_table->addOperator(cloud_rop);

	ROP_3DelightOperator* standin_rop =
		new ROP_3DelightOperator(rop_type::stand_in);
	standin_rop->setObsoleteTemplates(settings::GetObsoleteParameters());
	io_table->addOperator(standin_rop);

	ROP_3DelightOperator* viewport_rop =
		new ROP_3DelightOperator(rop_type::viewport);
	viewport_rop->setObsoleteTemplates(settings::GetObsoleteParameters());
	io_table->addOperator(viewport_rop);
}

OP_Node*
ROP_3Delight::alloc(OP_Network* net, const char* name, OP_Operator* op)
{
	return new ROP_3Delight(net, name, op, rop_type::standard);
}

OP_Node*
ROP_3Delight::cloud_alloc(OP_Network* net, const char* name, OP_Operator* op)
{
	return new ROP_3Delight(net, name, op, rop_type::cloud);
}

OP_Node*
ROP_3Delight::standin_alloc(OP_Network* net, const char* name, OP_Operator* op)
{
	return new ROP_3Delight(net, name, op, rop_type::stand_in);
}

OP_Node*
ROP_3Delight::viewport_alloc(OP_Network* net, const char* name, OP_Operator* op)
{
	return new ROP_3Delight(net, name, op, rop_type::viewport);
}

ROP_3Delight::ROP_3Delight(
	OP_Network* net,
	const char* name,
	OP_Operator* entry,
	rop_type i_rop_type)
	:	ROP_Node(net, name, entry),
		m_rop_type(i_rop_type),
		m_current_render(nullptr),
		m_end_time(0.0),
		m_nsi(GetNSIAPI()),
		m_static_nsi(GetNSIAPI()),
		m_time_notifier(nullptr),
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

void ROP_3Delight::UpdateCrop(const float* i_window)
{
	assert(i_window);

	m_is_cropped_on_iDisplay = true;

	m_render_end_mutex.lock();
	if(m_rendering && m_current_render && m_current_render->m_ipr)
	{
		OBJ_Camera *cam = GetCamera(m_current_render->m_current_time);
		assert( cam );
		if( !cam )
			return;

		std::string screen_name(camera::screen_handle(cam, *m_current_render));

		memcpy(
			m_idisplay_rendering_window,
			i_window,
			sizeof(m_idisplay_rendering_window));

		m_nsi.SetAttribute(
			screen_name,
			*NSI::Argument::New("crop")
				->SetArrayType(NSITypeFloat, 2)
				->SetCount(2)
				->SetValuePointer(i_window));

		m_nsi.RenderControl(NSI::CStringPArg("action", "synchronize"));
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
	// Exporting this will assign a surface shader to environmetns as well
	// which is not good. FIXME: should we do something "smarter"
	return;

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

	if( hasParm(settings::k_clamp_value) )
	{
		i_ctx.m_nsi.SetAttribute(
		".global",
		(
			NSI::DoubleArg("clampindirect",
				evalFloat(settings::k_clamp_value,0.0f,t))
		) );
	}

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

		if( m_sequence_context )
		{
			m_sequence_context->RenderControl(
				NSI::CStringPArg( "action", "stop") );
			/*
				The ->End will be called in m_sequence_waiter
			*/
			//m_sequence_context.reset();

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
		Wait for m_sequence_waiter to finish. This has to be done even if we
		didn't explicitly terminate the renderdl process (ie : if it has
		finished on its own), because we don't want	the thread hanging loose.
	*/
	if(m_sequence_waiter.joinable())
	{
		m_sequence_waiter.join();
	}

	// Notify the UI that rendering has stopped
	m_settings.Rendering(false,false);
}

unsigned ROP_3Delight::maxInputs() const
{
	return OP_MULTI_INPUT_MAX;
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

	std::string export_filename = GetNSIExportFilename(tstart);
	bool render = m_idisplay_rendering || export_filename.empty();

	/*
		Get the number of frames per second. This is equivalent to
		OPgetDirector()->getChannelManager()->getSamplesPerSec(), except it's
		not inline, and so doesn't suffer from CH_Manager class layout changes
		between versions.
	*/
	fpreal fps = HOM().fps();

	bool batch = !UTisUIAvailable();
	bool ipr = false;
	/*
		Avoid evaluating ipr parameter for standin ROP which would
		produce an error message on console.
	*/
	if (m_rop_type != rop_type::stand_in)
	{
		ipr =
			m_idisplay_rendering
			? m_idisplay_ipr
			: evalInt(settings::k_ipr_start, 0, tstart)
			|| m_rop_type == rop_type::viewport;
	}

	// An actual path in the file system where the scene description is exported
	std::string export_path =
		!render && export_filename != k_stdout
		?	export_filename
		:	std::string();

	m_current_render = new context(
		this,
		m_settings,
		m_nsi,
		m_static_nsi,
		tstart,
		tend,
		GetShutterInterval(tstart),
		GetShutterOffset(tstart),
		fps,
		HasDepthOfField(tstart),
		batch,
		ipr,
		!render,
		export_path,
		m_rop_type);

	m_end_time = tend;

	m_rendering = render;

	// Notify the UI that a new render might have started
	m_settings.Rendering(render,ipr);

	if(m_current_render->BackgroundProcessRendering())
	{
		bool batch_tag = batch || !m_current_render->SingleFrame();

		m_sequence_context.reset(new NSI::Context( GetNSIAPI()) );

		if( m_rop_type == rop_type::cloud )
		{
			m_sequence_context->Begin((
				NSI::IntegerArg("createsequencecontext", 1),
				NSI::StringArg("software", batch_tag ? "HOUDINI_BATCH" : "HOUDINI"),
				NSI::IntegerArg("cloud", 1)) );
		}
		else
		{
			m_sequence_context->Begin((
				NSI::IntegerArg("createsequencecontext", 1),
				NSI::StringArg("software", batch_tag ? "HOUDINI_BATCH" : "HOUDINI"),
				NSI::IntegerArg("readpreferences", batch ? 0 : 1)) );
		}

		/*
			Start a thread that will wait for the quence process to finish
		*/
		m_sequence_waiter =
			std::thread(
				[this]()
				{
					m_sequence_context->RenderControl(
						NSI::StringArg("action", "wait") );
					m_sequence_context->End();
					m_sequence_context.reset();

					m_render_end_mutex.lock();

					delete m_current_render; m_current_render = nullptr;

					m_rendering = false;

					// Notify the UI that rendering has stopped
					m_settings.Rendering(false,false);

					m_render_end_mutex.unlock();
				} );

	}

	/*
		Initialize a file name for the NSI stream receiving non-animated
		attributes.
	*/

	m_static_nsi_file.clear();
	if(m_current_render->m_export_nsi &&
		m_current_render->m_rop_type != rop_type::stand_in)
	{
		std::string first_frame =
			GetNSIExportFilename( m_current_render->m_start_time );
		if(first_frame != k_stdout)
		{
			/*
				Add ".static" in the file name, but try to preserve its
				extension. This also works for ".nsia", which is important.
			*/
			std::string extension;
			std::string::size_type extension_index = first_frame.rfind(".nsi");
			if(extension_index != std::string::npos)
			{
				extension = first_frame.substr(extension_index);
				first_frame.resize(extension_index);
			}

			m_static_nsi_file = first_frame + ".static" + extension;
		}
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

	m_current_render->set_current_time(time);

	std::string frame_nsi_file;
	if(m_current_render->m_export_nsi)
	{
		std::string export_file = GetNSIExportFilename(time);
		assert(!export_file.empty());
		m_current_render->set_export_path(export_file);
		InitNSIExport(m_nsi, export_file);
	}
	else if(m_sequence_context)
	{
		/*
			When exporting a sequence, the frames are tied to a sequence context.
			3Delight takes care of storing, rendering, and deleting them.
		*/
		assert( m_sequence_context );
		m_nsi.Begin( NSI::IntegerArg(
			"sequencecontext", m_sequence_context->Handle()) );
	}
	else
	{
		NSI::ArgumentList argList;

		if( m_current_render->m_ipr )
			argList.Add( new NSI::CStringPArg( "software", "HOUDINI_LIVE" ) );
		else
			argList.Add( new NSI::CStringPArg( "software", "HOUDINI" ) );


		argList.Add( new NSI::IntegerArg( "readpreferences", 1  ) );

		// Render directly from the current process
		m_nsi.Begin( argList );
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

	scene::convert_to_nsi(*m_current_render, true);

	if(m_current_render->m_rop_type != rop_type::stand_in)
	{
		ExportDefaultMaterial(*m_current_render);
		if(m_current_render->m_rop_type == rop_type::viewport)
		{
			viewport_hook_builder::instance().connect(&m_nsi);
			m_nsi.SetAttribute(
				NSI_SCENE_GLOBAL,
				NSI::CStringPArg("bucketorder", "circle"));
		}
		else
		{
			ExportAtmosphere(*m_current_render);
			ExportOutputs(*m_current_render);
		}
		ExportGlobals(*m_current_render);
		export_render_notes( *m_current_render );
	}

	if(m_current_render->m_ipr)
	{
		// Get notifications for newly created nodes
		creation_callbacks::register_ROP(this);
		// Get notifications for changes to this ROP
		m_current_render->register_interest(this, &ROP_3Delight::changed_cb);
		m_time_notifier =
			new time_notifier(
				[this](double time) { time_change_cb(time); },
				true);
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
					delete rop->m_time_notifier; rop->m_time_notifier = nullptr;
					creation_callbacks::unregister_ROP(rop);

					if(rop->m_current_render->m_rop_type == rop_type::viewport)
					{
						viewport_hook_builder::instance().disconnect();
					}
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
					rop->m_settings.Rendering(false,false);
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
		if(m_current_render->m_rop_type != rop_type::stand_in)
		{
			// Export an NSIRenderControl "start" command at the end of the frame
			m_nsi.RenderControl(NSI::CStringPArg("action", "start"));
		}

		/*
			If we're rendering in batch mode from the current process, then we
			must wait for the render to finish (as if we were rendering "in
			foreground", from the current thread).
		*/
		if(m_current_render->m_batch && !m_sequence_context)
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

	if(m_sequence_context)
	{
		m_sequence_context->RenderControl(
			NSI::StringArg("action", "endsequence") );

		// In batch mode, wait for the renderdl process to finish before exiting
		if(!UTisUIAvailable())
		{
			m_sequence_context->RenderControl(
				NSI::StringArg("action", "wait") );
			m_sequence_context.reset();
		}
	}

	/*
		Destroy the rendering context if rendering/exporting is over.
		If we render in a background thread, the rendering context will be
		destroyed by that thread's stoppedcallback.
		If we render in a separate renderdl process, it will be destroyed by the
		m_sequence_waiter thread.
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
		m_settings.Rendering(false,false);
	}
	m_render_end_mutex.unlock();

	return ROP_CONTINUE_RENDER;
}

bool
ROP_3Delight::updateParmsFlags()
{
	bool changed = OP_Network::updateParmsFlags();
	setVisibleState("trange", false);
	if (m_rop_type == rop_type::stand_in || m_rop_type == rop_type::viewport)
	{
		setVisibleState("take", false);
		return changed;
	}

	PRM_Parm& parm = getParm(settings::k_aov);
	int size = parm.getMultiParmNumItems();
	if (size > 0) changed |= enableParm("aov_clear_1", size > 1);

	for (int i = 0; i < size; i++)
	{
		changed |= enableParm(aov::getAovStrToken(i), false);
	}

	changed |= enableParm(settings::k_view_layer, false);

	UT_String multilight_selection_val;
	evalString(multilight_selection_val, settings::k_multi_light_selection, 0, 0.0f);
	bool enable_multi_light_list = multilight_selection_val == "selection";

	PRM_Parm& multilight_parm = getParm(settings::k_light_sets);
	int multilight_size = multilight_parm.getMultiParmNumItems();

	//Disable Multi-Light list if 'off' or 'selection' options are selected.
	for (int i = 0; i < multilight_size; i++)
	{
		changed |= enableParm(settings::GetLightToken(i), false);
		changed |= enableParm(settings::GetUseRBBAOnlyToken(i),
			enable_multi_light_list && evalInt(settings::GetUseLightToken(i), 0, 0.0f));
		changed |= enableParm(settings::GetUseLightToken(i), enable_multi_light_list);
	}

	return changed;
}

void
ROP_3Delight::loadFinished()
{
	ROP_Node::loadFinished();
	m_settings.UpdateLights();

	// Ensure that the ROP's initial rendering state is reflected in the UI
	m_settings.Rendering(false,false);
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
		if(export_mode == k_stdout)
		{
			setString(
				settings::k_rm_export_stdout,
				CH_STRING_LITERAL,
				settings::k_old_render_mode,
				0, 0.0f);
			return;
		}
		if(export_mode != "off")
		{
			setString(
				settings::k_rm_export_file,
				CH_STRING_LITERAL,
				settings::k_old_render_mode,
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
				settings::k_old_render_mode,
				0, 0.0f);
			return;
		}
	}

	PRM_Parm* render_mode = i_old_parms->getParmPtr(settings::k_old_render_mode);
	if (render_mode)
	{
		UT_String render_mode_val;
		i_old_parms->evalString(render_mode_val, settings::k_old_render_mode, 0, 0.0);
		if (render_mode_val == "Export to File")
		{
			setInt(settings::k_display_rendered_images, 0, 0.0f, false);
			setInt(settings::k_output_nsi_files, 0, 0.0f, true);
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
	else if (name == settings::k_camera)
	{
		context* ctx = (context*)i_callee;
		OBJ_Camera* cam = rop->GetCamera(ctx->m_current_time);
		if (cam)
		{
			rop->ExportOutputs(*ctx, true);
			ctx->m_nsi.RenderControl(
				NSI::CStringPArg("action", "synchronize"));
		}
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
	VOP_Node *mats[3] = { nullptr };
	exporter::resolve_material_path(
		this,
		m_settings.GetAtmosphere(i_ctx.m_current_time).c_str(),
		mats );

	std::string env_handle = "atmosphere|environment";

	VOP_Node *atmo_vop = mats[2]; // volume
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

//If image filename or NSI filename contain some specific token, they will be expanded
void replaceIndividualTokens(std::string &filename, std::string token, std::string new_val)
{
	size_t start_pos = filename.find(token);
	while (start_pos != std::string::npos)
	{
		filename.replace(start_pos, token.length() , new_val);
		start_pos = filename.find(token, start_pos + 1);
	}
}

void replaceAllTokens(std::string& filename, std::string ext, std::string cam)
{
	replaceIndividualTokens(filename, "<project>", "`$HIP`");
	replaceIndividualTokens(filename, "<scene>", "`$HIPNAME`");
	replaceIndividualTokens(filename, "<pass>", "`$OS`");
	replaceIndividualTokens(filename, "<ext>", ext);
	replaceIndividualTokens(filename, "<camera>", cam);
	replaceIndividualTokens(filename, "#", "$F4");
}

//Used to output individual layers when using <aov> and <light> tokens.
std::string getImageFilenamePerAOV(UT_String image_file_name, std::string aov_layer_name, std::string light_path)
{
	std::string file_output_name = image_file_name.toStdString();

	//Replace <aov> token with the image layer name.
	replaceIndividualTokens(file_output_name, "<aov>", aov_layer_name);

	//For lights we only get their name from their full path.
	UT_String dir_name, light_layer_name;
	UT_String light = light_path.c_str();
	light.splitPath(dir_name, light_layer_name);

	if (!light_layer_name.isstring())
	{
		light_layer_name = "all";
	}

	//Replace <light> token with the proper light name used as a layer.
	replaceIndividualTokens(file_output_name, "<light>", light_layer_name.toStdString());
	return file_output_name;
}

void
ROP_3Delight::ExportOutputs(const context& i_ctx, bool i_ipr_camera_change)const
{
	OBJ_Camera* cam = GetCamera( i_ctx.m_current_time );
	double current_time = i_ctx.m_current_time;

	if( !cam )
	{
		return;
	}

	std::string screen_name( camera::screen_handle(cam, i_ctx) );

	UT_String idisplay_driver = "idisplay";
	UT_String file_driver;
	evalString(
		file_driver,
		settings::k_default_image_format, 0,
		current_time );

	std::string png_driver = "png";
	std::string jpeg_driver = "jpeg";

	UT_String image_file_name;
	evalStringRaw(
		image_file_name,
		settings::k_default_image_filename, 0,
		current_time );

	std::string image_file_name_str = image_file_name.toStdString();
	replaceAllTokens(image_file_name_str, file_driver.toStdString(), cam->getName().toStdString());
	OPgetDirector()->getChannelManager()->expandString(image_file_name_str.c_str(),image_file_name,current_time);

	UT_String image_display_name =
		image_file_name.replaceExtension(idisplay_driver);

	std::string idisplay_driver_name;
	std::string file_driver_name;
	std::string png_driver_name;
	std::string jpeg_driver_name;

	int nb_aovs = evalInt(settings::k_aov, 0, current_time);
	unsigned sort_key = 0;

	UT_String scalar_format;
	evalString(
		scalar_format,
		settings::k_default_image_bits, 0, current_time );

	UT_String filter;
	evalString(filter, settings::k_pixel_filter, 0, current_time);
	double filter_width = evalFloat(settings::k_filter_width, 0, current_time);

	std::vector<std::string> light_names;
	std::vector<bool> rgba_only;
	light_names.push_back(""); // no light for the first one
	rgba_only.push_back(false);
	m_settings.GetSelectedLights(light_names,rgba_only);

	bool has_frame_buffer = false;
	std::vector<std::string> file_layers;

	std::map<std::string, std::vector<OBJ_Node*>> light_categories;
	// Create a category with empty name and empty list (which means ALL lights)
	light_categories[std::string()];

	UT_String multilight_selection_val;
	evalString(multilight_selection_val, settings::k_multi_light_selection, 0, 0.0f);

	//Export only the selected categories.
	ExportLightCategories(i_ctx, light_categories, light_names, current_time);
	for (int i = 0; i < nb_aovs; i++)
	{
		bool is_layer_active = evalInt(aov::getAovActiveLayerToken(i), 0, current_time);

		UT_String label;
		evalString(label, aov::getAovStrToken(i), 0, current_time );

		const aov::description& desc = aov::getDescription(label.toStdString());

		// Don't do anything if layer is not active or a previously disconnected custom AOV.
		if (!is_layer_active || desc.m_variable_name == "")
			continue;

		bool idisplay_output, file_output;
		if( i_ctx.m_batch || i_ctx.m_export_nsi )
		{
			/*
				In thise mode, we only output to files and we don't care
				about the actual toggles.
			*/
			idisplay_output = false;
			file_output = true;
		}
		else
		{
			if( evalInt(
				settings::k_display_and_save_rendered_images, 0, current_time) )
			{
				idisplay_output = true;
				file_output = true;
			}
			else
			{
				idisplay_output =
					evalInt(
						settings::k_display_rendered_images, 0, current_time) != 0;
				file_output =
					evalInt(settings::k_save_rendered_images, 0, current_time);
			}
		}


		bool png_output = file_output;
		file_output = file_output && file_driver.toStdString() != "png";
		png_output = png_output && file_driver.toStdString() == "png";
		bool jpeg_output = evalInt(settings::k_save_jpeg_copy, 0, current_time);

		// We only output Jpeg and PNG images for beauty layer
		if (desc.m_variable_name != "Ci")
		{
			jpeg_output = false;
			png_output = false;
		}

		/**
			Always render to iDisplay if "Start IPR" button is clicked OR
			we called from inside 3Delight Display to re-render an IPR
			session. Also reconnect the camera for each layer when it changes
			during IPR from Render Settings.
		*/
		if (evalInt(settings::k_ipr_start, 0, current_time)
			|| m_idisplay_ipr || i_ipr_camera_change)
		{
			idisplay_output = true;
			file_output = false;
			png_output = false;
			jpeg_output = false;
		}
		if (!idisplay_output && !file_output && !png_output && !jpeg_output)
		{
			continue;
		}

		char prefix[12] = "";
		::sprintf(prefix, "%d", i+1);

		/*
			Some of the AOVs cannot be exported in Multi-light.
			For example, "Z"
		*/
		std::map<std::string, std::vector<OBJ_Node*>> aov_light_categories;
		unsigned nb_light_categories = light_categories.size();
		if (desc.m_support_multilight)
		{
			aov_light_categories = light_categories;
		}
		else
		{
			/* Empty category = All lights */
			aov_light_categories[std::string()];
		}

		int j = -1;
		for (auto& category : aov_light_categories)
		{
			j++;
			if (multilight_selection_val == "selection" && rgba_only[j] && desc.m_variable_name != "Ci")
				continue;

			std::string layer_name = prefix;
			layer_name += "-";
			layer_name += desc.m_filename_token;
			if (!category.first.empty())
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
					screen_name, category.first,
					idisplay_driver_name, idisplay_driver.toStdString(),
					sort_key, i_ipr_camera_change);

				/*
					3Delight Display's Multi-Light tool needs some information,
					called "feedback data" to communicate back the values.
				*/
				ExportLayerFeedbackData( i_ctx, layer_name, category.first,category.second);
			}

			if (file_output)
			{
				std::string file_layer_name = layer_name + "_file";
				std::string file_output_name =
					getImageFilenamePerAOV(image_file_name, desc.m_filename_token, category.first);

				size_t light_pos = image_file_name.toStdString().find("<light>");
				size_t aov_pos = image_file_name.toStdString().find("<aov>");

				char suffix[12] = "1";

				bool suffix_updated = true;
				//Use the proper suffix for drivername so it writes the aov properly when using layer tokens.

				if (light_pos != std::string::npos && aov_pos != std::string::npos)
				{
					::sprintf(suffix, "%u", i * nb_light_categories + j + 1);
				}
				else if (aov_pos != std::string::npos)
				{
					::sprintf(suffix, "%u", i + 1);
				}
				else if (light_pos != std::string::npos)
				{
					::sprintf(suffix, "%u", j + 1);
				}

				file_driver_name = "file_driver_";
				file_driver_name += suffix;

				//Don't create a new driver for the same filename.
				//Could have a nicer solution rather than using vector.
				if (file_driver_name.empty() 
					|| std::find(file_layers.begin(), file_layers.end(), file_output_name) == file_layers.end())
				{
					file_layers.push_back(file_output_name);
					i_ctx.m_nsi.Create(file_driver_name, "outputdriver");
					i_ctx.m_nsi.SetAttribute(
						file_driver_name,
					(
						NSI::CStringPArg("drivername", file_driver.c_str()),
						NSI::StringArg("imagefilename", file_output_name)
					) );
				}

				ExportOneOutputLayer(
					i_ctx, file_layer_name, desc, scalar_format,
					filter, filter_width,
					screen_name, category.first,
					file_driver_name, file_driver.toStdString(),
					sort_key);
			}

			//png does not support multi-layers. Only output beauty layer for all lights.
			if (png_output && j==0)
			{
				std::string png_layer_name = layer_name + "_png";
				png_driver_name = "png_driver";

				UT_String image_png_name = image_file_name.replaceExtension("png");

				//use meaningful replacements for <aov> and <light> token when multi-layer is ignored.
				image_png_name.substitute("<aov>", "rgba", true);
				image_png_name.substitute("<light>", "all", true);

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
					screen_name, category.first,
					png_driver_name, png_driver,
					sort_key);
			}

			if (jpeg_output)
			{
				std::string jpeg_layer_name = layer_name + "_jpeg";
				jpeg_driver_name = "jpeg_driver";

				UT_String image_jpeg_name = image_file_name.replaceExtension("jpg");
				image_jpeg_name.substitute("<aov>","rgba",true);
				image_jpeg_name.substitute("<light>","all",true);

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
					screen_name, category.first,
					jpeg_driver_name, jpeg_driver,
					sort_key);
			}
		}
	}

	/*
		If we have at least one frame buffer output, we go for the circular
		pattern. When only file output is request, we will use the more
		efficient (at least, memory wise) scanline pattern.

		Also, when rendering a sequence, it makes little sense to have a
		framebuffer as we need maximum efficiency in that case as well.
	*/
	if( !has_frame_buffer || !i_ctx.SingleFrame() )
	{
		i_ctx.m_nsi.SetAttribute(
			NSI_SCENE_GLOBAL,
			NSI::CStringPArg( "bucketorder", "horizontal"));
	}

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
	unsigned& io_sort_key,
	bool i_ipr_camera_change) const
{
	// Output only RGBA layer if Disable extra Image Layer has been selected.
	if ((HasSpeedBoost(i_ctx.m_current_time)
		&& evalInt(settings::k_disable_extra_image_layers, 0, i_ctx.m_current_time)
		&& i_desc.m_variable_name!="Ci")
		|| i_desc.m_variable_name=="")
	{
		return;
	}
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

		bool enable_clamping =
			hasParm(settings::k_enable_clamp) &&
			evalInt(settings::k_enable_clamp, 0, i_ctx.m_current_time);

		if( !enable_clamping && i_desc.m_layer_type == "color" )
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

	if( i_desc.m_variable_name == "Ci" || i_desc.m_variable_name == "outlines" )
	{
		/* We draw outlines on "Ci" and the "outlines" AOV. */
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
		(i_driver_name == "exr" || i_driver_name == "dwaaexr"))
	{
		cryptomatte_layers = 2;
	}

	if (i_ipr_camera_change)
	{
		i_ctx.m_nsi.Disconnect(
			i_layer_handle, "",
			".all", "outputlayers");
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

			if (i_ipr_camera_change)
			{
				i_ctx.m_nsi.Disconnect(
					cl_handle, "",
					".all", "outputlayers");
			}

			i_ctx.m_nsi.Connect(cl_handle, "", i_screen_handle, "outputlayers");

			i_ctx.m_nsi.Connect(i_driver_handle, "", cl_handle, "outputdrivers");
		}
	}
}

void
ROP_3Delight::ExportLayerFeedbackData(
	const context& i_ctx,
	const std::string& i_layer_handle,
	const std::string& i_light_category,
	const std::vector<OBJ_Node*>& i_lights) const
{
	// Build nodes, intensities and colors JSON arrays

	std::string nodes = "\"nodes\":[";
	std::string intensities = "\"intensities\":[";
	std::string colors = "\"colors\":[";

	double time = i_ctx.m_current_time;
	for (OBJ_Node* light : i_lights)
	{
		double intensity = 1.0;
		if (light->hasParm("light_intensity"))
		{
			intensity = light->evalFloat("light_intensity", 0, time);
		}

		double color[3] = { 1.0, 1.0, 1.0 };
		if (light->hasParm("light_color"))
		{
			color[0] = light->evalFloat("light_color", 0, time);
			color[1] = light->evalFloat("light_color", 1, time);
			color[2] = light->evalFloat("light_color", 2, time);
		}

		nodes += "\"" + light->getFullPath().toStdString() + "\",";
		intensities += std::to_string(intensity) + ",";
		colors +=
			"[" + std::to_string(color[0]) + "," + std::to_string(color[1]) +
			"," + std::to_string(color[2]) + "],";
	}

	// Remove trailing commas
	if (!i_lights.empty())
	{
		nodes.pop_back();
		intensities.pop_back();
		colors.pop_back();
	}

	nodes += "]";
	intensities += "]";
	colors += "]";

	// Build name and type JSON attributes
	std::string name = "\"name\":\"" + i_light_category + "\"";
	std::string light_type =
		std::string("\"type\":") +
		(i_lights.size() == 1 ? "\"single light\"" : "\"light group\"");

	// Put all attributes together in a single JSON object.
	std::string feedback_data =
		"{" + name + "," + light_type + "," + nodes + "," + intensities + "," +
		colors + "}";

	// Retrieve connection information
	idisplay_port* idp = idisplay_port::get_instance();
	std::string feedback_id = idp->GetServerID();
	i_ctx.m_nsi.SetAttribute(
		i_layer_handle,
		(
			NSI::StringArg("sourceapp", "Houdini"),
			NSI::StringArg("feedbackid", feedback_id),
			NSI::StringArg("feedbackdata", feedback_data)
			));
}

void ROP_3Delight::ExportLightCategories(
	const context& i_ctx,
	std::map<std::string, std::vector<OBJ_Node*>>& o_light_categories,
	std::vector<std::string> o_sel_lights,
	fpreal t) const
{
	std::vector<OBJ_Node*> i_lights;
	m_settings.GetLights(i_lights, t);

	if (i_lights.empty())
		return;

	OP_BundleList* blist = OPgetDirector()->getBundles();
	assert(blist);
	int numBundles = blist->entries();

	for (auto light_source : i_lights)
	{
		bool foundInBundle = false;
		bool incand =
			light_source->getOperator()->getName() ==
			"3Delight::IncandescenceLight";
		bool isvdb = light_source->castToOBJLight() == nullptr;

		std::string light_handle = exporter::handle(*light_source, i_ctx);

		for (int i = 0; !incand && !isvdb && i < numBundles; i++)
		{
			OP_Bundle* bundle = blist->getBundle(i);
			assert(bundle);

			if (bundle && bundle->contains(light_source, false)
				&& std::find(o_sel_lights.begin(), o_sel_lights.end(), bundle->getName())
				!= o_sel_lights.end())
			{
				std::string category = bundle->getName();

				if (o_light_categories[category].empty())
				{
					// Create a set the first time we encounter a bundle
					i_ctx.m_nsi.Create(category, "set");
				}

				o_light_categories[category].push_back(light_source);

				// Connect the light to the set
				i_ctx.m_nsi.Connect(light_handle, "", category, "members");

				foundInBundle = true;
			}
		}

		if (!foundInBundle)
		{
			/* Make a category for this single light */
			std::string category = light_source->getFullPath().toStdString();

			if (std::find(o_sel_lights.begin(), o_sel_lights.end(), category)
				== o_sel_lights.end())
			{
				continue;
			}
			if (category != light_handle && !incand)
			{
				/*
					In IPR, the light handle is not the same as its full path,
					but we still want to use the full path as a category name,
					so we create a set with the proper name.
				*/
				assert(i_ctx.m_ipr);
				i_ctx.m_nsi.Create(category, "set");
				i_ctx.m_nsi.Connect(light_handle, "", category, "members");
			}

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

	/*
		trange parameter is responsible for sequence rendering. Whenever it's value
		is true it means that we are rendering a sequence of frames. So we ignore
		the overrides on sequence render.
	*/
	if (evalInt("trange", 0, t))
	{
		return false;
	}

	bool export_to_nsi = m_settings.export_to_nsi(t);
	if(export_to_nsi)
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

	if(!evalInt(settings::k_override_camera_resolution, 0, t))
	{
		return 1.0f;
	}

	int resolution_factor = evalInt(settings::k_resolution_factor_override, 0, t);

	//-1 factor stands for user specified resolution.
	float factors[] = { 0.125f, 0.25f, 1.0f/3.0f, 0.5f, 2.0f/3.0f, 0.75f, 1.5f, 2.0f, -1 };
	if(resolution_factor < 0 ||
		resolution_factor >= sizeof(factors) / sizeof(factors[0]))
	{
		return 1.0f;
	}

	return factors[resolution_factor];
}

float
ROP_3Delight::GetSpeedBoostResolutionFactor()const
{
	fpreal t = m_current_render->m_current_time;

	if (!HasSpeedBoost(t))
	{
		return 1.0f;
	}

	int resolution_factor = evalInt(settings::k_resolution_factor_speed_boost, 0, t);
	float factors[] = { 1.0f, 0.5f, 0.25f, 0.125f };
	if (resolution_factor < 0 ||
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

	float factors[] = { 1.0f, 0.5f, 0.25f, 0.1f, 0.04f, 0.01f };
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

	if (hasParm(settings::k_camera))
	{
		evalString(cam_path, settings::k_camera, 0, t);
	}
	if (!cam_path)
		return nullptr;

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

	/*
		Since we do not export camera parameters on standin ROP
		but we export motion blur as enabled, we return 1.0
		for shutter interval as trying to get it from camera
		when it does not exist would produce an error.
	*/
	if (m_rop_type == rop_type::stand_in)
	{
		return 1.0;
	}

	if (m_rop_type == rop_type::viewport)
	{
		return viewport_hook_builder::instance().active_vport_camera_shutter();
	}

	OBJ_Camera* cam = ROP_3Delight::GetCamera(i_time);
	return cam ? camera::get_shutter_duration(*cam, i_time) : 1.0;
}

double
ROP_3Delight::GetShutterOffset(double i_time)const
{
	if(!HasMotionBlur(i_time))
	{
		return 0.0;
	}

	OBJ_Camera* cam = ROP_3Delight::GetCamera(i_time);
	return cam ? camera::get_shutter_offset(*cam, i_time) : 0.0;
}

bool
ROP_3Delight::HasDepthOfField( double t )const
{
	return !(HasSpeedBoost(t) && evalInt(settings::k_disable_depth_of_field, 0, t));
}

void ROP_3Delight::time_change_cb(double i_time)
{
	assert(m_current_render);
	assert(m_current_render->m_ipr);

	m_current_render->m_time_dependent = true;
	m_current_render->set_current_time(i_time);

	scene::convert_to_nsi(*m_current_render);
	if (m_current_render->m_rop_type != rop_type::viewport)
	{
		ExportAtmosphere(*m_current_render, true);

		/*
			Camera attributes are are set on viewport_hook for viewport rendering
			so we don't need to set them for viewport rendering again.
		*/
		OBJ_Camera* cam = GetCamera(m_current_render->m_current_time);
		if (cam)
		{
			camera* cam_obj = new camera(*m_current_render, cam);
			cam_obj->set_attributes();
		}
	}
	m_current_render->m_nsi.RenderControl(
		NSI::CStringPArg("action", "synchronize"));
}

std::string
ROP_3Delight::GetNSIExportFilename(double i_time)const
{
	bool export_to_nsi = m_settings.export_to_nsi(i_time);
	if (!export_to_nsi)
		return {};

	OBJ_Camera* cam = GetCamera(i_time);

	UT_String export_file;
	evalStringRaw(export_file, settings::k_default_export_nsi_filename, 0, i_time);
	std::string export_file_str = export_file.toStdString();
	replaceAllTokens(export_file_str, "nsi", cam ? cam->getName().toStdString() : "");
	OPgetDirector()->getChannelManager()->expandString(export_file_str.c_str(), export_file, i_time);

	if(export_file.length() == 0)
	{
		// When no file is specified, we output to standard output by default
		return k_stdout;
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
	notes += getFullPath().toStdString();
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
