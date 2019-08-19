#include "settings.h"
#include "select_layers_dialog.h"
#include "../scene.h"
#include "../ROP_3Delight.h"

#include <OBJ/OBJ_Node.h>
#include <OP/OP_BundlePattern.h>
#include <OP/OP_OperatorTable.h>
#include <OP/OP_Parameters.h>
#include <PRM/PRM_Include.h>
#include <PRM/PRM_SpareData.h>
#include <ROP/ROP_Node.h>
#include <ROP/ROP_Templates.h>
#include <UT/UT_String.h>

static const float k_one_line = 0.267;

const char* settings::k_export_nsi = "export_nsi";
const char* settings::k_shading_samples = "shading_samples";
const char* settings::k_pixel_samples = "pixel_samples";
const char* settings::k_volume_samples = "volume_samples";
const char* settings::k_pixel_filter = "pixel_filter";
const char* settings::k_filter_width = "filter_width";
const char* settings::k_motion_blur = "motion_blur";
const char* settings::k_max_diffuse_depth = "max_diffuse_depth";
const char* settings::k_max_reflection_depth = "max_reflection_depth";
const char* settings::k_max_refraction_depth = "max_refraction_depth";
const char* settings::k_max_hair_depth = "max_hair_depth";
const char* settings::k_max_distance = "max_distance";
const char* settings::k_camera = "camera";
const char* settings::k_objects_to_render = "objects_to_render";
const char* settings::k_lights_to_render = "lights_to_render";
const char* settings::k_default_image_filename = "default_image_filename";
const char* settings::k_default_image_format = "default_image_format";
const char* settings::k_default_image_bits = "default_image_bits";
const char* settings::k_save_ids_as_cryptomatte = "save_ids_as_cryptomatte";
const char* settings::k_batch_output_mode = "batch_output_mode";
const char* settings::k_interactive_output_mode = "interactive_output_mode";
const char* settings::k_aovs = "aovs";
const char* settings::k_aov = "aov";
const char* settings::k_aov_clear = "aov_clear_#";
const char* settings::k_add_layer = "add_layer";
const char* settings::k_view_layer = "view_layer";
const char* settings::k_ignore_matte_attribute = "ignore_matte_attribute";
const char* settings::k_matte_sets = "matte_sets";
const char* settings::k_light_sets = "light_sets";
const char* settings::k_use_light_set = "use_light_set_#";
const char* settings::k_light_set = "light_set_#";
const char* settings::k_display_all_lights = "display_all_lights";
const char* settings::k_speed_boost = "speed_boost";
const char* settings::k_disable_motion_blur = "disable_motion_blur";
const char* settings::k_disable_depth_of_field = "disable_depth_of_field";
const char* settings::k_disable_displacement = "disable_displacement";
const char* settings::k_disable_subsurface = "disable_subsurface";
const char* settings::k_resolution_factor = "resolution_factor";
const char* settings::k_sampling_factor = "sampling_factor";

