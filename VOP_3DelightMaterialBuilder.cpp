#include "VOP_3DelightMaterialBuilder.h"

#include "VOP_ExternalOSL.h"

const char* VOP_3DelightMaterialBuilder::theChildTableName = VOPNET_TABLE_NAME;

bool
VOP_3DelightOperatorFilter::allowOperatorAsChild(OP_Operator* i_op)
{
    return dynamic_cast<VOP_ExternalOSLOperator*>(i_op) != NULL;
}

void
VOP_3DelightMaterialBuilder::Register(OP_OperatorTable* io_table)
{
	io_table->addOperator(new VOP_External3DelightMaterialBuilderOperator());
}

OP_Node*
VOP_3DelightMaterialBuilder::alloc(OP_Network* net, const char* name, OP_Operator* entry)
{
	VOP_External3DelightMaterialBuilderOperator* osl_entry =
		dynamic_cast<VOP_External3DelightMaterialBuilderOperator*>(entry);
	assert(osl_entry);
    return new VOP_3DelightMaterialBuilder(net, name, osl_entry);
}

PRM_Template
VOP_3DelightMaterialBuilder::myTemplateList[] = 
{
    PRM_Template()      // List terminator
};

OP_OperatorFilter*
VOP_3DelightMaterialBuilder::getOperatorFilter()
{
	return &myOperatorFilter;
}

VOP_3DelightMaterialBuilder::VOP_3DelightMaterialBuilder(
	OP_Network* parent, const char* name, VOP_External3DelightMaterialBuilderOperator* entry)
    :	VOP_SubnetBase(parent, name, entry)
{
}

VOP_3DelightMaterialBuilder::~VOP_3DelightMaterialBuilder()
{
}

unsigned
VOP_3DelightMaterialBuilder::minInputs() const
{
	return 0;
}

unsigned
VOP_3DelightMaterialBuilder::getNumVisibleInputs() const
{
	return 0;
}

VOP_External3DelightMaterialBuilderOperator::VOP_External3DelightMaterialBuilderOperator()
	:	VOP_Operator(
			"3DelightMaterialBuilder",
			"3Delight Material Builder",
			VOP_3DelightMaterialBuilder::alloc,
			VOP_3DelightMaterialBuilder::myTemplateList,
			VOP_3DelightMaterialBuilder::theChildTableName,
			0, 0,
			"nsi")
{
	setIconName("ROP_3Delight");
	setOpTabSubMenuPath("3Delight");
}