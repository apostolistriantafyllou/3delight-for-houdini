#pragma once

#include "aov.h"
#include <SYS/SYS_Types.h>
#include <UT/UT_String.h>
#include <vector>

class OP_Parameters;
class OP_VariablePair;
class OP_TemplatePair;
class PRM_Template;
class OBJ_Node;
class SelectLayersDialog;

class settings
{
	friend class ROP_3Delight;

public:

	settings( OP_Parameters &i_parameters )
	:
		m_parameters(i_parameters)
	{
	}

	~settings();

	static PRM_Template* GetTemplates();
	static OP_TemplatePair* GetTemplatePair();
	static OP_VariablePair* GetVariablePair();

    static int image_format_cb(
		void* data, int index, fpreal t,
		const PRM_Template* tplate);

	static int aov_clear_cb(
		void* data, int index, fpreal t,
		const PRM_Template* tplate );

	static int add_layer_cb(
		void* data, int index, fpreal t,
		const PRM_Template* tplate);

	static int refresh_lights_cb(
		void* data, int index, fpreal t,
		const PRM_Template* tplate );

public:
	static const char* k_export_nsi;
	static const char* k_shading_samples;
	static const char* k_pixel_samples;
	static const char* k_volume_samples;
	static const char* k_pixel_filter;
	static const char* k_filter_width;
	static const char* k_motion_blur;
	static const char* k_max_diffuse_depth;
	static const char* k_max_reflection_depth;
	static const char* k_max_refraction_depth;
	static const char* k_max_hair_depth;
	static const char* k_max_distance;
	static const char* k_camera;
	static const char* k_atmosphere;
	static const char* k_objects_to_render;
	static const char* k_lights_to_render;
	static const char* k_default_image_filename;
	static const char* k_default_image_format;
	static const char* k_default_image_bits;
	static const char* k_save_ids_as_cryptomatte;
	static const char* k_batch_output_mode;
	static const char* k_interactive_output_mode;
	static const char* k_aovs;
	static const char* k_aov;
	static const char* k_aov_clear;
	static const char* k_add_layer;
	static const char* k_view_layer;
	static const char* k_ignore_matte_attribute;
	static const char* k_matte_sets;
	static const char* k_light_sets;
	static const char* k_use_light_set;
	static const char* k_light_set;
	static const char* k_display_all_lights;
	static const char* k_speed_boost;
	static const char* k_disable_motion_blur;
	static const char* k_disable_depth_of_field;
	static const char* k_disable_displacement;
	static const char* k_disable_subsurface;
	static const char* k_resolution_factor;
	static const char* k_sampling_factor;

protected:

	/**
		\brief Update UI lights from scene lights.
	*/
	void UpdateLights();
	void GetSelectedLights(std::vector<std::string>& o_light_names) const;

	/**
		\brief Returns the use light's token for the specified index
	*/
	static const char* GetUseLightToken(int index);
	static const char* GetLightToken(int index);

	UT_String GetAtmosphere() const;
	UT_String GetObjectsToRender() const;
	UT_String GetLightsToRender() const;

	/** ROP base class containing all the parameters */
	OP_Parameters &m_parameters;

	std::vector<OBJ_Node*> m_lights;
	static SelectLayersDialog* sm_dialog;
};