PRM_Template* settings::GetTemplates()
{
	static PRM_Name separator1("separator1", "");
	static PRM_Name separator2("separator2", "");
	static PRM_Name separator3("separator3", "");
	static PRM_Name separator4("separator4", "");
	static PRM_Name separator5("separator5", "");
	static PRM_Name separator6("separator6", "");

	// Quality
	static PRM_Name shading_samples(k_shading_samples, "Shading Samples");
	static PRM_Default shading_samples_d(64);
	static PRM_Range shading_samples_r( PRM_RANGE_RESTRICTED, 1, PRM_RANGE_UI, 400 );

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
		0.0f, "$HIP/render/`$HIPNAME`_`$OS`_$F4.exr");

	static PRM_Name default_image_format(k_default_image_format, "Image Format");
	static PRM_Default default_image_format_d(0.0f, "exr");
	static PRM_Item default_image_format_i[] =
	{
		PRM_Item("tiff", "TIFF"),
		PRM_Item("exr", "OpenEXR"),
		PRM_Item("deepexr", "OpenEXR (deep)"),
//		PRM_Item("jpeg", "JPEG"),
		PRM_Item("png", "PNG"),
		PRM_Item(),
	};
	static PRM_ChoiceList default_image_format_c(PRM_CHOICELIST_SINGLE, default_image_format_i);

	static PRM_Name default_image_bits(k_default_image_bits, "Image Bits");
	static PRM_Default default_image_bits_d(0.0f, "half");
	static PRM_Item default_image_bits_i[] =
	{
//		PRM_Item("uint8", "8-bit"),
//		PRM_Item("uint16", "16-bit"),
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
	static PRM_Default interactive_output_mode_d(2);
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
	static PRM_Name framebuffer_output(aov::getAovFrameBufferOutputStr(), "Preview");
	static PRM_Default framebuffer_output_d(true);
	static PRM_Name file_output(aov::getAovFileOutputStr(), "File");
	static PRM_Default file_output_d(true);
	static PRM_Name jpeg_output(aov::getAovJpegOutputStr(), " ");
	static PRM_Default jpeg_output_d(false);
	static PRM_Name aov_label(aov::getAovLabelStr(), "Ci");
	static PRM_Name aov_name(aov::getAovStrStr(), "Ci");
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
					&settings::aov_clear_cb),
		PRM_Template()
	};

	static PRM_Name add_layer(k_add_layer, "Add...");
	static PRM_Name view_layer(k_view_layer, "View...");
	static PRM_Name dummy("dummy", "");

	static std::vector<PRM_Template> image_layers_templates =
	{
		PRM_Template(PRM_FILE, 1, &default_image_filename, &default_image_filename_d),
		PRM_Template(PRM_STRING|PRM_TYPE_JOIN_NEXT, 1, &default_image_format, &default_image_format_d, &default_image_format_c, 0, &settings::image_format_cb),
		PRM_Template(PRM_STRING|PRM_TYPE_LABEL_NONE, 1, &default_image_bits, &default_image_bits_d, &default_image_bits_c),
		PRM_Template(PRM_TOGGLE, 1, &save_ids_as_cryptomatte, &save_ids_as_cryptomatte_d),
		PRM_Template(PRM_ORD, 1, &batch_output_mode, &batch_output_mode_d, &batch_output_mode_c),
		PRM_Template(PRM_ORD, 1, &interactive_output_mode, &interactive_output_mode_d, &interactive_output_mode_c),
		PRM_Template(PRM_SEPARATOR, 0, &separator4),
		PRM_Template(PRM_LABEL|PRM_TYPE_JOIN_NEXT, 1, &aovs_titles1),
		PRM_Template(PRM_LABEL, 1, &aovs_titles2),
		PRM_Template(PRM_MultiType(PRM_MULTITYPE_SCROLL|PRM_MULTITYPE_NO_CONTROL_UI), aov_templates, k_one_line*6.0f, &aov, &nb_aovs),
		PRM_Template(PRM_CALLBACK|PRM_TYPE_JOIN_NEXT, 1, &add_layer, 0, 0, 0,
					&settings::add_layer_cb),
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
	static PRM_Default nb_lights(1);
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
					&settings::refresh_lights_cb)
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

	// Debug

	static PRM_Name export_nsi(k_export_nsi, "Export NSI to stdout");
	static PRM_Default export_nsi_d(false);

	static std::vector<PRM_Template> debug_templates =
	{
		PRM_Template(PRM_TOGGLE, 1, &export_nsi, &export_nsi_d)
	};

	// Put everything together

	static PRM_Name main_tabs_name("main_tabs");
	static std::vector<PRM_Default> main_tabs =
	{
		PRM_Default(quality_templates.size(), "Quality"),
		PRM_Default(scene_elements_templates.size(), "Scene Elements"),
		PRM_Default(image_layers_templates.size() + 1, "Image Layers"),
		PRM_Default(overrides_templates.size(), "Overrides"),
		PRM_Default(debug_templates.size(), "Debug")
	};

	static std::vector<PRM_Template> templates;
	if(templates.size() == 0)
	{
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

			// Debug
			templates.insert(
				templates.end(),
				debug_templates.begin(),
				debug_templates.end());

		templates.push_back(PRM_Template());
	}

	return &templates[0];
}

