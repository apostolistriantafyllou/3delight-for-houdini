#include "OBJ_IncandescenceLight.h"
#include "ROP_3Delight.h"
#include "VOP_3DelightMaterialBuilder.h"
#include "VOP_AOVGroup.h"
#include "creation_callbacks.h"
#include "shader_library.h"
#include "viewport_hook.h"

#include <DM/DM_RenderTable.h>
#include <HOM/HOM_Module.h>
#include <HOM/HOM_ui.h>
#include <UT/UT_DSOVersion.h>
#include <UT/UT_Version.h>

static void check_houdini_version()
{
	const char build_version[] =
		SYS_VERSION_MAJOR "." SYS_VERSION_MINOR "." SYS_VERSION_BUILD;
	std::string runtime_version;
	try
	{
		runtime_version = ::HOM().applicationVersionString();
	}
	catch(...)
	{
		/*
			According to header comments, this should not happen. But it does
			when running hython. If we ever want the warning for that case, we
			could possibly parse ${HFS}/toolkit/include/SYS/SYS_Version.h
		*/
		return;
	}

	/*
		Check the version only once. We try multiple times because of the above
		failure case. With hython, the call from newVopOperator() works. But we
		still want to try checking as early as possible when not in hython.
	*/
	static bool checked = false;
	if(checked)
	{
		return;
	}
	checked = true;

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
	check_houdini_version();
	ROP_3Delight::Register(io_table);
	creation_callbacks::init();
}

extern "C" SYS_VISIBILITY_EXPORT void
newVopOperator(OP_OperatorTable* io_table)
{
	check_houdini_version();
	VOP_3DelightMaterialBuilder::Register(io_table);
	VOP_AOVGroup::Register(io_table);
	shader_library::get_instance().Register(io_table);
}

extern "C" SYS_VISIBILITY_EXPORT void
newObjectOperator(OP_OperatorTable* io_table)
{
	check_houdini_version();
	OBJ_IncandescenceLight::Register(io_table);
}

extern "C" SYS_VISIBILITY_EXPORT void
newRenderHook(DM_RenderTable* io_table)
{
	check_houdini_version();
	io_table->registerSceneHook(
		&viewport_hook_builder::instance(),
		DM_HOOK_BEAUTY,
		DM_HOOK_AFTER_NATIVE);
}
