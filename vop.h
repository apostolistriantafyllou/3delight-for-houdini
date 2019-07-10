#pragma once

#include "exporter.h"

class OP_Parameters;

/**
	\brief Exports a VOP (shader). This means attributes (parameters)
	and connections to other VOP nodes.
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

protected:
	/**
		Set all parameters of 'i_shader' by finding their values pairs
		in 'i_parameters'.

		\param i_parameters
			The node where to get parameter values.
		\param i_shader
			The OSL shader from where to get parameter names.
		\param o_list
			The resulting NSI argument list that can be passed directly
			to NSISetAttribute[AtTime]
	*/
	static void list_shader_parameters(
		const OP_Parameters *i_parameters,
		const char *i_shader,
		NSI::ArgumentList &o_list );
};
