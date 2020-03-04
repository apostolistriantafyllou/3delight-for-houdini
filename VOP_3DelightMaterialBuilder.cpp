#include "VOP_3DelightMaterialBuilder.h"

#include "VOP_ExternalOSL.h"

const char* VOP_3DelightMaterialBuilder::theChildTableName = VOPNET_TABLE_NAME;

bool
VOP_3DelightOperatorFilter::allowOperatorAsChild(OP_Operator* i_op)
{
    return dynamic_cast<VOP_ExternalOSLOperator*>(i_op) != NULL ||
			i_op->getName().toStdString() == "bind";
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
	// Updates a shared table, may be dangerous
	initializeOperatorTable();
	/*
		Tell Houdini that this shader can be used as a material.  It allows
		it to appear in the "Choose Operator" window that is displayed when
		one clicks on the associated button near an geometry object's
		"Material" parameter field.
		We were told to call this in an override of
		VOP_Node::initMaterialFlag(), but it works just fine here. After
		all, here is the earliest time we can call it.
	*/
	setMaterialFlag(true);
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

void
VOP_3DelightMaterialBuilder::initializeOperatorTable()
{
    OP_OperatorTable& global_ot = *OP_Network::getOperatorTable(VOP_TABLE_NAME);
    OP_OperatorTable& ot = *getOperatorTable();

	OP_OperatorList global_ol;
	global_ot.getOperators(global_ol);

	// Updates "ot" from our filter apply from those into global_ot
	VOP_3DelightOperatorFilter ourFilter;
	for(int o = 0; o < global_ol.size(); o++)
	{
		OP_Operator* op = global_ot.getOperator(global_ol[o]->getName());
		assert(op);
		if (ourFilter.allowOperatorAsChild(op))
		{	
			ot.addOperator(op);
		}
	}
    // Notify observers of the operator table that it has been changed.
    ot.notifyUpdateTableSinksOfUpdate();
}

VOP_External3DelightMaterialBuilderOperator::VOP_External3DelightMaterialBuilderOperator()
	:	VOP_Operator(
			"3Delight::dlMaterialBuilder",
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
