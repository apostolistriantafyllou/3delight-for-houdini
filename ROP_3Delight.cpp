#include "ROP_3Delight.h"

#include "camera.h"
#include "context.h"
#include "mplay.h"
#include "scene.h"
#include "shader_library.h"
#include "ui/aov.h"
#include "ui/select_layers_dialog.h"

#include <OBJ/OBJ_Camera.h>
#include <OP/OP_BundlePattern.h>
#include <OP/OP_Director.h>
#include <OP/OP_OperatorTable.h>
#include <PRM/PRM_Include.h>
#include <PRM/PRM_SpareData.h>
#include <ROP/ROP_Templates.h>

#include <nsi.hpp>
#include <nsi_dynamic.hpp>

#include <iostream>

#define EXPORT_NSI_CHECKBOX

static const float k_one_line = 0.267;

#ifdef EXPORT_NSI_CHECKBOX
static const char* k_export_nsi = "export_nsi";
#endif
static const char* k_shading_samples = "shading_samples";
static const char* k_pixel_samples = "pixel_samples";
static const char* k_volume_samples = "volume_samples";
static const char* k_pixel_filter = "pixel_filter";
static const char* k_filter_width = "filter_width";
static const char* k_motion_blur = "motion_blur";
static const char* k_max_diffuse_depth = "max_diffuse_depth";
static const char* k_max_reflection_depth = "max_reflection_depth";
static const char* k_max_refraction_depth = "max_refraction_depth";
static const char* k_max_hair_depth = "max_hair_depth";
static const char* k_max_distance = "max_distance";
static const char* k_camera = "camera";
static const char* k_objects_to_render = "objects_to_render";
static const char* k_lights_to_render = "lights_to_render";
static const char* k_default_image_filename = "default_image_filename";
static const char* k_default_image_format = "default_image_format";
static const char* k_default_image_bits = "default_image_bits";
static const char* k_save_ids_as_cryptomatte = "save_ids_as_cryptomatte";
static const char* k_batch_output_mode = "batch_output_mode";
static const char* k_interactive_output_mode = "interactive_output_mode";
static const char* k_aovs = "aovs";
static const char* k_aov = "aov";
static const char* k_framebuffer_output = "framebuffer_output_#";
static const char* k_file_output = "file_output_#";
static const char* k_jpeg_output = "jpeg_output_#";
static const char* k_aov_label = "aov_label_#";
static const char* k_aov_name = "aov_name_#";
static const char* k_aov_clear = "aov_clear_#";
static const char* k_add_layer = "add_layer";
static const char* k_view_layer = "view_layer";
static const char* k_ignore_matte_attribute = "ignore_matte_attribute";
static const char* k_matte_sets = "matte_sets";
static const char* k_light_sets = "light_sets";
static const char* k_use_light_set = "use_light_set_#";
static const char* k_light_set = "light_set_#";
static const char* k_display_all_lights = "display_all_lights";
static const char* k_speed_boost = "speed_boost";
static const char* k_disable_motion_blur = "disable_motion_blur";
static const char* k_disable_depth_of_field = "disable_depth_of_field";
static const char* k_disable_displacement = "disable_displacement";
static const char* k_disable_subsurface = "disable_subsurface";
static const char* k_resolution_factor = "resolution_factor";
static const char* k_sampling_factor = "sampling_factor";

