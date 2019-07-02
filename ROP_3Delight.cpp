#include "ROP_3Delight.h"

#include <ROP/ROP_Error.h>
#include <ROP/ROP_Templates.h>
#include <OP/OP_OperatorTable.h>

#include <UT/UT_DSOVersion.h>


static PRM_Template*
GetTemplates()
{
	static PRM_Template t[] = 
	{
		theRopTemplates[ROP_TPRERENDER_TPLATE],
		theRopTemplates[ROP_PRERENDER_TPLATE],
		theRopTemplates[ROP_LPRERENDER_TPLATE],
		theRopTemplates[ROP_TPREFRAME_TPLATE],
		theRopTemplates[ROP_PREFRAME_TPLATE],
		theRopTemplates[ROP_LPREFRAME_TPLATE],
		theRopTemplates[ROP_TPOSTFRAME_TPLATE],
		theRopTemplates[ROP_POSTFRAME_TPLATE],
		theRopTemplates[ROP_LPOSTFRAME_TPLATE],
		theRopTemplates[ROP_TPOSTRENDER_TPLATE],
		theRopTemplates[ROP_POSTRENDER_TPLATE],
		theRopTemplates[ROP_LPOSTRENDER_TPLATE],
		PRM_Template()
	};

	return t;
}

static OP_TemplatePair*
GetTemplatePair()
{
	static OP_TemplatePair* ropPair = nullptr;
	if(!ropPair)
	{
		OP_TemplatePair* base;

		base = new OP_TemplatePair(GetTemplates());
		ropPair = new OP_TemplatePair(ROP_Node::getROPbaseTemplate(), base);
	}

	return ropPair;
}

static OP_VariablePair*
GetVariablePair()
{
	static OP_VariablePair* pair = nullptr;
	if(!pair)
	{
		pair = new OP_VariablePair(ROP_Node::myVariableList);
	}

	return pair;
}

void
newDriverOperator(OP_OperatorTable *table)
{
	table->addOperator(
		new OP_Operator(
			"3delight",
			"3Delight",
			ROP_3Delight::alloc,
			GetTemplatePair(),
			0,
			0,
			GetVariablePair(),
			0u,
			nullptr,
			0,
			"Render"));
}


OP_Node*
ROP_3Delight::alloc(OP_Network* net, const char* name, OP_Operator* op)
{
	return new ROP_3Delight(net, name, op);
}

ROP_3Delight::ROP_3Delight(
	OP_Network* net,
	const char* name,
	OP_Operator* entry)
	:	ROP_Node(net, name, entry)
{
}


ROP_3Delight::~ROP_3Delight()
{
}

int
ROP_3Delight::startRender(int, fpreal tstart, fpreal tend)
{
	end_time = tend;
	if(error() < UT_ERROR_ABORT)
	{
		executePreRenderScript(tstart);
	}

	return 1;
}

ROP_RENDER_CODE
ROP_3Delight::renderFrame(fpreal time, UT_Interrupt*)
{
	executePreFrameScript(time);

	if(error() < UT_ERROR_ABORT)
	{
		executePostFrameScript(time);
	}

	return ROP_CONTINUE_RENDER;
}

ROP_RENDER_CODE
ROP_3Delight::endRender()
{
	if(error() < UT_ERROR_ABORT)
	{
		executePostRenderScript(end_time);
	}

	return ROP_CONTINUE_RENDER;
}

