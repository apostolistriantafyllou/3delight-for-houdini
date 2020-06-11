#pragma once

#include <OBJ/OBJ_Geometry.h>
#include <OP/OP_Operator.h>

struct OP_IncandescenceLightOperator;

/// Our incandescence light (a fake light for multi-light)
class OBJ_IncandescenceLight: public OBJ_Geometry
{
public:

	/// Registers the OBJ_IncandescenceLight
	static void Register(OP_OperatorTable* io_table);
	/**
		Adds an instance of OBJ_IncandescenceLight to a network.
	*/
	static OP_Node* alloc(OP_Network* net, const char* name, OP_Operator* entry);

	/// Returns the templates for this light's input parameters
	static PRM_Template templateList[];

	static OP_TemplatePair* buildTemplatePair(OP_TemplatePair *prevstuff);

	/// Minimum inputs that must be connected to a node for it to cook.
	virtual unsigned minInputs() const override;

	/// Returns the number input ports that should be visible
	virtual unsigned getNumVisibleInputs()const override;
protected:

	/**
		\brief Constructor.

		Our Incandescence Light.
	*/
	OBJ_IncandescenceLight(
		OP_Network* net,
		const char* name,
		OP_IncandescenceLightOperator* entry);
	virtual ~OBJ_IncandescenceLight();
};


struct OP_IncandescenceLightOperator : public OP_Operator
{
	/// Constructor.
	OP_IncandescenceLightOperator();
	virtual bool getOpHelpURL(UT_String& url);
};
