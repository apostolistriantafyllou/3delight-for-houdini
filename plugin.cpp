#include "OBJ_IncandescenceLight.h"
#include "ROP_3Delight.h"
#include "VOP_3DelightMaterialBuilder.h"
#include "creation_callbacks.h"
#include "shader_library.h"

#include <HOM/HOM_Module.h>
#include <HOM/HOM_ui.h>
#include <UT/UT_DSOVersion.h>
#include <UT/UT_Version.h>

static void check_houdini_version()
{
	const char build_version[] =
		SYS_VERSION_MAJOR "." SYS_VERSION_MINOR "." SYS_VERSION_BUILD;
	std::string runtime_version = ::HOM().applicationVersionString();

	if(runtime_version != build_version)
	{
		std::cerr
			<< "3Delight for Houdini: This 3Delight package is configured to be "
				"used with Houdini " << build_version << ".\n"
			<< "Using it with another version of Houdini (currently, "
			<< runtime_version << ") might result in crashes and other problems."
			<< std::endl;
	}
}

extern "C" SYS_VISIBILITY_EXPORT void
HoudiniDSOInit(UT_DSOInfo&)
{
	check_houdini_version();
}

extern "C" SYS_VISIBILITY_EXPORT void
newDriverOperator(OP_OperatorTable* io_table)
{
	ROP_3Delight::Register(io_table);
	creation_callbacks::init();
}

extern "C" SYS_VISIBILITY_EXPORT void
newVopOperator(OP_OperatorTable* io_table)
{
	VOP_3DelightMaterialBuilder::Register(io_table);
	shader_library::get_instance().Register(io_table);
}

extern "C" SYS_VISIBILITY_EXPORT void
newObjectOperator(OP_OperatorTable* io_table)
{
	OBJ_IncandescenceLight::Register(io_table);
}

