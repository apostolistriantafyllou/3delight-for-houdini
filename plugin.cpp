#include "ROP_3Delight.h"
#include "VOP_3DelightMaterialBuilder.h"
#include "attributes_callbacks.h"
#include "shader_library.h"

#include <UT/UT_DSOVersion.h>


extern "C" SYS_VISIBILITY_EXPORT void
newDriverOperator(OP_OperatorTable* io_table)
{
	ROP_3Delight::Register(io_table);
	attributes_callbacks::init();
}

extern "C" SYS_VISIBILITY_EXPORT void
newVopOperator(OP_OperatorTable* io_table)
{
	VOP_3DelightMaterialBuilder::Register(io_table);
	shader_library::get_instance().Register(io_table);
}

