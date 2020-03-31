#include "OBJ_IncandescenceLight.h"
#include "ROP_3Delight.h"
#include "VOP_3DelightMaterialBuilder.h"
#include "creation_callbacks.h"
#include "shader_library.h"

#include <UT/UT_DSOVersion.h>


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

