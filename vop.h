#pragma once

#include "exporter.h"

#include "3Delight/ShaderQuery.h"
#include "osl_utilities.h"

#include <OP/OP_Value.h>

class OP_Node;
class OP_Parameters;

namespace NSI { class ArgumentList; }

/**
	Creates the NSI representation of a VOP node by creating a shader node
	with VOP's type id as the shader file name and connecting it to it's
	input VOP nodes. So implementing any VEX node is just a matter of providing
	the OSL implementation in the osl/ directory. Of course, some VEX nodes are
	not implementable in OSL.
*/
class vop : public exporter
{
	/**
		The light source acts as a virtual VOP for the purpose of
		light shader export.
	*/
	friend class light;

	/**
		The scene needs to find out if a certain vop is an AOV
		defifinition.

		\ref scene::find_custom_aovs
	*/
	friend class scene;

public:
	vop( const context&, VOP_Node *);

	void create( void ) const override;
	void set_attributes( void ) const override;
	void connect( void ) const override;

	/**
		\returns m_vop->getOperator()->getName()
	*/
	std::string vop_name( void ) const;

	static void changed_cb(
		OP_Node* i_caller,
		void* i_callee,
		OP_EventType i_type,
		void* i_data);

protected:
	/**
		Set all parameters of 'i_shader' by finding their values pairs
		in 'i_parameters'.

		\param i_parameters
			The node where to get parameter values.
		\param i_shader
			The OSL shader from where to get parameter names.
		\param i_time
			The time to use for the parameter's eval.
		\param i_parm_index
			If non-negative, index of the only node parameter to be exported.
		\param o_list
			The resulting NSI argument list that can be passed directly
			to NSISetAttribute[AtTime]
	*/
	static void list_shader_parameters(
		const OP_Parameters *i_parameters,
		const char *i_shader,
		float i_time,
		int i_parm_index,
		NSI::ArgumentList &o_list );

private:
	/// Exports time-dependent attributes to NSI
	void set_attributes_at_time( double i_time ) const;

	/**
		\returns true if we should not connect this VOP to its subnetworks.
		(probably because the OSL version has all the functionality inside).
	*/
	bool ignore_subnetworks( void ) const;

	/**
		\returns true if unsupported shader. Meaning that we don't have an
		OSL counterpart for this shader.
	*/
	bool unsupported( void ) const;

	/**
		\brief If vop is our material and have some bind node connected, add
		and connect our aov group
	*/
	void add_and_connect_aov_group() const;

	/// Sets a single attribute on the associated NSI node
	bool set_single_attribute(int i_parm_index)const;

	/**
		\returns true if i_param_name indicates used of a texture path.
	*/
	static bool is_texture_path( const char* i_param_name );

	/**
		\brief Fills a list of NSI arguments from a shader for a ramp-type
		parameter.
	*/
	static void list_ramp_parameters(
		const OP_Parameters* i_opp,
		const DlShaderInfo::Parameter& i_param,
		osl_utilities::ramp::eType i_type,
		float i_time,
		NSI::ArgumentList& o_list );

	static bool is_aov_definition( VOP_Node *i_vop );
};
