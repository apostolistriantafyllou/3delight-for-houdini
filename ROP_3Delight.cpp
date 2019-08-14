#include "ROP_3Delight.h"

#include "camera.h"
#include "context.h"
#include "mplay.h"
#include "scene.h"
#include "shader_library.h"
#include "ui/select_layers_dialog.h"

#include <OBJ/OBJ_Camera.h>
#include <OP/OP_BundlePattern.h>
#include <OP/OP_Director.h>
#include <OP/OP_OperatorTable.h>
#include <ROP/ROP_Templates.h>
#include <UT/UT_UI.h>

#include <nsi.hpp>
#include <nsi_dynamic.hpp>

#include <iostream>

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
}

OP_Node*
ROP_3Delight::alloc(OP_Network* net, const char* name, OP_Operator* op)
{
	return new ROP_3Delight(net, name, op);
}

ROP_3Delight::ROP_3Delight(
	OP_Network* net,
	const char* name,
	OP_Operator* entry)
	:	ROP_Node(net, name, entry), m_settings(*this)
{
	/*
		Rename the "Render to MPlay" button to avoid referencing a specific
		framebuffer.
	*/
	PRM_Parm* preview_parm = getParmPtr("renderpreview");
	assert(preview_parm);
	PRM_Template* preview_tmpl = preview_parm->getTemplatePtr();
	assert(preview_tmpl);
	PRM_Name* preview_name = preview_tmpl->getNamePtr();
	assert(preview_name);
	preview_name->setLabel("Render Preview");
}


ROP_3Delight::~ROP_3Delight()
{
}


