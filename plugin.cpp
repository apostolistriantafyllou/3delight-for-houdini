#include "ROP_3Delight.h"
#include "shader_library.h"

#include <UT/UT_DSOVersion.h>


extern "C" void
newDriverOperator(OP_OperatorTable* io_table)
{
	ROP_3Delight::Register(io_table);
}

extern "C" void
newVopOperator(OP_OperatorTable* io_table)
{
	shader_library::get_instance().Register(io_table);
}

