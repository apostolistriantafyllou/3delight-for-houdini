#include "aov.h"

#include <VOP/VOP_Node.h>

namespace
{

/*
	******* WARNING *******

	This list of predefined AOVs should match the one defined in file
	select_layers_ui.ui

	******* WARNING *******
*/
std::vector<aov::description> descriptions =
{
	{ aov::e_shading, "Ci", "rgba", "Ci", "shader", "color", true, true },
	{ aov::e_shading, "Ci (direct)", "direct", "Ci.direct", "shader", "color", false, true },
	{ aov::e_shading, "Ci (indirect)", "indirect", "Ci.indirect", "shader", "color", false, true },
	{ aov::e_shading, "Diffuse", "diffuse", "diffuse", "shader", "color", false, true },
	{ aov::e_shading, "Diffuse (direct)", "directdiffuse", "diffuse.direct", "shader", "color", false, true },
	{ aov::e_shading, "Diffuse (indirect)", "indirectdiffuse", "diffuse.indirect", "shader", "color", false, true },
	{ aov::e_shading, "Subsurface Scattering", "subsurface", "subsurface", "shader", "color", false, true },
	{ aov::e_shading, "Reflection", "reflection", "reflection", "shader", "color", false, true },
	{ aov::e_shading, "Reflection (direct)", "directreflection", "reflection.direct", "shader", "color", false, true },
	{ aov::e_shading, "Reflection (indirect)", "indirectreflection", "reflection.indirect", "shader", "color", false, true },
	{ aov::e_shading, "Refraction", "refraction", "refraction", "shader", "color", false, true },
	{ aov::e_shading, "Volume Scattering", "volume", "volume", "shader", "color", false, true },
	{ aov::e_shading, "Volume Scattering (direct)", "directvolume", "volume.direct", "shader", "color", false, true },
	{ aov::e_shading, "Volume Scattering (indirect)", "indirectvolume", "volume.indirect", "shader", "color", false, true },
	{ aov::e_shading, "Incandescence", "incandescence", "incandescence", "shader", "color", false, true },
	{ aov::e_shading, "Toon Base", "toon_base", "toon_base", "shader", "color", false, false },
	{ aov::e_shading, "Toon Diffuse", "toon_diff", "toon_diffuse", "shader", "color", false, false },
	{ aov::e_shading, "Toon Specular", "toon_spec", "toon_specular", "shader", "color", false, false },
	{ aov::e_shading, "Toon Matte", "toon_matte", "toon_matte", "shader", "color", false, false },
	{ aov::e_shading, "Outlines", "outlines", "outlines", "shader", "quad", false, false },
	{ aov::e_auxiliary, "Albedo", "albedo", "albedo", "shader", "color", false, false },
	{ aov::e_auxiliary, "Z (depth)", "zdepth", "z", "builtin", "scalar", false, false },
	{ aov::e_auxiliary, "Camera Space Position", "position", "P.camera", "builtin", "vector", false, false },
	{ aov::e_auxiliary, "Camera Space Normal", "normal", "N.camera", "builtin", "vector", false, false },
	{ aov::e_auxiliary, "World Space Position", "position", "P.world", "builtin", "vector", false, false },
	{ aov::e_auxiliary, "World Space Normal", "normal", "N.world", "builtin", "vector", false, false },
	{ aov::e_auxiliary, "Shadow Mask", "shadow_mask", "shadow_mask", "shader", "color", false, false },
	{ aov::e_auxiliary, "UV", "st", "st", "attribute", "vector", false, false },
	{ aov::e_auxiliary, "Geometry Cryptomatte", "geoid", "id.geometry", "builtin", "scalar", false, false },
	{ aov::e_auxiliary, "Scene Path Cryptomatte", "scenepathid", "id.scenepath", "builtin", "scalar", false, false },
	{ aov::e_auxiliary, "Surface Shader Cryptomatte", "surfaceid", "id.surfaceshader", "builtin", "scalar", false, false },
	{ aov::e_auxiliary, "Relighting Multiplier", "relightingmultiplier", "relighting_multiplier", "shader", "color", false, false },
	{ aov::e_auxiliary, "Relighting Reference", "relightingref", "relighting_reference", "shader", "color", false, false },
	{ aov::e_auxiliary, "Motion Vector", "motion", "motionvector", "builtin", "vector", false, false },
	{ aov::e_auxiliary, "Ambient Occlusion", "occlusion", "occlusion", "shader", "color", false, false }
};

/*
	The number of AOVs may change as custom ones are added, but the number of
	predefined AOVs corresponds to the initial length of vector "descriptions".
*/
const unsigned predefined_aovs = descriptions.size();

}

