#include "VOP_3DelightMaterialBuilder.h"

#include "VOP_ExternalOSL.h"

const char* VOP_3DelightMaterialBuilder::theChildTableName = VOP_TABLE_NAME;

bool
VOP_3DelightOperatorFilter::allowOperatorAsChild(OP_Operator* i_op)
{
    return dynamic_cast<VOP_ExternalOSLOperator*>(i_op) != NULL ||
			i_op->getName().toStdString() == "bind";
}

void
VOP_3DelightMaterialBuilder::Register(OP_OperatorTable* io_table)
{
	io_table->addOperator(new VOP_3DelightMaterialBuilderOperator());
}

OP_Node*
VOP_3DelightMaterialBuilder::alloc(OP_Network* net, const char* name, OP_Operator* entry)
{
	VOP_3DelightMaterialBuilderOperator* osl_entry =
		dynamic_cast<VOP_3DelightMaterialBuilderOperator*>(entry);
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
	OP_Network* parent, const char* name, VOP_3DelightMaterialBuilderOperator* entry)
    :	VOP_SubnetBase(parent, name, entry)
{
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

/**
	We loop through all the nodes in the material builder and just take
	the first material we get.
*/
VOP_Node* VOP_3DelightMaterialBuilder::get_material()
{
	int nkids = getNchildren();

	for( int i=0; i<nkids; i++ )
	{
		VOP_Node *mat = CAST_VOPNODE(getChild(i) );

		if( !mat || !mat->getMaterialFlag() )
			continue;

		return mat;
	}

	return nullptr;
}

VOP_3DelightMaterialBuilderOperator::VOP_3DelightMaterialBuilderOperator()
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