OP_TemplatePair* settings::GetTemplatePair()
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

OP_VariablePair* settings::GetVariablePair()
{
	static OP_VariablePair* pair = nullptr;
	if(!pair)
	{
		pair = new OP_VariablePair(ROP_Node::myVariableList);
	}

	return pair;
}

int settings::image_format_cb(
	void* data, int index, fpreal t,
	const PRM_Template* tplate )
{
	static PRM_Item default_image_bits_i_tiff[] =
	{
		PRM_Item("uint8", "8-bit"),
		PRM_Item("uint16", "16-bit"),
		PRM_Item("float", "32-bit float"),
		PRM_Item(),
	};
	static PRM_ChoiceList default_image_bits_c_tiff(
		PRM_CHOICELIST_SINGLE, default_image_bits_i_tiff);

	static PRM_Item default_image_bits_i_exr[] =
	{
		PRM_Item("half", "16-bit float"),
		PRM_Item("float", "32-bit float"),
		PRM_Item(),
	};
	static PRM_ChoiceList default_image_bits_c_exr(
		PRM_CHOICELIST_SINGLE, default_image_bits_i_exr);

	static PRM_Item default_image_bits_i_png[] =
	{
		PRM_Item("uint8", "8-bit"),
		PRM_Item(),
	};
	static PRM_ChoiceList default_image_bits_c_png(
		PRM_CHOICELIST_SINGLE, default_image_bits_i_png);

	OP_Parameters* node = reinterpret_cast<OP_Parameters*>(data);

	UT_String image_format;
	node->evalString(image_format, k_default_image_format, 0, 0.0f);

	PRM_Parm& parm = node->getParm(k_default_image_bits);
	PRM_Template* parmTemp = parm.getTemplatePtr();
	assert(parmTemp);

	if (image_format == "tiff")
	{
		node->setString("uint8", CH_STRING_LITERAL, k_default_image_bits, 0, 0.0f);
		parmTemp->setChoiceListPtr(&default_image_bits_c_tiff);
	}
	else if (image_format == "exr" || image_format == "deepexr")
	{
		node->setString("half", CH_STRING_LITERAL, k_default_image_bits, 0, 0.0f);
		parmTemp->setChoiceListPtr(&default_image_bits_c_exr);
	}
	else if (image_format == "jpeg" || image_format == "png")
	{
		node->setString("uint8", CH_STRING_LITERAL, k_default_image_bits, 0, 0.0f);
		parmTemp->setChoiceListPtr(&default_image_bits_c_png);
	}

	return 1;
}

