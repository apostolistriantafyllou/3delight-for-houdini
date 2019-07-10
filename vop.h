#pragma once

#include "exporter.h"

class OP_Parameters;

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

public:
	vop( const context&, VOP_Node *);

	void create( void ) const override;
	void set_attributes( void ) const override;
	void set_attributes_at_time( double i_time ) const override;
	void connect( void ) const override;

	/**
		\returns a legalized OSL name from vop->getOperator()->getName()
		as these names can contain illegal characters for a file system
		and/or OSL.

		principled::2.0 would be converted to principled__2_0.
	*/
	std::string osl_name( void ) const;
	static std::string osl_name( const char * );

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
		\param o_list
			The resulting NSI argument list that can be passed directly
			to NSISetAttribute[AtTime]
	*/
	static void list_shader_parameters(
		const OP_Parameters *i_parameters,
		const char *i_shader,
		float i_time,
		NSI::ArgumentList &o_list );

private:
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
};
