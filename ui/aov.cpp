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