int settings::aov_clear_cb(
	void* data, int index, fpreal t,
	const PRM_Template* tplate )
{
	const PRM_Name* name = tplate->getNamePtr();

	std::string token = name->getToken();
	size_t pos = token.find_last_of('_');
	UT_String token_number =
		token.assign(token.begin()+pos+1, token.end()).c_str();
	int number = token_number.toInt();

	OP_Parameters* node = reinterpret_cast<OP_Parameters*>(data);
	assert( node );
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

int settings::add_layer_cb(
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

int settings::refresh_lights_cb(
	void* data, int index, fpreal t,
	const PRM_Template* tplate )
{
	ROP_3Delight* node = reinterpret_cast<ROP_3Delight*>(data);
	node->m_settings.UpdateLights();
	return 1;
}

void settings::GetSelectedLights(
	std::vector<std::string>& o_light_names ) const
{
	unsigned nb_lights = m_parameters.evalInt(k_light_sets, 0, 0.0f);

	for (unsigned i = 0; i < nb_lights; i++)
	{
		const char* use_light_token = GetUseLightToken(i);
		bool selected = m_parameters.evalInt(use_light_token, 0, 0.0f);
		if (selected)
		{
			const PRM_Parm* use_light_parm = m_parameters.getParmPtr(use_light_token);
			assert(use_light_parm);
			const PRM_Template* use_light_tmpl =
				use_light_parm->getTemplatePtr();
			assert(use_light_tmpl);
			const PRM_Name* use_light_name = use_light_tmpl->getNamePtr();
			assert(use_light_name);
			o_light_names.push_back(use_light_name->getLabel());
		}
	}
}

void settings::UpdateLights()
{
	m_lights.clear();

	auto pattern = OP_BundlePattern::allocPattern(GetLightsToRender());

	scene::find_lights(*pattern, m_lights);

	OP_BundlePattern::freePattern( pattern );

	PRM_Parm& light_sets_parm = m_parameters.getParm(k_light_sets);
	unsigned nb_lights = m_parameters.evalInt(k_light_sets, 0, 0.0f);

	// The only way I know to modify the label of a specific toggle is to
	// remove the multi item and rebuilds it! Using setLabel of an existing
	// PRM_Name change the label but not what you see on the screen!
	UT_Map<std::string, bool> selectedLights;
	for (int i = nb_lights-1; i >= 0; i--)
	{
		const char* use_light_token = GetUseLightToken(i);
		bool selected = m_parameters.evalInt(use_light_token, 0, 0.0f);
		PRM_Parm* use_light_parm = m_parameters.getParmPtr(use_light_token);
		assert(use_light_parm);

		selectedLights[use_light_parm->getLabel()] = selected;
		light_sets_parm.removeMultiParmItem(i);
	}

	for (unsigned i = 0; i < m_lights.size(); i++)
	{
		// Create a new item
		light_sets_parm.insertMultiParmItem(light_sets_parm.getMultiParmNumItems());
		const char* light_token = GetLightToken(i);

		m_parameters.setString(
			m_lights[i]->getFullPath(), CH_STRING_LITERAL, light_token, 0, 0.0);
		m_parameters.setVisibleState(light_token, false);

		std::string light_name = m_lights[i]->getFullPath().toStdString();
		bool selected = false;
		UT_Map<std::string, bool>::iterator sel_item =
			selectedLights.find(light_name);
		if (sel_item != selectedLights.end())
		{
			selected = sel_item->second;
			selectedLights.erase(light_name);
		}

		const char* use_light_token = GetUseLightToken(i);
		m_parameters.setInt(use_light_token, 0, 0.0f, (exint)selected);

		PRM_Parm* use_light_parm = m_parameters.getParmPtr(use_light_token);
		assert(use_light_parm);
		PRM_Template* use_light_tmpl = use_light_parm->getTemplatePtr();
		assert(use_light_tmpl);
		PRM_Name* use_light_name = use_light_tmpl->getNamePtr();
		assert(use_light_name);
		use_light_name->setLabel(light_name.c_str());
	}
	selectedLights.clear();
}

const char* settings::GetUseLightToken(int index)
{
	static std::string use_light_token;
	use_light_token = settings::k_use_light_set;
	use_light_token.pop_back();

	char suffix[12] = "";
	::sprintf(suffix, "%d", index+1);
	use_light_token += suffix;

	return use_light_token.c_str();
}

const char* settings::GetLightToken(int index)
{
	static std::string light_token;
	light_token = settings::k_light_set;
	light_token.pop_back();

	char suffix[12] = "";
	::sprintf(suffix, "%d", index+1);
	light_token += suffix;

	return light_token.c_str();
}

UT_String settings::GetObjectsToRender() const
{
	UT_String objects_pattern("*");
	m_parameters.evalString(objects_pattern, settings::k_objects_to_render, 0, 0.0f);
	return objects_pattern;
}

UT_String settings::GetLightsToRender() const
{
	UT_String lights_pattern("*");
	m_parameters.evalString(lights_pattern, settings::k_lights_to_render, 0, 0.0f);
	return lights_pattern;
}
