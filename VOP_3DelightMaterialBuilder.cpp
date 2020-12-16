#include "VOP_3DelightMaterialBuilder.h"
#include "VOP_ExternalOSL.h"

#include "vop.h"

const char* VOP_3DelightMaterialBuilder::theChildTableName = VOP_TABLE_NAME;

bool
VOP_3DelightOperatorFilter::allowOperatorAsChild(OP_Operator* i_op)
{
    return dynamic_cast<VOP_ExternalOSLOperator*>(i_op) != NULL ||
			i_op->getName().toStdString() == "makexform"||
			i_op->getName().toStdString() == "dlAOVGroup";
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

/*
	Automatically instance dlTerminal node inside dlMaterialBuilder
	when it is created. This is done using createNode() function
	which adds a new OP_Node of the specified type (dlTerminal)
	as a child of this node (dlMaterialBuilder).
*/
bool
VOP_3DelightMaterialBuilder::runCreateScript()
{
	const char* dlTerminalShader = "3Delight::dlTerminal";
	createNode(dlTerminalShader);
	return VOP_Node::runCreateScript();
}

/**
	First try to find a Terminal node. And get the surface shader from
	there.

	If none found, loop through all the nodes in the material builder and just
	take the first material we get.
*/
void VOP_3DelightMaterialBuilder::get_materials( VOP_Node *o_materials[3] )
{
	assert( o_materials );

	int nkids = getNchildren();

	for( int i=0; i<nkids; i++ )
	{
		VOP_Node *mat = CAST_VOPNODE(getChild(i) );

		if( !mat || !(mat->getOperator()->getName() == "3Delight::dlTerminal") )
		{
			continue;
		}

		assert( mat->getMaterialFlag() );

		for( int i=0; i< mat->nInputs(); i++ )
		{
			VOP_Node *source = CAST_VOPNODE( mat->getInput(i) );

			if( !source )
				continue;

			UT_String input_name;
			mat->getInputName(input_name, i);

			if( input_name == "Surface" )
			{
				o_materials[0] = source;
			}
			else if( input_name == "Displacement" )
			{
				o_materials[1] = source;
			}
			else if( input_name == "Volume" )
			{
				o_materials[2] = source;
			}
		}

		/*
			If there is a terminal node, we rely on it for the assignment so
			don't scan further.
		*/
		return;
	}

	for( int i=0; i<nkids; i++ )
	{
		VOP_Node *mat = CAST_VOPNODE( getChild(i) );

		if( !mat || !mat->getMaterialFlag() )
			continue;

		vop::osl_type type = vop::shader_type( mat );

		if( type == vop::osl_type::e_surface || type == vop::osl_type::e_other )
			o_materials[0]= mat;

		if( type == vop::osl_type::e_displacement )
			o_materials[1] = mat;

		if( type == vop::osl_type::e_volume )
			o_materials[2] = mat;
	}
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

	VOP_OperatorInfo* vop_info =
		static_cast<VOP_OperatorInfo*>(getOpSpecificData());

	/*
		The RenderMask is what ends up being the MaterialNetworkSelector in
		Hydra. If we don't set it, the default translator will not provide
		networks at all. And if it does not match the Hydra plugin, we won't
		see the networks there.
	*/
	vop_info->setRenderMask("nsi");
}
