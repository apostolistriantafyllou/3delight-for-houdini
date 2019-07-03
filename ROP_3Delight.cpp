#include "ROP_3Delight.h"

#include <PRM/PRM_Include.h>
#include <ROP/ROP_Error.h>
#include <ROP/ROP_Templates.h>
#include <OP/OP_OperatorTable.h>

#include <UT/UT_DSOVersion.h>


static const float k_one_line = 0.267;

static PRM_Template*
GetTemplates()
{
	static PRM_Name separator1("separator1", "");
	static PRM_Name separator2("separator2", "");
	static PRM_Name separator3("separator3", "");
	static PRM_Name separator4("separator4", "");
	static PRM_Name separator5("separator5", "");


	// Quality

	static PRM_Name shading_samples("shading_samples", "Shading Samples");
	static PRM_Default shading_samples_d(64);
	static PRM_Range shading_samples_r(PRM_RANGE_RESTRICTED, 1, PRM_RANGE_UI, 400);

	static PRM_Name pixel_samples("pixel_samples", "Pixel Samples");
	static PRM_Default pixel_samples_d(8);
	static PRM_Range pixel_samples_r(PRM_RANGE_RESTRICTED, 1, PRM_RANGE_UI, 100);

	static PRM_Name volume_samples("volume_samples", "Volume Samples");
	static PRM_Default volume_samples_d(16);
	static PRM_Range volume_samples_r(PRM_RANGE_RESTRICTED, 1, PRM_RANGE_UI, 400);

	static PRM_Name pixel_filter("pixel_filter", "Pixel Filter");
	static PRM_Default pixel_filter_d(0.0f, "blackmann-harris");
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

	static PRM_Name filter_width("filter_width", "Filter Width");
	static PRM_Default filter_width_d(3.0f);
	static PRM_Range filter_width_r(PRM_RANGE_RESTRICTED, 0.001f, PRM_RANGE_RESTRICTED, 10.0f);

	static PRM_Name motion_blur("motion_blur", "Motion Blur");
	static PRM_Default motion_blur_d(true);

	static PRM_Name max_diffuse_depth("max_diffuse_depth", "Max Diffuse Depth");
	static PRM_Default max_diffuse_depth_d(2);
	static PRM_Range max_diffuse_depth_r(PRM_RANGE_RESTRICTED, 0, PRM_RANGE_UI, 10);

	static PRM_Name max_reflection_depth("max_reflection_depth", "Max Reflection Depth");
	static PRM_Default max_reflection_depth_d(2);
	static PRM_Range max_reflection_depth_r(PRM_RANGE_RESTRICTED, 0, PRM_RANGE_UI, 10);

	static PRM_Name max_refraction_depth("max_refraction_depth", "Max Refraction Depth");
	static PRM_Default max_refraction_depth_d(4);
	static PRM_Range max_refraction_depth_r(PRM_RANGE_RESTRICTED, 0, PRM_RANGE_UI, 10);

	static PRM_Name max_hair_depth("max_hair_depth", "Max Hair Depth");
	static PRM_Default max_hair_depth_d(5);
	static PRM_Range max_hair_depth_r(PRM_RANGE_RESTRICTED, 0, PRM_RANGE_UI, 10);

	static PRM_Name max_distance("max_distance", "Max Distance");
	static PRM_Default max_distance_d(1000.0f);
	static PRM_Range max_distance_r(PRM_RANGE_RESTRICTED, 0.0f, PRM_RANGE_UI, 2000.0f);

	static PRM_Template quality_templates[] =
	{
		PRM_Template(PRM_INT, 1, &shading_samples, &shading_samples_d, nullptr, &shading_samples_r),
		PRM_Template(PRM_INT, 1, &pixel_samples, &pixel_samples_d, nullptr, &pixel_samples_r),
		PRM_Template(PRM_INT, 1, &volume_samples, &volume_samples_d, nullptr, &volume_samples_r),
		PRM_Template(PRM_SEPARATOR, 0, &separator1),
		PRM_Template(PRM_STRING, 1, &pixel_filter, &pixel_filter_d, &pixel_filter_c),
		PRM_Template(PRM_FLT, 1, &filter_width, &filter_width_d, nullptr, &filter_width_r),
		PRM_Template(PRM_SEPARATOR, 0, &separator2),
		PRM_Template(PRM_TOGGLE, 1, &motion_blur, &motion_blur_d),
		PRM_Template(PRM_SEPARATOR, 0, &separator3),
		PRM_Template(PRM_INT, 1, &max_diffuse_depth, &max_diffuse_depth_d, nullptr, &max_diffuse_depth_r),
		PRM_Template(PRM_INT, 1, &max_reflection_depth, &max_reflection_depth_d, nullptr, &max_reflection_depth_r),
		PRM_Template(PRM_INT, 1, &max_refraction_depth, &max_refraction_depth_d, nullptr, &max_refraction_depth_r),
		PRM_Template(PRM_INT, 1, &max_hair_depth, &max_hair_depth_d, nullptr, &max_hair_depth_r),
		PRM_Template(PRM_FLT, 1, &max_distance, &max_distance_d, nullptr, &max_distance_r)
	};

/*
	//? Scene elements

	Camera
	Environment
	Atmosphere
	Objects To Render
	Lights To Render

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

	static PRM_Name default_image_filename("default_image_filename", "Default Image Filename");
	static PRM_Default default_image_filename_d(0.0f, "3delight/<scene>/image/<scene>_<pass>_#.<ext>");

	static PRM_Name default_image_format("default_image_format", "Default Image Format");
	static PRM_Default default_image_format_d(0.0f, "exr");
	static PRM_Item default_image_format_i[] =
	{
		PRM_Item("tiff", "TIFF"),
		PRM_Item("exr", "OpenEXR"),
		PRM_Item("deepexr", "OpenEXR (deep)"),
		PRM_Item("jpeg", "JPEG"),
		PRM_Item("png", "PNGt"),
		PRM_Item(),
	};
	static PRM_ChoiceList default_image_format_c(PRM_CHOICELIST_SINGLE, default_image_format_i);

	static PRM_Name default_image_bits("default_image_bits", "Default Image Bits");
	static PRM_Default default_image_bits_d(0.0f, "float16");
	static PRM_Item default_image_bits_i[] =
	{
		PRM_Item("uint8", "8-bit"),
		PRM_Item("uint16", "16-bit"),
		PRM_Item("float16", "16-bit float"),
		PRM_Item("float32", "32-bit float"),
		PRM_Item(),
	};
	static PRM_ChoiceList default_image_bits_c(PRM_CHOICELIST_SINGLE, default_image_bits_i);

	static PRM_Name save_ids_as_cryptomatte("save_ids_as_cryptomatte", "Save IDs as Cryptomatte");
	static PRM_Default save_ids_as_cryptomatte_d(false);

	static PRM_Name batch_output_mode("batch_output_mode", "Batch Output Mode");
	static PRM_Default batch_output_mode_d(0);
	static PRM_Item batch_output_mode_i[] =
	{
		PRM_Item("", "Enable file output as selected"),
		PRM_Item("", "Enable all file output and selected JPEG"),
		PRM_Item(),
	};
	static PRM_ChoiceList batch_output_mode_c(PRM_CHOICELIST_SINGLE, batch_output_mode_i);

	static PRM_Name interactive_output_mode("interactive_output_mode", "Interactive Output Mode");
	static PRM_Default interactive_output_mode_d(0);
	static PRM_Item interactive_output_mode_i[] =
	{
		PRM_Item("", "Enable file output as selected"),
		PRM_Item("", "Enable file output only for selected layer"),
		PRM_Item("", "Disable file output"),
		PRM_Item(),
	};
	static PRM_ChoiceList interactive_output_mode_c(PRM_CHOICELIST_SINGLE, interactive_output_mode_i);

	static PRM_Name aovs("aovs", "Image Layers");
	static PRM_Name aov("aov", "Image Layer (AOV)");
	static PRM_Name framebuffer_output("framebuffer_output_#", "FB");
	static PRM_Default framebuffer_output_d(true);
	static PRM_Name file_output("file_output_#", "File");
	static PRM_Default file_output_d(true);
	static PRM_Name jpeg_output("jpeg_output_#", "JPG");
	static PRM_Default jpeg_output_d(false);
	static PRM_Name aov_name("aov_name_#", "AOV");
	static PRM_Default aov_name_d(0.0f, "Ci");
//	static PRM_Name override_image_filename
//	static PRM_Name override_image_format
//	static PRM_Name override_image_bits
	static PRM_Default nb_aovs(1);
	static PRM_Template aov_templates[] =
	{
		PRM_Template(PRM_TOGGLE|PRM_TYPE_LABEL_NONE|PRM_TYPE_JOIN_NEXT, 1, &framebuffer_output, &framebuffer_output_d),
		PRM_Template(PRM_TOGGLE|PRM_TYPE_LABEL_NONE|PRM_TYPE_JOIN_NEXT, 1, &file_output, &file_output_d),
		PRM_Template(PRM_TOGGLE|PRM_TYPE_LABEL_NONE|PRM_TYPE_JOIN_NEXT, 1, &jpeg_output, &jpeg_output_d),
		PRM_Template(PRM_STRING|PRM_TYPE_LABEL_NONE, 1, &aov_name, &aov_name_d),
		PRM_Template()
	};

	static PRM_Name add_layer("add_layer", "Add");
	static PRM_Name remove_layer("remove_layer", "Remove");
	static PRM_Name duplicate_layer("duplicate_layer", "Duplicate");
	static PRM_Name view_layer("view_layer", "View...");

	static PRM_Template image_layers_templates[] =
	{
		PRM_Template(PRM_FILE, 1, &default_image_filename, &default_image_filename_d),
		PRM_Template(PRM_STRING|PRM_TYPE_JOIN_NEXT, 1, &default_image_format, &default_image_format_d, &default_image_format_c),
		PRM_Template(PRM_STRING|PRM_TYPE_LABEL_NONE, 1, &default_image_bits, &default_image_bits_d, &default_image_bits_c),
		PRM_Template(PRM_TOGGLE, 1, &save_ids_as_cryptomatte, &save_ids_as_cryptomatte_d),
		PRM_Template(PRM_ORD, 1, &batch_output_mode, &batch_output_mode_d, &batch_output_mode_c),
		PRM_Template(PRM_ORD, 1, &interactive_output_mode, &interactive_output_mode_d, &interactive_output_mode_c),
		PRM_Template(PRM_SEPARATOR, 0, &separator4),
		PRM_Template(PRM_MultiType(PRM_MULTITYPE_SCROLL|PRM_MULTITYPE_NO_CONTROL_UI), aov_templates, k_one_line*4.0f, &aov, &nb_aovs),
		PRM_Template(PRM_CALLBACK|PRM_TYPE_JOIN_NEXT, 1, &add_layer),
		PRM_Template(PRM_CALLBACK|PRM_TYPE_JOIN_NEXT, 1, &remove_layer),
		PRM_Template(PRM_CALLBACK|PRM_TYPE_JOIN_NEXT, 1, &duplicate_layer),
		PRM_Template(PRM_CALLBACK, 1, &view_layer),
		PRM_Template(PRM_SEPARATOR, 0, &separator5)
	};

	//// Matte

	static PRM_Name ignore_matte_attribute("ignore_matte_attribute", "Ignore Matte Attribute");
	static PRM_Default ignore_matte_attribute_d(false);
	static PRM_Name matte_sets("matte_sets", "Matte Sets");

	static PRM_Template matte_templates[] =
	{
		PRM_Template(PRM_TOGGLE, 1, &ignore_matte_attribute),
		PRM_Template(PRM_STRING, 1, &matte_sets)
	};

	//// Multi-Light

	static PRM_Name light_sets("light_sets", "Light Sets");
	static PRM_Name use_light_set("use_light_set_#", "Use Light Set");
	static PRM_Default use_light_set_d(false);
	static PRM_Name light_set("light_set_#", "Light Set");
	static PRM_Default nb_lights(10);
	static PRM_Template light_set_templates[] =
	{
		PRM_Template(PRM_TOGGLE|PRM_TYPE_LABEL_NONE|PRM_TYPE_JOIN_NEXT, 1, &use_light_set, &use_light_set_d),
		PRM_Template(PRM_STRING|PRM_TYPE_LABEL_NONE, 1, &light_set),
		PRM_Template()
	};

	static PRM_Name display_all_lights("display_all_lights", "Display All Lights");
	static PRM_Default display_all_lights_d(false);
	static PRM_Name refresh_lights("refresh_lights", "Refresh");

	static PRM_Template multi_light_templates[] =
	{
		PRM_Template(PRM_MultiType(PRM_MULTITYPE_SCROLL|PRM_MULTITYPE_NO_CONTROL_UI), light_set_templates, k_one_line*10.0f, &light_sets, &nb_lights),
		PRM_Template(PRM_TOGGLE, 1, &display_all_lights, &display_all_lights_d),
		PRM_Template(PRM_CALLBACK, 1, &refresh_lights)
	};

	static PRM_Name image_layers_tabs_name("image_layers_tabs");
	static PRM_Default image_layers_tabs[] =
	{
		PRM_Default(sizeof(matte_templates)/sizeof(matte_templates[0]), "Matte"),
		PRM_Default(sizeof(multi_light_templates)/sizeof(multi_light_templates[0]), "Multi-Light")
	};

	// Overrides

	static PRM_Name speed_boost("speed_boost", "Enable Interactive Previewing with Speed Boost");
	static PRM_Default speed_boost_d(false);

	static PRM_Name disable_motion_blur("disable_motion_blur", "Disable Motion Blur");
	static PRM_Default disable_motion_blur_d(false);

	static PRM_Name disable_depth_of_field("disable_depth_of_field", "Disable Depth of Field");
	static PRM_Default disable_depth_of_field_d(false);

	static PRM_Name disable_displacement("disable_displacement", "Disable Displacement");
	static PRM_Default disable_displacement_d(false);

	static PRM_Name disable_subsurface("disable_subsurface", "Disable Subsurface");
	static PRM_Default disable_subsurface_d(false);

	static PRM_Name resolution_factor("resolution_factor", "Resolution");
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

	static PRM_Name sampling_factor("sampling_factor", "Sampling");
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

	static PRM_Template overrides_templates[] =
	{
		PRM_Template(PRM_TOGGLE, 1, &speed_boost, &speed_boost_d),
		PRM_Template(PRM_TOGGLE, 1, &disable_motion_blur, &disable_motion_blur_d),
		PRM_Template(PRM_TOGGLE, 1, &disable_depth_of_field, &disable_depth_of_field_d),
		PRM_Template(PRM_TOGGLE, 1, &disable_displacement, &disable_displacement_d),
		PRM_Template(PRM_TOGGLE, 1, &disable_subsurface, &disable_subsurface_d),
		PRM_Template(PRM_ORD, 1, &resolution_factor, &resolution_factor_d, &resolution_factor_c),
		PRM_Template(PRM_ORD, 1, &sampling_factor, &sampling_factor_d, &sampling_factor_c)
	};

	// Put everything together

	static PRM_Name main_tabs_name("main_tabs");
	static PRM_Default main_tabs[] =
	{
		PRM_Default(sizeof(quality_templates)/sizeof(quality_templates[0]), "Quality"),
		PRM_Default(sizeof(image_layers_templates)/sizeof(image_layers_templates[0])+1, "Image Layers"),
		PRM_Default(sizeof(overrides_templates)/sizeof(overrides_templates[0]), "Overrides")
	};

	static PRM_Template templates[] =
	{
		PRM_Template(
			PRM_SWITCHER,
			sizeof(main_tabs)/sizeof(main_tabs[0]),
			&main_tabs_name,
			main_tabs),

			quality_templates[0],
			quality_templates[1],
			quality_templates[2],
			quality_templates[3],
			quality_templates[4],
			quality_templates[5],
			quality_templates[6],
			quality_templates[7],
			quality_templates[8],
			quality_templates[9],
			quality_templates[10],
			quality_templates[11],
			quality_templates[12],
			quality_templates[13],

			image_layers_templates[0],
			image_layers_templates[1],
			image_layers_templates[2],
			image_layers_templates[3],
			image_layers_templates[4],
			image_layers_templates[5],
			image_layers_templates[6],
			image_layers_templates[7],
			image_layers_templates[8],
			image_layers_templates[9],
			image_layers_templates[10],
			image_layers_templates[11],
			image_layers_templates[12],

			PRM_Template(
				PRM_SWITCHER,
				sizeof(image_layers_tabs)/sizeof(image_layers_tabs[0]),
				&image_layers_tabs_name,
				image_layers_tabs),

				matte_templates[0],
				matte_templates[1],

				multi_light_templates[0],
				multi_light_templates[1],
				multi_light_templates[2],

			overrides_templates[0],
			overrides_templates[1],
			overrides_templates[2],
			overrides_templates[3],
			overrides_templates[4],
			overrides_templates[5],
			overrides_templates[6],

		PRM_Template()
	};

	return templates;
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
newDriverOperator(OP_OperatorTable *table)
{
	table->addOperator(
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

ROP_3Delight::ROP_3Delight(
	OP_Network* net,
	const char* name,
	OP_Operator* entry)
	:	ROP_Node(net, name, entry)
{
}


ROP_3Delight::~ROP_3Delight()
{
}

int
ROP_3Delight::startRender(int, fpreal tstart, fpreal tend)
{
	end_time = tend;
	if(error() < UT_ERROR_ABORT)
	{
		executePreRenderScript(tstart);
	}

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
		executePostRenderScript(end_time);
	}

	return ROP_CONTINUE_RENDER;
}

