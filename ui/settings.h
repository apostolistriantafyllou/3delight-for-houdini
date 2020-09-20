#pragma once

#include "aov.h"
#include <SYS/SYS_Types.h>
#include <UT/UT_String.h>
#include <vector>

class ROP_3Delight;
class OP_VariablePair;
class OP_TemplatePair;
class PRM_Template;
class OBJ_Node;
class SelectLayersDialog;

class settings
{
	friend class ROP_3Delight;

public:

	settings( ROP_3Delight &i_rop );

	~settings();

	/// Notifies the object of the ROP's rendering state
	void Rendering(bool i_render, bool i_ipr);

	static PRM_Template* GetTemplates(bool i_cloud, bool standin);
	static OP_TemplatePair* GetTemplatePair(bool i_cloud,bool standin);
	static OP_VariablePair* GetVariablePair();
	static PRM_Template* GetObsoleteParameters();

    static int image_format_cb(
		void* data, int index, fpreal t,
		const PRM_Template* tplate);

	static int aov_clear_cb(
		void* data, int index, fpreal t,
		const PRM_Template* tplate );

	static int add_layer_cb(
		void* data, int index, fpreal t,
		const PRM_Template* tplate);

	static int setSave(
		void* data, int index, fpreal t,
		const PRM_Template* tplate);

	static int setDisplay(
		void* data, int index, fpreal t,
		const PRM_Template* tplate);

	static int setSaveDislay(
		void* data, int index, fpreal t,
		const PRM_Template* tplate);

	static int setOutput(
		void* data, int index, fpreal t,
		const PRM_Template* tplate);

	static int ipr_render(
		void* data, int index, fpreal t,
		const PRM_Template* tplate);

	static int sequence_render(
		void* data, int index, fpreal t,
		const PRM_Template* tplate);

	static int export_standin(
		void* data, int index, fpreal t,
		const PRM_Template* tplate);

	static int export_standin_sequence(
		void* data, int index, fpreal t,
		const PRM_Template* tplate);

	UT_String GetObjectsToRender( fpreal ) const;
	UT_String GetLightsToRender( fpreal ) const;
	UT_String get_matte_objects( fpreal ) const;
	bool OverrideDisplayFlags( fpreal )const;

public:

	static const char* k_rendering;
	static const char* k_old_render_mode;
	static const char* k_ipr_rendering;
	static const char* k_ipr_start;
	static const char* k_export_file;
	static const char* k_export_sequence_file;
	static const char* k_sequence_rendering;
	static const char* k_sequence_start;
	static const std::string k_rm_render;
	static const std::string k_rm_live_render;
	static const std::string k_rm_viewport_render;
	static const std::string k_rm_export_file;
	static const std::string k_rm_export_archive;
	static const std::string k_rm_export_stdout;
	static const char* k_stop_render;
	static const char* k_export;
	static const char* k_export_sequence;
	static const char* k_old_export_nsi;
	static const char* k_old_ipr;
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
	static const char* k_override_display_flags;
	static const char* k_objects_to_render;
	static const char* k_lights_to_render;
	static const char* k_matte_objects;
	static const char* k_default_image_filename;
	static const char* k_default_image_format;
	static const char* k_default_image_bits;
	static const char* k_batch_output_mode;
	static const char* k_interactive_output_mode;
	static const char* k_display_rendered_images;
	static const char* k_display_and_save_rendered_images;
	static const char* k_save_rendered_images;
	static const char* k_save_jpeg_copy;
	static const char* k_output_nsi_files;
	static const char* k_output_standin;
	static const char* k_export_standin;
	static const char* k_aovs;
	static const char* k_aov;
	static const char* k_aov_clear;
	static const char* k_add_layer;
	static const char* k_view_layer;
	static const char* k_enable_multi_light;
	static const char* k_speed_boost;
	static const char* k_disable_motion_blur;
	static const char* k_disable_depth_of_field;
	static const char* k_disable_displacement;
	static const char* k_disable_subsurface;
	static const char* k_disable_atmosphere;
	static const char* k_disable_multiple_scattering;
	static const char* k_disable_extra_image_layers;
	static const char* k_resolution_factor;
	static const char* k_sampling_factor;
	static const char* k_default_export_nsi_filename;

private:

	/**
		\brief Get lights from scene lights.
	*/
	void GetLights(std::vector<OBJ_Node*>& o_lights, fpreal t ) const;

	UT_String GetAtmosphere( fpreal t ) const;
	bool EnableMultiLight( fpreal t )const;
	UT_String get_render_mode( fpreal t )const;
	bool export_to_nsi( fpreal t )const;

	/// Called when the Abort button is pressed
	static int StopRenderCB(void* i_node, int, double, const PRM_Template*);

	/** ROP containing all the parameters */
	ROP_3Delight& m_parameters;

	static SelectLayersDialog* sm_dialog;
};