void
aov::updateCustomVariables(const std::vector<VOP_Node*>& i_custom_aovs)
{
	removeAllCustomVariables();
	for (unsigned i = 0; i < i_custom_aovs.size(); i++)
	{
		for (int j = 0; j < i_custom_aovs[i]->getNumVisibleInputs(); j++)
		{
			UT_String aov_name = i_custom_aovs[i]->inputLabel(j);
			if (!findCustomVariable(aov_name.c_str()))
			{
				addCustomVariable(
					aov_name.toStdString(),
					aov_name.toStdString(),
					aov_name.toStdString(),
					"shader", "color");
			}
		}
	}
}

void
aov::removeAllCustomVariables()
{
	descriptions.resize(predefined_aovs);
}

void
aov::addCustomVariable(
	const std::string& i_ui_name,
	const std::string& i_filename_token,
	const std::string& i_variable_name,
	const std::string& i_variable_source,
	const std::string& i_layer_type)
{
	aov::description desc =
	{
		aov::e_custom,
		i_ui_name,
		i_filename_token,
		i_variable_name,
		i_variable_source,
		i_layer_type,
		false,
		false
	};
	descriptions.push_back(desc);
}

bool
aov::findCustomVariable(const std::string& i_aov_name)
{
	for (int i = predefined_aovs ; i < descriptions.size(); i++)
	{
		assert(descriptions[i].m_type == aov::e_custom);

		if (descriptions[i].m_ui_name == i_aov_name)
		{
			return true;
		}
	}

	return false;
}

unsigned aov::nbPredefined()
{
	return predefined_aovs;
}

const aov::description&
aov::getDescription(unsigned i_index)
{
	return descriptions[i_index];
}

const aov::description&
aov::getDescription(const std::string& i_ui_name)
{
	for (unsigned i = 0; i < descriptions.size(); i++)
	{
		if (descriptions[i].m_ui_name == i_ui_name)
		{
			return descriptions[i];
		}
	}

	static aov::description dummy;
	return dummy;
}

static const char* k_active_layer = "active_layer_#";
static const char* k_framebuffer_output = "framebuffer_output_#";
static const char* k_file_output = "file_output_#";
static const char* k_jpeg_output = "jpeg_output_#";
static const char* k_aov_label = "aov_label_#";
static const char* k_aov_name = "aov_name_#";

const char*
aov::getActiveLayerOutputStr()
{
	return k_active_layer;
}

const char*
aov::getAovFrameBufferOutputStr()
{
	return k_framebuffer_output;
}

const char*
aov::getAovFileOutputStr()
{
	return k_file_output;
}

const char*
aov::getAovJpegOutputStr()
{
	return k_jpeg_output;
}

const char*
aov::getAovLabelStr()
{
	return k_aov_label;
}

const char*
aov::getAovStrStr()
{
	return k_aov_name;
}

const char*
aov::getAovActiveLayerToken(int index)
{
	static std::string framebuffer_output_token;
	framebuffer_output_token = k_active_layer;
	framebuffer_output_token.pop_back();

	char suffix[12] = "";
	::sprintf(suffix, "%d", index+1);
	framebuffer_output_token += suffix;

	return framebuffer_output_token.c_str();
}

const char*
aov::getAovLabelToken(int index)
{
	static std::string label_token;
	label_token = k_aov_label;
	label_token.pop_back();

	char suffix[12] = "";
	::sprintf(suffix, "%d", index+1);
	label_token += suffix;

	return label_token.c_str();
}

const char*
aov::getAovStrToken(int index)
{
	static std::string str_token;
	str_token = k_aov_name;
	str_token.pop_back();

	char suffix[12] = "";
	::sprintf(suffix, "%d", index+1);
	str_token += suffix;

	return str_token.c_str();
}
