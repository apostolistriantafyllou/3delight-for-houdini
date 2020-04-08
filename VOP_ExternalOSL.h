#pragma once

#include <3Delight/ShaderQuery.h>

#include <UT/UT_HDKVersion.h>
#include <VOP/VOP_Node.h>
#include <VOP/VOP_Operator.h>

class DlShaderInfo;
struct VOP_ExternalOSLOperator;

/// Description of hardcoded parameters added to the "vdbVolume" shader
namespace VolumeGridParameters
{
	/*
		Names of the hardcoded grid parameters. They are deliberately the same
		names used on the NSI volume node.
	*/
	const char density_name[] = "densitygrid";
	const char color_name[] = "colorgrid";
	const char temperature_name[] = "temperaturegrid";
	const char emission_name[] = "emissionintensitygrid";
	const char velocity_name[] = "velocitygrid";
	/// Name of the velocity scale  hardcoded parameter
	const char velocity_scale_name[] = "velocityscale";

	/* Default values for the above parameters. */
	const char density_default[] = "density";
	const char color_default[] = "color";
	const char temperature_default[] = "temperature";
	const char emission_default[] = "heat";
	const char velocity_default[] = "vel";
	const float velocity_scale_default = 1.0f;
}


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

	/// Returns true if the shader can be used as a material
	bool IsTerminal()const { return m_terminal; }

	VOP_Type ShaderType() const { return m_shader_type; }

	/// The shader description
	const DlShaderInfo& m_dl;

private:

	// Indices of input parameters
	std::vector<unsigned> m_inputs;
	// Indices of output parameters
	std::vector<unsigned> m_outputs;
	// Indicates that the shader is meant to be used as a shading network's root
	bool m_terminal;
	// The type of shader.
	VOP_Type m_shader_type{VOP_SURFACE_SHADER};
};


/// A VOP node that takes its definition from a compiled OSL file
class VOP_ExternalOSL : public VOP_Node
{
public:

	/// Returns a VOP_Type for the given parameter.
	VOP_Type GetVOPType(const DlShaderInfo::Parameter& i_osl_param);

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

	/**
		Called when something changes in node. When loading a scene, it
		seems that an event OP_NAME_CHANGED is always sent. We use this
		instead of method onCreated, which fails because no parameters
		are created yet.
	*/
	virtual void opChanged(OP_EventType reason, void* data) override;
	/**
		Called when a new node is created. We use this instead of method
		onCreated, which fails because no parameters are created yet.
	*/
	virtual bool runCreateScript() override;

#if HDK_API_VERSION >= 18000000
	/// From VOP_Node
	virtual UT_StringHolder getShaderName(
		VOP_ShaderNameStyle style,
		VOP_Type shader_type) const override;

	virtual VOP_Type getShaderType() const override;
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

	/**
		\brief Initializes ramp parameters to their default value.

		It seems impossible to initialize the default values of ramp parameter,
		at the time of their template's creation, while still being able to
		specify their name (which we want to keep for backward compatibility
		with existing scenes). So, we set the default values manually, on each
		node that contains ramps.
	*/
	void SetRampParametersDefaults();

	void SetVDBVolumeDefaults();

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