static PRM_Template*
GetTemplates()
{
	static PRM_Name separator1("separator1", "");
	static PRM_Name separator2("separator2", "");
	static PRM_Name separator3("separator3", "");
	static PRM_Name separator4("separator4", "");
	static PRM_Name separator5("separator5", "");
	static PRM_Name separator6("separator6", "");

#ifdef EXPORT_NSI_CHECKBOX
	static PRM_Name export_nsi(k_export_nsi, "Export NSI");
	static PRM_Default export_nsi_d(true);
#endif

	// Quality

	static PRM_Name shading_samples(k_shading_samples, "Shading Samples");
	static PRM_Default shading_samples_d(64);
	static PRM_Range shading_samples_r(PRM_RANGE_RESTRICTED, 1, PRM_RANGE_UI, 400);

	static PRM_Name pixel_samples(k_pixel_samples, "Pixel Samples");
	static PRM_Default pixel_samples_d(8);
	static PRM_Range pixel_samples_r(PRM_RANGE_RESTRICTED, 1, PRM_RANGE_UI, 100);

	static PRM_Name volume_samples(k_volume_samples, "Volume Samples");
	static PRM_Default volume_samples_d(16);
	static PRM_Range volume_samples_r(PRM_RANGE_RESTRICTED, 1, PRM_RANGE_UI, 400);

	static PRM_Name pixel_filter(k_pixel_filter, "Pixel Filter");
	static PRM_Default pixel_filter_d(0.0f, "blackman-harris");
	static PRM_Item pixel_filter_i[] =
	{
		PRM_Item("blackman-harris", "Blackman-Harris"),
		PRM_Item("mitchell", "Mitchell"),
		PRM_Item("catmull-rom", "Catmull-Rom"),
		PRM_Item("sinc", "Sinc"),
		PRM_Item("box", "Box"),
		PRM_Item("triangle", "Triangle"),
		PRM_Item("gaussian", "Gaussian"),
		PRM_Item()
	};
	static PRM_ChoiceList pixel_filter_c(PRM_CHOICELIST_SINGLE, pixel_filter_i);

	static PRM_Name filter_width(k_filter_width, "Filter Width");
	static PRM_Default filter_width_d(3.0f);
	static PRM_Range filter_width_r(PRM_RANGE_RESTRICTED, 0.001f, PRM_RANGE_RESTRICTED, 10.0f);

	static PRM_Name motion_blur(k_motion_blur, "Motion Blur");
	static PRM_Default motion_blur_d(true);

	static PRM_Name motion_blur_note1(
		"motion_blur_note1",
		"This enables linear blur with 2 samples. For motion blur with noticeable");
	static PRM_Name motion_blur_note2(
		"motion_blur_note2",
		"curvature, additional samples can be specified per object and per set.");

	static PRM_Name max_diffuse_depth(k_max_diffuse_depth, "Max Diffuse Depth");
	static PRM_Default max_diffuse_depth_d(2);
	static PRM_Range max_diffuse_depth_r(PRM_RANGE_RESTRICTED, 0, PRM_RANGE_UI, 10);

	static PRM_Name max_reflection_depth(k_max_reflection_depth, "Max Reflection Depth");
	static PRM_Default max_reflection_depth_d(2);
	static PRM_Range max_reflection_depth_r(PRM_RANGE_RESTRICTED, 0, PRM_RANGE_UI, 10);

	static PRM_Name max_refraction_depth(k_max_refraction_depth, "Max Refraction Depth");
	static PRM_Default max_refraction_depth_d(4);
	static PRM_Range max_refraction_depth_r(PRM_RANGE_RESTRICTED, 0, PRM_RANGE_UI, 10);

	static PRM_Name max_hair_depth(k_max_hair_depth, "Max Hair Depth");
	static PRM_Default max_hair_depth_d(5);
	static PRM_Range max_hair_depth_r(PRM_RANGE_RESTRICTED, 0, PRM_RANGE_UI, 10);

	static PRM_Name max_distance(k_max_distance, "Max Distance");
	static PRM_Default max_distance_d(1000.0f);
	static PRM_Range max_distance_r(PRM_RANGE_RESTRICTED, 0.0f, PRM_RANGE_UI, 2000.0f);

	static std::vector<PRM_Template> quality_templates =
	{
		PRM_Template(PRM_INT, 1, &shading_samples, &shading_samples_d, nullptr, &shading_samples_r),
		PRM_Template(PRM_INT, 1, &pixel_samples, &pixel_samples_d, nullptr, &pixel_samples_r),
		PRM_Template(PRM_INT, 1, &volume_samples, &volume_samples_d, nullptr, &volume_samples_r),
		PRM_Template(PRM_SEPARATOR, 0, &separator1),
		PRM_Template(PRM_STRING, 1, &pixel_filter, &pixel_filter_d, &pixel_filter_c),
		PRM_Template(PRM_FLT, 1, &filter_width, &filter_width_d, nullptr, &filter_width_r),
		PRM_Template(PRM_SEPARATOR, 0, &separator2),
		PRM_Template(PRM_TOGGLE, 1, &motion_blur, &motion_blur_d),
		PRM_Template(PRM_LABEL, 0, &motion_blur_note1),
		PRM_Template(PRM_LABEL, 0, &motion_blur_note2),
		PRM_Template(PRM_SEPARATOR, 0, &separator3),
		PRM_Template(PRM_INT, 1, &max_diffuse_depth, &max_diffuse_depth_d, nullptr, &max_diffuse_depth_r),
		PRM_Template(PRM_INT, 1, &max_reflection_depth, &max_reflection_depth_d, nullptr, &max_reflection_depth_r),
		PRM_Template(PRM_INT, 1, &max_refraction_depth, &max_refraction_depth_d, nullptr, &max_refraction_depth_r),
		PRM_Template(PRM_INT, 1, &max_hair_depth, &max_hair_depth_d, nullptr, &max_hair_depth_r),
		PRM_Template(PRM_FLT|PRM_TYPE_PLAIN, 1, &max_distance, &max_distance_d, nullptr, &max_distance_r)
	};

	// Scene elements

	static PRM_Name camera(k_camera, "Camera");
	static PRM_Default camera_d(0.0f, "/obj/cam1");

	static PRM_Name objects_to_render(k_objects_to_render, "Objects to Render");
	static PRM_Default objects_to_render_d(0.0f, "*");

	static PRM_Name lights_to_render(k_lights_to_render, "Lights to Render");
	static PRM_Default lights_to_render_d(0.0f, "*");

	static std::vector<PRM_Template> scene_elements_templates =
	{
		PRM_Template(PRM_STRING, PRM_TYPE_DYNAMIC_PATH, 1, &camera, &camera_d, nullptr, nullptr, nullptr, &PRM_SpareData::objCameraPath),
		PRM_Template(PRM_STRING, PRM_TYPE_DYNAMIC_PATH_LIST, 1, &objects_to_render, &objects_to_render_d, nullptr, nullptr, nullptr, &PRM_SpareData::objGeometryPath),
		PRM_Template(PRM_STRING, PRM_TYPE_DYNAMIC_PATH_LIST, 1, &lights_to_render, &lights_to_render_d, nullptr, nullptr, nullptr, &PRM_SpareData::objLightPath)
	};

/*
	Environment
	Atmosphere

	//? Frame range

	Current Frame Only
	Frame Range
	Increment

	//? Image Resolution and Crop

	Use Render Settings' Image Size
	Resolution
	Pixel Aspect Ratio

	Use Crop Window
	Crop Min
	Crop Max
*/

	// Image Layers

	static PRM_Name default_image_filename(k_default_image_filename, "Image Filename");
	static PRM_Default default_image_filename_d(
		0.0f, "$HIP/render/$HIPNAME.$OS.$F4.exr");

	static PRM_Name default_image_format(k_default_image_format, "Image Format");
	static PRM_Default default_image_format_d(0.0f, "exr");
	static PRM_Item default_image_format_i[] =
	{
		PRM_Item("tiff", "TIFF"),
		PRM_Item("exr", "OpenEXR"),
		PRM_Item("deepexr", "OpenEXR (deep)"),
		PRM_Item("jpeg", "JPEG"),
		PRM_Item("png", "PNG"),
		PRM_Item(),
	};
	static PRM_ChoiceList default_image_format_c(PRM_CHOICELIST_SINGLE, default_image_format_i);

	static PRM_Name default_image_bits(k_default_image_bits, "Image Bits");
	static PRM_Default default_image_bits_d(0.0f, "half");
	static PRM_Item default_image_bits_i[] =
	{
		PRM_Item("uint8", "8-bit"),
		PRM_Item("uint16", "16-bit"),
		PRM_Item("half", "16-bit float"),
		PRM_Item("float", "32-bit float"),
		PRM_Item(),
	};
	static PRM_ChoiceList default_image_bits_c(PRM_CHOICELIST_SINGLE, default_image_bits_i);

	static PRM_Name batch_output_mode(k_batch_output_mode, "Batch Output Mode");
	static PRM_Default batch_output_mode_d(0);
	static PRM_Item batch_output_mode_i[] =
	{
		PRM_Item("", "Enable file output as selected"),
		PRM_Item("", "Enable all file output and selected JPEG"),
		PRM_Item(),
	};
	static PRM_ChoiceList batch_output_mode_c(PRM_CHOICELIST_SINGLE, batch_output_mode_i);

	static PRM_Name interactive_output_mode(k_interactive_output_mode, "Interactive Output Mode");
	static PRM_Default interactive_output_mode_d(0);
	static PRM_Item interactive_output_mode_i[] =
	{
		PRM_Item("", "Enable file output as selected"),
		PRM_Item("", "Enable file output only for selected layer"),
		PRM_Item("", "Disable file output"),
		PRM_Item(),
	};
	static PRM_ChoiceList interactive_output_mode_c(PRM_CHOICELIST_SINGLE, interactive_output_mode_i);

	static PRM_Name save_ids_as_cryptomatte(k_save_ids_as_cryptomatte, "Save IDs as Cryptomatte");
	static PRM_Default save_ids_as_cryptomatte_d(false);

	static PRM_Name aovs_titles1("aovs_titles1", "D   F   J                                  \t");
	static PRM_Name aovs_titles2("aovs_titles2", "Layer Name                 ");

	static PRM_Name aovs(k_aovs, "Image Layers");
	static PRM_Name aov(k_aov, "Image Layer (AOV)");
	static PRM_Name framebuffer_output(k_framebuffer_output, "Preview");
	static PRM_Default framebuffer_output_d(true);
	static PRM_Name file_output(k_file_output, "File");
	static PRM_Default file_output_d(true);
	static PRM_Name jpeg_output(k_jpeg_output, " ");
	static PRM_Default jpeg_output_d(false);
	static PRM_Name aov_label(k_aov_label, "Ci");
	static PRM_Name aov_name(k_aov_name, "Ci");
	static PRM_Default aov_name_d(0.0f, "Ci");
	static PRM_Name aov_clear(k_aov_clear, "Clear");
//	static PRM_Name override_image_filename
//	static PRM_Name override_image_format
//	static PRM_Name override_image_bits
	static PRM_Default nb_aovs(1);
	static PRM_Template aov_templates[] =
	{
		PRM_Template(PRM_TOGGLE|PRM_TYPE_LABEL_NONE|PRM_TYPE_JOIN_NEXT, 1, &framebuffer_output, &framebuffer_output_d),
		PRM_Template(PRM_TOGGLE|PRM_TYPE_LABEL_NONE|PRM_TYPE_JOIN_NEXT, 1, &file_output, &file_output_d),
		PRM_Template(PRM_TOGGLE|PRM_TYPE_JOIN_NEXT, 1, &jpeg_output, &jpeg_output_d),
//		PRM_Template(PRM_LABEL|PRM_TYPE_JOIN_NEXT, 1, &aov_label),
//		PRM_Template(PRM_STRING|PRM_TYPE_LABEL_NONE|PRM_TYPE_JOIN_NEXT|PRM_TYPE_INVISIBLE, 1, &aov_name, &aov_name_d),
		PRM_Template(PRM_STRING|PRM_TYPE_LABEL_NONE|PRM_TYPE_JOIN_NEXT, 1, &aov_name, &aov_name_d),
		PRM_Template(PRM_CALLBACK, 1, &aov_clear, 0, 0, 0,
					&ROP_3Delight::aov_clear_cb),
		PRM_Template()
	};

	static PRM_Name add_layer(k_add_layer, "Add...");
	static PRM_Name view_layer(k_view_layer, "View...");
	static PRM_Name dummy("dummy", "");

	static std::vector<PRM_Template> image_layers_templates =
	{
		PRM_Template(PRM_FILE, 1, &default_image_filename, &default_image_filename_d),
		PRM_Template(PRM_STRING|PRM_TYPE_JOIN_NEXT, 1, &default_image_format, &default_image_format_d, &default_image_format_c),
		PRM_Template(PRM_STRING|PRM_TYPE_LABEL_NONE, 1, &default_image_bits, &default_image_bits_d, &default_image_bits_c),
		PRM_Template(PRM_TOGGLE, 1, &save_ids_as_cryptomatte, &save_ids_as_cryptomatte_d),
		PRM_Template(PRM_ORD, 1, &batch_output_mode, &batch_output_mode_d, &batch_output_mode_c),
		PRM_Template(PRM_ORD, 1, &interactive_output_mode, &interactive_output_mode_d, &interactive_output_mode_c),
		PRM_Template(PRM_SEPARATOR, 0, &separator4),
		PRM_Template(PRM_LABEL|PRM_TYPE_JOIN_NEXT, 1, &aovs_titles1),
		PRM_Template(PRM_LABEL, 1, &aovs_titles2),
		PRM_Template(PRM_MultiType(PRM_MULTITYPE_SCROLL|PRM_MULTITYPE_NO_CONTROL_UI), aov_templates, k_one_line*6.0f, &aov, &nb_aovs),
		PRM_Template(PRM_CALLBACK|PRM_TYPE_JOIN_NEXT, 1, &add_layer, 0, 0, 0,
					&ROP_3Delight::add_layer_cb),
		PRM_Template(PRM_CALLBACK|PRM_TYPE_JOIN_NEXT, 1, &view_layer),
		PRM_Template(PRM_LABEL, 1, &dummy),
		PRM_Template(PRM_SEPARATOR, 0, &separator5)
	};

	//// Matte

	static PRM_Name ignore_matte_attribute(k_ignore_matte_attribute, "Ignore Matte Attribute");
	static PRM_Default ignore_matte_attribute_d(false);
	static PRM_Name matte_sets(k_matte_sets, "Matte Sets");

	static std::vector<PRM_Template> matte_templates =
	{
		PRM_Template(PRM_TOGGLE, 1, &ignore_matte_attribute),
		PRM_Template(PRM_STRING, 1, &matte_sets)
	};

	//// Multi-Light

	static PRM_Name light_sets(k_light_sets, "Light Sets");
	static PRM_Name use_light_set(k_use_light_set, "");
	static PRM_Default use_light_set_d(false);
	static PRM_Name light_set(k_light_set, "Light Set");
	static PRM_Default light_set_d(0.0, "");
	static PRM_Default nb_lights(10);
	static PRM_Template light_set_templates[] =
	{
		PRM_Template(PRM_TOGGLE|PRM_TYPE_JOIN_NEXT, 1, &use_light_set, &use_light_set_d),
		PRM_Template(PRM_STRING|PRM_TYPE_LABEL_NONE, 1, &light_set, &light_set_d),
		PRM_Template()
	};

	static PRM_Name display_all_lights(k_display_all_lights, "Display All Lights");
	static PRM_Default display_all_lights_d(false);
	static PRM_Name refresh_lights("refresh_lights", "Refresh");

	static std::vector<PRM_Template> multi_light_templates =
	{
		PRM_Template(PRM_MultiType(PRM_MULTITYPE_SCROLL|PRM_MULTITYPE_NO_CONTROL_UI), light_set_templates, k_one_line*10.0f, &light_sets, &nb_lights),
		PRM_Template(PRM_TOGGLE, 1, &display_all_lights, &display_all_lights_d),
		PRM_Template(PRM_CALLBACK, 1, &refresh_lights, 0, 0, 0,
					&ROP_3Delight::refresh_lights_cb)
	};

	static PRM_Name image_layers_tabs_name("image_layers_tabs");
	static std::vector<PRM_Default> image_layers_tabs =
	{
		PRM_Default(matte_templates.size(), "Matte"),
		PRM_Default(multi_light_templates.size(), "Multi-Light")
	};

	// Overrides

	static PRM_Name speed_boost(k_speed_boost, "Enable Interactive Previewing with Speed Boost\t");
	static PRM_Default speed_boost_d(false);
	static PRM_Name speed_boost_align("speed_boost_align", "\t");

	static PRM_Name disable_motion_blur(k_disable_motion_blur, "Disable Motion Blur");
	static PRM_Default disable_motion_blur_d(false);

	static PRM_Name disable_depth_of_field(k_disable_depth_of_field, "Disable Depth of Field");
	static PRM_Default disable_depth_of_field_d(false);

	static PRM_Name disable_displacement(k_disable_displacement, "Disable Displacement");
	static PRM_Default disable_displacement_d(false);

	static PRM_Name disable_subsurface(k_disable_subsurface, "Disable Subsurface");
	static PRM_Default disable_subsurface_d(false);

	static PRM_Name resolution_factor(k_resolution_factor, "Resolution");
	static PRM_Default resolution_factor_d(0);
	static PRM_Item resolution_factor_i[] =
	{
		PRM_Item("", "Full"),
		PRM_Item("", "Half"),
		PRM_Item("", "Quarter"),
		PRM_Item("", "Eighth"),
		PRM_Item(),
	};
	static PRM_ChoiceList resolution_factor_c(PRM_CHOICELIST_SINGLE, resolution_factor_i);

	static PRM_Name sampling_factor(k_sampling_factor, "Sampling");
	static PRM_Default sampling_factor_d(0.1f);
	static PRM_Item sampling_factor_i[] =
	{
		PRM_Item("", "100%"),
		PRM_Item("", "  25%"),
		PRM_Item("", "  10%"),
		PRM_Item("", "    4%"),
		PRM_Item("", "    1%"),
		PRM_Item(),
	};
	static PRM_ChoiceList sampling_factor_c(PRM_CHOICELIST_SINGLE, sampling_factor_i);

	static PRM_Name overrides_note_spacing("overrides_note_spacing", "");
	static PRM_Name overrides_note("overrides_note", "These settings are ignored in batch rendering");

	static std::vector<PRM_Template> overrides_templates =
	{
		PRM_Template(PRM_TOGGLE|PRM_TYPE_JOIN_NEXT, 1, &speed_boost, &speed_boost_d),
		PRM_Template(PRM_LABEL, 0, &speed_boost_align),
		PRM_Template(PRM_TOGGLE, 1, &disable_motion_blur, &disable_motion_blur_d),
		PRM_Template(PRM_TOGGLE, 1, &disable_depth_of_field, &disable_depth_of_field_d),
		PRM_Template(PRM_TOGGLE, 1, &disable_displacement, &disable_displacement_d),
		PRM_Template(PRM_TOGGLE, 1, &disable_subsurface, &disable_subsurface_d),
		PRM_Template(PRM_SEPARATOR, 0, &separator6),
		PRM_Template(PRM_ORD, 1, &resolution_factor, &resolution_factor_d, &resolution_factor_c),
		PRM_Template(PRM_ORD, 1, &sampling_factor, &sampling_factor_d, &sampling_factor_c),
		PRM_Template(PRM_LABEL, 0, &overrides_note_spacing),
		PRM_Template(PRM_LABEL, 0, &overrides_note)
	};

	// Put everything together

	static PRM_Name main_tabs_name("main_tabs");
	static std::vector<PRM_Default> main_tabs =
	{
		PRM_Default(quality_templates.size(), "Quality"),
		PRM_Default(scene_elements_templates.size(), "Scene Elements"),
		PRM_Default(image_layers_templates.size() + 1, "Image Layers"),
		PRM_Default(overrides_templates.size(), "Overrides")
	};

	static std::vector<PRM_Template> templates;
	if(templates.size() == 0)
	{
#ifdef EXPORT_NSI_CHECKBOX
		templates.push_back(
			PRM_Template(PRM_TOGGLE, 1, &export_nsi, &export_nsi_d));
#endif

		templates.push_back(
			PRM_Template(
				PRM_SWITCHER,
				main_tabs.size(),
				&main_tabs_name,
				&main_tabs[0]));

			// Quality
			templates.insert(
				templates.end(),
				quality_templates.begin(),
				quality_templates.end());

			// Scene Elements
			templates.insert(
				templates.end(),
				scene_elements_templates.begin(),
				scene_elements_templates.end());

			// Image Layers
			templates.insert(
				templates.end(),
				image_layers_templates.begin(),
				image_layers_templates.end());
			// That's what the "+ 1" in the definition of main_tabs is for
			templates.push_back(
				PRM_Template(
					PRM_SWITCHER,
					image_layers_tabs.size(),
					&image_layers_tabs_name,
					&image_layers_tabs[0]));
				templates.insert(
					templates.end(),
					matte_templates.begin(),
					matte_templates.end());
				templates.insert(
					templates.end(),
					multi_light_templates.begin(),
					multi_light_templates.end());

			// Overrides
			templates.insert(
				templates.end(),
				overrides_templates.begin(),
				overrides_templates.end());

		templates.push_back(PRM_Template());
	}

	return &templates[0];
}

