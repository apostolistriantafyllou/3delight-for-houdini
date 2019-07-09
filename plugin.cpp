#include "ROP_3Delight.h"

#include <UT/UT_DSOVersion.h>


void
newDriverOperator(OP_OperatorTable* io_table)
{
	ROP_3Delight::Register(io_table);
}

