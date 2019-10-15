#pragma once

#include <3Delight/ShaderQuery.h>

#include <UT/UT_HDKVersion.h>
#include <VOP/VOP_Node.h>
#include <VOP/VOP_Operator.h>

class DlShaderInfo;
struct VOP_ExternalOSLOperator;


/// A partition of a DlShaderInfo object into input and output parameters
class StructuredShaderInfo
{
public:

	/// Constructor
	explicit StructuredShaderInfo(const DlShaderInfo* i_info);

	/// Returns the description of an input parameter
	const DlShaderInfo::Parameter& GetInput(unsigned i_index)const
	{
		assert(i_index < m_inputs.size());
		return *m_dl.getparam(m_inputs[i_index]);
	}

	/// Returns the description of an output parameter
	const DlShaderInfo::Parameter& GetOutput(unsigned i_index)const
	{
		assert(i_index < m_outputs.size());
		return *m_dl.getparam(m_outputs[i_index]);
	}

	/// Returns the number of input parameters
	unsigned NumInputs()const { return m_inputs.size(); }
	/// Returns the number of output parameters
	unsigned NumOutputs()const { return m_outputs.size(); }

	/// Should probably returns true if the shader can be used as a material
	bool IsTerminal()const { return false; }

	/// The shader description
	const DlShaderInfo& m_dl;

private:

	// Indices of input parameters
	std::vector<unsigned> m_inputs;
	// Indices of output parameters
	std::vector<unsigned> m_outputs;
};


/// A VOP node that takes its definition from a compiled OSL file
class VOP_ExternalOSL : public VOP_Node
{
public:

	/**
		\brief Adds an instance of VOP_ExternalOSL to a network.

		The actual operator type that will be created is the one registered
		using the OP_Operator entry (actually a VOP_ExternalOSLOperator), which
		can be one of many, each containing information about a different
		shader.
	*/
	static OP_Node* alloc(OP_Network* net, const char* name, OP_Operator* entry);

	/// Returns the templates for the shader's input parameters
	static PRM_Template* GetTemplates(const StructuredShaderInfo& i_shader_info);

	/// Returns the label for input port at index i_idx
	virtual const char* inputLabel(unsigned i_idx)const override;
	/// Returns the label for output port at index i_idx
	virtual const char* outputLabel(unsigned i_idx)const override;

	/// Minimum inputs that must be connected to a node for it to cook.
	virtual unsigned minInputs() const override;

	/// Returns the number input ports that should be visible
	virtual unsigned getNumVisibleInputs()const override;

	/// Returns the number of ordered (ie : non-indexed) inputs
	virtual unsigned orderedInputs()const override;

#if HDK_API_VERSION >= 18000000
	/// From VOP_Node
	virtual UT_StringHolder getShaderName(
		VOP_ShaderNameStyle style,
		VOP_Type shader_type) const override;
#endif

protected:

	/**
		\brief Constructor.

		Expects a VOP_ExternalOSLOperator in order to retrieve shader
		information from it.
	*/
	VOP_ExternalOSL(
		OP_Network* net,
		const char* name,
		VOP_ExternalOSLOperator* entry);
	virtual ~VOP_ExternalOSL();

	/// Returns the internal name of an input parameter.
	virtual void getInputNameSubclass(UT_String &in, int i_idx)const override;
	/// Returns the index of the named input
	virtual int getInputFromNameSubclass(const UT_String &in)const override;
	/// Fills the info about the source of an input parameter
	virtual void getInputTypeInfoSubclass(
		VOP_TypeInfo &o_type_info, 
		int i_idx) override;
	/**
		Fills the info about the acceptable types for the source of an input
		parameter.
	*/
	virtual void getAllowedInputTypeInfosSubclass(
		unsigned i_idx,
		VOP_VopTypeInfoArray &o_type_infos) override;

	/// Returns the internal name of an output parameter.
	virtual void getOutputNameSubclass(UT_String &out, int i_idx)const override;
	/// Fills the info about an output parameter
	virtual void getOutputTypeInfoSubclass(
		VOP_TypeInfo &o_type_info, 
		int i_idx) override;

private:

	// Description of the shader this node stands for
	StructuredShaderInfo m_shader_info;
};


/**
	A subclass of VOP_Operator with additional data aimed at newly created
	VOP_ExternalOSL nodes.

	This is used to register many different operator types, each based on a
	different OSL shader.
*/
struct VOP_ExternalOSLOperator : public VOP_Operator
{
	/// Constructor.
	VOP_ExternalOSLOperator(
		const StructuredShaderInfo& i_shader_info,
		const std::string& i_menu_name);

	/// The shader information we want to pass to the VOP_ExternalOSL node
	StructuredShaderInfo m_shader_info;
};