void ROP_3Delight::onCreated()
{
	ROP_Node::onCreated();
	m_settings.UpdateLights();
	enableParm(settings::k_save_ids_as_cryptomatte, false);
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


int ROP_3Delight::startRender(int, fpreal tstart, fpreal tend)
{
	m_end_time = tend; // \ref endRender

	NSI::DynamicAPI api;
	NSI::Context nsi(api);

	register_mplay_driver( api );

	bool render = !evalInt(settings::k_export_nsi, 0, 0.0f);

	if(render)
	{
		nsi.Begin();
	}
	else
	{
		nsi.Begin( NSI::IntegerArg("streamfiledescriptor", 1) );
	}

	fpreal fps = OPgetDirector()->getChannelManager()->getSamplesPerSec();

	/*
		Unfortunately, getRenderMode() always returns RENDER_RM_PRM, so we have
		to use a special recipe to detect that the "Render yo MPlay" button has
		been pressed.
	*/
	UT_String output_override;
	bool output_overriden = getOutputOverride(output_override, tstart);
	UT_String device_override;
	bool device_overriden = getDeviceOverride(device_override, tstart);
	bool preview =
		output_overriden && output_override == "ip" &&
		device_overriden && device_override == "";
	bool batch = !UTisUIAvailable();
	context ctx(
		nsi,
		tstart,
		tend,
		GetShutterInterval(tstart),
		fps,
		HasDepthOfField(),
		batch,
		!render,
		OP_BundlePattern::allocPattern(m_settings.GetObjectsToRender()),
		OP_BundlePattern::allocPattern(m_settings.GetLightsToRender()));

	if(error() < UT_ERROR_ABORT)
	{
		executePreRenderScript(tstart);

		scene::convert_to_nsi( ctx );
	}

	ExportOutputs(ctx);

	ExportGlobals(ctx);
	ExportDefaultMaterial(ctx);

	if(render)
	{
		nsi.RenderControl(NSI::CStringPArg("action", "start"));
		nsi.RenderControl(NSI::CStringPArg("action", "wait"));
	}

	nsi.End();

	OP_BundlePattern::freePattern(ctx.m_lights_to_render_pattern);
	OP_BundlePattern::freePattern(ctx.m_objects_to_render_pattern);

	return 1;
}

ROP_RENDER_CODE
ROP_3Delight::renderFrame(fpreal time, UT_Interrupt*)
{
	executePreFrameScript(time);

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

bool
ROP_3Delight::isPreviewAllowed()
{
	return true;
}

void
ROP_3Delight::loadFinished()
{
	ROP_Node::loadFinished();
	m_settings.UpdateLights();
}

void
ROP_3Delight::ExportOutputs(const context& i_ctx)const
{
	OBJ_Camera* cam = GetCamera();

	if( !cam )
	{
		return;
	}

	int default_resolution[2] =
	{
		int(::roundf(cam->RESX(0)*GetResolutionFactor())),
		int(::roundf(cam->RESY(0)*GetResolutionFactor()))
	};
	float crop[2][2] = {{ float(cam->CROPL(0)), float(cam->CROPB(0)) },
						{ float(cam->CROPR(0)), float(cam->CROPT(0)) }};
	// screenwindow
#if 0
	double size[2] = { cam->WINSIZEX(0), cam->WINSIZEY(0) };
	double center[2] = { cam->WINX(0), cam->WINY(0) };
	double sw[2][2] =
	{
		{ center[0] - size[0], center[1] - size[1] },
		{ center[0] + size[0], center[1] + size[1] }
	};
#endif
	i_ctx.m_nsi.Create("default_screen", "screen");
	i_ctx.m_nsi.SetAttribute(
		"default_screen",
		(
			*NSI::Argument::New("resolution")
			->SetArrayType(NSITypeInteger, 2)
			->SetCount(1)
			->CopyValue(default_resolution, sizeof(default_resolution)),
			*NSI::Argument::New("crop")
			->SetArrayType(NSITypeFloat, 2)
			->SetCount(2)
			->SetValuePointer(crop),
#if 0
			*NSI::Argument::New("screenwindow")
			->SetArrayType(NSITypeDouble, 2)
			->SetCount(2)
			->SetValuePointer(sw),
#endif
			NSI::IntegerArg("oversampling", GetPixelSamples())
		) );

	std::string screen_name = "default_screen";

	i_ctx.m_nsi.Connect(
		screen_name.c_str(), "",
		camera::get_nsi_handle(*cam), "screens");

	UT_String idisplay_driver = "idisplay";
	UT_String file_driver;
	evalString(file_driver, settings::k_default_image_format, 0, 0.0f);
	UT_String png_driver = "png";
	UT_String jpeg_driver = "jpeg";

	UT_String image_file_name;
	evalString( image_file_name, settings::k_default_image_filename, 0, 0.0f );

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
	evalString(scalar_format, settings::k_default_image_bits, 0, 0.0f);

	UT_String filter;
	evalString(filter, settings::k_pixel_filter, 0, 0.0f);
	double filter_width = evalFloat(settings::k_filter_width, 0, 0.0f);

	std::vector<std::string> light_names;
	light_names.push_back(""); // no light for the first one
	m_settings.GetSelectedLights(light_names);

	for (int i = 0; i < nb_aovs; i++)
	{
		UT_String label;
		evalString(label, aov::getAovStrToken(i), 0, 0.0f);

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
				if (idisplay_driver_name.empty())
				{
					idisplay_driver_name = "idisplay_driver";
					i_ctx.m_nsi.Create(
						idisplay_driver_name.c_str(), "outputdriver");
					i_ctx.m_nsi.SetAttribute(
						idisplay_driver_name.c_str(),
					(
						NSI::CStringPArg("drivername", idisplay_driver.c_str()),
						NSI::CStringPArg("imagefilename", image_display_name.c_str())
					) );
				}

				ExportOneOutputLayer(
					i_ctx, layer_name, desc, scalar_format,
					filter, filter_width,
					screen_name, light_names[j],
					idisplay_driver_name, sort_key);
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
					screen_name, light_names[j],
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
					screen_name, light_names[j],
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
					screen_name, light_names[j],
					jpeg_driver_name, sort_key);
			}
		}
	}

/*
FIXME : do the real thing

and the following ROP_3Delight node attributes:
k_default_image_format
k_save_ids_as_cryptomatte
k_aovs

refer to
https://www.sidefx.com/docs/hdk/_h_d_k__node_intro__working_with_parameters.html
*/
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
	int speed_boost = evalInt(settings::k_speed_boost, 0, 0.0f);
	return speed_boost;
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

float
ROP_3Delight::GetShutterInterval(float i_time)const
{
	if(!HasMotionBlur())
	{
		return 0.0f;
	}

	OBJ_Camera* cam = ROP_3Delight::GetCamera();
	return cam ? cam->SHUTTER(i_time) : 1.0f;
}

bool
ROP_3Delight::HasDepthOfField()const
{
	return !(HasSpeedBoost() && evalInt(settings::k_disable_depth_of_field, 0, 0.0f));
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
