#include "aov.h"

#include <vector>

std::vector<aov::description> descriptions =
{
	{ aov::e_shading, "Ci", "rgba", "Ci", "shader", "color", true, true },
	{ aov::e_shading, "Diffuse", "diffuse", "diffuse", "shader", "color", false, true },
	{ aov::e_shading, "Subsurface scattering", "subsurface", "subsurface", "shader", "color", false, true },
	{ aov::e_shading, "Reflection", "reflection", "reflection", "shader", "color", false, true },
	{ aov::e_shading, "Refraction", "refraction", "refraction", "shader", "color", false, true },
	{ aov::e_shading, "Volume scattering", "volume", "volume", "shader", "color", false, true },
	{ aov::e_shading, "Incandescence", "incandescence", "incandescence", "shader", "color", false, true },
	{ aov::e_auxiliary, "Z (depth)", "zdepth", "z", "builtin", "scalar", false, false },
	{ aov::e_auxiliary, "Camera space position", "position", "P.camera", "builtin", "color", false, false },
	{ aov::e_auxiliary, "Camera space normal", "normal", "N.camera", "builtin", "color", false, false },
	{ aov::e_auxiliary, "UV", "uv", "uv", "builtin", "color", false, false },
	{ aov::e_auxiliary, "Geometry ID", "geoid", "id.geometry", "builtin", "scalar", false, false },
	{ aov::e_auxiliary, "Scene Path ID", "scenepathid", "id.scenepath", "builtin", "scalar", false, false },
	{ aov::e_auxiliary, "Surface Shader ID", "surfaceid", "id.surfaceshader", "builtin", "scalar", false, false }
};

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

static const char* k_framebuffer_output = "framebuffer_output_#";
static const char* k_file_output = "file_output_#";
static const char* k_jpeg_output = "jpeg_output_#";
static const char* k_aov_label = "aov_label_#";
static const char* k_aov_name = "aov_name_#";

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
aov::getAovFrameBufferOutputToken(int index)
{
	static std::string framebuffer_output_token;
	framebuffer_output_token = k_framebuffer_output;
	framebuffer_output_token.pop_back();

	char suffix[12] = "";
	::sprintf(suffix, "%d", index+1);
	framebuffer_output_token += suffix;

	return framebuffer_output_token.c_str();
}

const char*
aov::getAovFileOutputToken(int index)
{
	static std::string file_output_token;
	file_output_token = k_file_output;
	file_output_token.pop_back();

	char suffix[12] = "";
	::sprintf(suffix, "%d", index+1);
	file_output_token += suffix;

	return file_output_token.c_str();
}

const char*
aov::getAovJpegOutputToken(int index)
{
	static std::string jpeg_output_token;
	jpeg_output_token = k_jpeg_output;
	jpeg_output_token.pop_back();

	char suffix[12] = "";
	::sprintf(suffix, "%d", index+1);
	jpeg_output_token += suffix;

	return jpeg_output_token.c_str();
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
