#pragma once

#include <VOP/VOP_SubnetBase.h>
#include <VOP/VOP_Operator.h>

struct VOP_3DelightMaterialBuilderOperator;

class VOP_3DelightOperatorFilter : public OP_OperatorFilter
{
public:
    virtual bool allowOperatorAsChild(OP_Operator* i_op);
};

/// Our material builder to filter our VOP nodes.
class VOP_3DelightMaterialBuilder : public VOP_SubnetBase
{
public:

	/// Registers the VOP_3DelightMaterialBuilder
	static void Register(OP_OperatorTable* io_table);
	/**
		Adds an instance of VOP_3DelightMaterialBuilder to a network.
	*/
	static OP_Node* alloc(OP_Network* net, const char* name, OP_Operator* entry);

	/// Returns the templates for the shader's input parameters
    static PRM_Template myTemplateList[];
    static const char* theChildTableName;

    /// Override this to provide custom behaviour for what is allowed in the
    /// tab menu.
    virtual OP_OperatorFilter* getOperatorFilter() override;

	/// Minimum inputs that must be connected to a node for it to cook.
	virtual unsigned minInputs() const override;

	/// Returns the number input ports that should be visible
	virtual unsigned getNumVisibleInputs()const override;

	//Called when a new dlMaterialBuilder node is created.
	virtual bool runCreateScript() override;

	VOP_Node* get_material();

protected:

	/**
		\brief Constructor.

		Our 3Delight Material Builder.
	*/
	VOP_3DelightMaterialBuilder(
		OP_Network* net,
		const char* name,
		VOP_3DelightMaterialBuilderOperator* entry);
	virtual ~VOP_3DelightMaterialBuilder();

private:

    VOP_3DelightOperatorFilter myOperatorFilter;
};


struct VOP_3DelightMaterialBuilderOperator : public VOP_Operator
{
	/// Constructor.
	VOP_3DelightMaterialBuilderOperator();
};