static OP_TemplatePair*
GetTemplatePair()
{
	static OP_TemplatePair* ropPair = nullptr;
	if(!ropPair)
	{
		OP_TemplatePair* base;

		base = new OP_TemplatePair(GetTemplates());
		ropPair = new OP_TemplatePair(ROP_Node::getROPbaseTemplate(), base);
	}

	return ropPair;
}

static OP_VariablePair*
GetVariablePair()
{
	static OP_VariablePair* pair = nullptr;
	if(!pair)
	{
		pair = new OP_VariablePair(ROP_Node::myVariableList);
	}

	return pair;
}

void
ROP_3Delight::Register(OP_OperatorTable* io_table)
{
	io_table->addOperator(
		new OP_Operator(
			"3Delight",
			"3Delight",
			ROP_3Delight::alloc,
			GetTemplatePair(),
			0,
			0,
			GetVariablePair(),
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

void
ROP_3Delight::onCreated()
{
	ROP_Node::onCreated();
	UpdateLights();
	enableParm(k_batch_output_mode, false);
	enableParm(k_interactive_output_mode, false);
}

int
ROP_3Delight::aov_clear_cb(void* data, int index, fpreal t,
							const PRM_Template* tplate)
{
	const PRM_Name* name = tplate->getNamePtr();

	std::string token = name->getToken();
	size_t pos = token.find_last_of('_');
	UT_String token_number =
		token.assign(token.begin()+pos+1, token.end()).c_str();
	int number = token_number.toInt();

	ROP_3Delight* node = reinterpret_cast<ROP_3Delight*>(data);

	PRM_Parm& parm = node->getParm(k_aov);
#if 0
	int size = parm.getMultiParmNumItems();

	PRM_Parm* currParm = parm.getMultiParm(i-1);
	assert(currParm);
	PRM_ParmList* pList = currParm->getOwner();
	assert(pList);

	PRM_Parm* label = pList->getParmPtr(aov::getAovLabelToken(i-1));
	assert(label);
	PRM_Template* labelTemp = label->getTemplatePtr();
	assert(labelTemp);
	PRM_Name* labelName = labelTemp->getNamePtr();
	assert(labelName);
	// PATCH: houdini don't update labels correctly, only tokens/values
	for (int j = number; j < size; j++)
	{
		PRM_Parm* currParm2 = parm.getMultiParm(j);
		assert(currParm2);
		PRM_ParmList* pList2 = currParm2->getOwner();
		assert(pList2);

		PRM_Parm* label2 = pList2->getParmPtr(aov::getAovLabelToken(j));
		assert(label2);
		PRM_Template* labelTemp2 = label2->getTemplatePtr();
		assert(labelTemp2);
		PRM_Name* labelName2 = labelTemp2->getNamePtr();
		assert(labelName2);

		labelName->setLabel(labelName2->getLabel());
		labelName = labelName2;
	}
#endif
	parm.removeMultiParmItem(number-1);
	
	return 1;
}

int
ROP_3Delight::add_layer_cb(
	void* data, int index, fpreal t,
	const PRM_Template* tplate)
{
	static SelectLayersDialog dlg;
	if (!dlg.open(reinterpret_cast<ROP_Node*>(data)))
	{
		fprintf(
			stderr,
			"3Delight for Houdini: Could not parse select_layers_ui.ui file\n");
	}

	return 1;
}

int
ROP_3Delight::refresh_lights_cb(void* data, int index, fpreal t,
								const PRM_Template* tplate)
{
	ROP_3Delight* node = reinterpret_cast<ROP_3Delight*>(data);
	node->UpdateLights();
	return 1;
}

bool
ROP_3Delight::HasMotionBlur()const
{
	return
		evalInt(k_motion_blur, 0, 0.0f) &&
		!(HasSpeedBoost() && evalInt(k_disable_motion_blur, 0, 0.0f));
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
	int shading_samples = evalInt(k_shading_samples, 0, 0.0f);
	shading_samples = int(float(shading_samples) * GetSamplingFactor() + 0.5f);
	int volume_samples = evalInt(k_volume_samples, 0, 0.0f);
	i_ctx.m_nsi.SetAttribute(
		".global",
		(
			NSI::IntegerArg("quality.shadingsamples", shading_samples),
			NSI::IntegerArg("quality.volumesamples", volume_samples)
		) );

	int max_diffuse_depth = evalInt(k_max_diffuse_depth, 0, 0.0f);
	int max_reflection_depth = evalInt(k_max_reflection_depth, 0, 0.0f);
	int max_refraction_depth = evalInt(k_max_refraction_depth, 0, 0.0f);
	int max_hair_depth = evalInt(k_max_hair_depth, 0, 0.0f);
	i_ctx.m_nsi.SetAttribute(
		".global",
		(
			NSI::IntegerArg("maximumraydepth.diffuse", max_diffuse_depth),
			NSI::IntegerArg("maximumraydepth.reflection", max_reflection_depth),
			NSI::IntegerArg("maximumraydepth.refraction", max_refraction_depth),
			NSI::IntegerArg("maximumraydepth.hair", max_hair_depth)
		) );

	float max_distance = evalInt(k_max_distance, 0, 0.0f);
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

		if(evalInt(k_disable_displacement, 0, 0.0f))
		{
			i_ctx.m_nsi.SetAttribute(
				".global", NSI::IntegerArg("show.displacement", 0));
		}

		if(evalInt(k_disable_subsurface, 0, 0.0f))
		{
			i_ctx.m_nsi.SetAttribute(
				".global", NSI::IntegerArg("show.osl.subsurface", 0));
		}
	}
}

ROP_3Delight::ROP_3Delight(
	OP_Network* net,
	const char* name,
	OP_Operator* entry)
	:	ROP_Node(net, name, entry)
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


int ROP_3Delight::startRender(int, fpreal tstart, fpreal tend)
{
	m_end_time = tend; // \ref endRender

	NSI::DynamicAPI api;
	NSI::Context nsi(api);

	register_mplay_driver( api );

	bool render = !evalInt(k_export_nsi, 0, 0.0f);

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

	context ctx(
		nsi,
		tstart,
		tend,
		GetShutterInterval(tstart),
		fps,
		HasDepthOfField(),
		preview,
		OP_BundlePattern::allocPattern(GetObjectsToRender()),
		OP_BundlePattern::allocPattern(GetLightsToRender()));

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

	PRM_Parm& parm = getParm(k_aov);
	int size = parm.getMultiParmNumItems();

	if (size > 0) changed |= enableParm("aov_clear_1", size > 1);

	for (int i = 0; i < size; i++)
	{
		changed |= enableParm(aov::getAovStrToken(i), false);
	}

	changed |= enableParm(k_view_layer, false);
	changed |= enableParm(k_display_all_lights, false);

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
	UpdateLights();
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
	i_ctx.m_nsi.Connect(
		"default_screen", "",
		camera::get_nsi_handle(*cam), "screens");

	UT_String driver = "idisplay";
	if(!i_ctx.m_preview)
	{
		evalString(driver, k_default_image_format, 0, 0.0f);
	}

	UT_String image_file_name;
	evalString( image_file_name, k_default_image_filename, 0, 0.0f );

	i_ctx.m_nsi.Create("default_driver", "outputdriver");
	i_ctx.m_nsi.SetAttribute(
		"default_driver",
		(
			NSI::CStringPArg("drivername", driver.c_str()),
			NSI::CStringPArg("imagefilename", image_file_name.c_str())
		) );
	int nb_aovs = evalInt(k_aov, 0, 0.0f);
	unsigned sort_key = 0;

	UT_String scalar_format;
	evalString(scalar_format, k_default_image_bits, 0, 0.0f);

	UT_String filter;
	evalString(filter, k_pixel_filter, 0, 0.0f);
	double filter_width = evalFloat(k_filter_width, 0, 0.0f);

	std::vector<std::string> light_names;
	light_names.push_back(""); // no light for the first one
	GetSelectedLights(light_names);

	for (int i = 0; i < nb_aovs; i++)
	{
		UT_String label;
		evalString(label, aov::getAovStrToken(i), 0, 0.0f);

		const aov::description& desc = aov::getDescription(label.toStdString());

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

			i_ctx.m_nsi.Create(layer_name.c_str(), "outputlayer");
			i_ctx.m_nsi.SetAttribute(
				layer_name.c_str(),
			(
				NSI::CStringPArg("variablename", desc.m_variable_name.c_str()),
				NSI::CStringPArg("variablesource", desc.m_variable_source.c_str()),
				NSI::CStringPArg("scalarformat", scalar_format.c_str()),
				NSI::CStringPArg("layertype", desc.m_layer_type.c_str()),
				NSI::IntegerArg("withalpha", (int)desc.m_with_alpha),
				NSI::CStringPArg("filter", filter.c_str()),
				NSI::DoubleArg("filterwidth", filter_width),
				NSI::IntegerArg("sortkey", sort_key++)
			) );

			if (scalar_format == "uint8")
			{
				i_ctx.m_nsi.SetAttribute(layer_name.c_str(),
					NSI::StringArg("colorprofile", "srgb"));
			}

			i_ctx.m_nsi.Connect(
				layer_name.c_str(), "",
				"default_screen", "outputlayers");
			if (!light_names[j].empty())
			{
				i_ctx.m_nsi.Connect(
					light_names[j].c_str(), "", layer_name.c_str(), "lightset");
			}
			i_ctx.m_nsi.Connect(
				"default_driver", "",
				layer_name.c_str(), "outputdrivers");
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
ROP_3Delight::UpdateLights()
{
	m_lights.clear();

	auto pattern = OP_BundlePattern::allocPattern(GetObjectsToRender());

	scene::find_lights(*pattern, m_lights);

	OP_BundlePattern::freePattern( pattern );

	std::string use_light_prefix = k_use_light_set;
	use_light_prefix.pop_back();
	std::string light_prefix = k_light_set;
	light_prefix.pop_back();

	PRM_Parm& light_sets_parm = getParm(k_light_sets);
	unsigned nb_lights = evalInt(k_light_sets, 0, 0.0f);

	for (unsigned i = 0; i < m_lights.size(); i++)
	{
		// Create a new item if needed
		if (i >= nb_lights)
			light_sets_parm.insertMultiParmItem(light_sets_parm.getMultiParmNumItems());
		char suffix[12] = "";
		::sprintf(suffix, "%d", i+1);
		std::string use_light_token = use_light_prefix + suffix;
		std::string light_token = light_prefix + suffix;

		setString(m_lights[i]->getFullPath(), CH_STRING_LITERAL,
					light_token.c_str(), 0, 0.0);
		setVisibleState(light_token.c_str(), false);

		PRM_Parm* use_light_parm = getParmPtr(use_light_token.c_str());
		assert(use_light_parm);
		PRM_Template* use_light_tmpl = use_light_parm->getTemplatePtr();
		assert(use_light_tmpl);
		PRM_Name* use_light_name = use_light_tmpl->getNamePtr();
		assert(use_light_name);
		use_light_name->setLabel(m_lights[i]->getFullPath().toStdString().c_str());
	}
	// All non used items should be invisible
	nb_lights = evalInt(k_light_sets, 0, 0.0f);
	for (unsigned i = m_lights.size(); i < nb_lights; i++)
	{
		char suffix[12] = "";
		::sprintf(suffix, "%d", i+1);
		std::string use_light_token = use_light_prefix + suffix;
		std::string light_token = light_prefix + suffix;
		setVisibleState(use_light_token.c_str(), false);
		setVisibleState(light_token.c_str(), false);
	}
}

void
ROP_3Delight::GetSelectedLights(std::vector<std::string>& o_light_names) const
{
	std::string use_light_prefix = k_use_light_set;
	use_light_prefix.pop_back();

	unsigned nb_lights = evalInt(k_light_sets, 0, 0.0f);

	for (unsigned i = 0; i < nb_lights; i++)
	{
		char suffix[12] = "";
		::sprintf(suffix, "%d", i+1);
		std::string use_light_token = use_light_prefix + suffix;
		bool selected = evalInt(use_light_token.c_str(), 0, 0.0f);
		if (selected)
		{
			const PRM_Parm* use_light_parm = getParmPtr(use_light_token.c_str());
			assert(use_light_parm);
			const PRM_Template* use_light_tmpl = use_light_parm->getTemplatePtr();
			assert(use_light_tmpl);
			const PRM_Name* use_light_name = use_light_tmpl->getNamePtr();
			assert(use_light_name);
			o_light_names.push_back(use_light_name->getLabel());
		}
	}
}

bool
ROP_3Delight::HasSpeedBoost()const
{
	int speed_boost = evalInt(k_speed_boost, 0, 0.0f);
	return speed_boost;
}

float
ROP_3Delight::GetResolutionFactor()const
{
	if(!HasSpeedBoost())
	{
		return 1.0f;
	}

	int resolution_factor = evalInt(k_resolution_factor, 0, 0.0f);

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

	int sampling_factor = evalInt(k_sampling_factor, 0, 0.0f);

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
	int pixel_samples = evalInt(k_pixel_samples, 0, 0.0f);
	return pixel_samples;
}

OBJ_Camera*
ROP_3Delight::GetCamera()const
{
	UT_String cam_path;
	evalString(cam_path, k_camera, 0, 0.0f);

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
	return !(HasSpeedBoost() && evalInt(k_disable_depth_of_field, 0, 0.0f));
}

UT_String
ROP_3Delight::GetObjectsToRender()const
{
	UT_String objects_pattern("*");
	evalString(objects_pattern, k_objects_to_render, 0, 0.0f);
	return objects_pattern;
}

UT_String
ROP_3Delight::GetLightsToRender()const
{
	UT_String lights_pattern("*");
	evalString(lights_pattern, k_lights_to_render, 0, 0.0f);
	return lights_pattern;
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
