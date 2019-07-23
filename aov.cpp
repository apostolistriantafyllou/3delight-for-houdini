#include "aov.h"

#include <vector>

std::vector<aovs::desc_aov> desc_aovs =
{
	{ aovs::e_shading, "Ci", "Ci", "shader", "color", true, true },
	{ aovs::e_shading, "Diffuse", "diffuse", "shader", "color", false, true },
	{ aovs::e_shading, "Subsurface scattering", "subsurface", "shader", "color", false, true },
	{ aovs::e_shading, "Reflection", "reflection", "shader", "color", false, true },
	{ aovs::e_shading, "Refraction", "refraction", "shader", "color", false, true },
	{ aovs::e_shading, "Volume scattering", "volume", "shader", "color", false, true },
	{ aovs::e_shading, "Incandescence", "rgba", "shader", "color", false, true },
	{ aovs::e_auxiliary, "Z (depth)", "zdepth", "builtin", "scalar", false, false },
	{ aovs::e_auxiliary, "Camera space position", "position", "builtin", "color", false, false },
	{ aovs::e_auxiliary, "Camera space normal", "normal", "builtin", "color", false, false },
	{ aovs::e_auxiliary, "UV", "uv", "builtin", "color", false, false },
	{ aovs::e_auxiliary, "Geometry ID", "geoid", "builtin", "scalar", false, false },
	{ aovs::e_auxiliary, "Scene Path ID", "scenepathid", "builtin", "scalar", false, false },
	{ aovs::e_auxiliary, "Surface Shader ID", "surfaceid", "builtin", "scalar", false, false }
};

const aovs::desc_aov&
aovs::getDescAov(const std::string& i_ui_name)
{
	for (unsigned i = 0; i < desc_aovs.size(); i++)
	{
		if (desc_aovs[i].m_ui_name == i_ui_name)
		{
			return desc_aovs[i];
		}
	}

	static aovs::desc_aov dummy;
	return dummy;
}
