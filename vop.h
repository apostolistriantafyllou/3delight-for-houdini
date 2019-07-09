#pragma once

#include "exporter.h"

/**
	\brief Exports a VOP (shader). This means attributes (parameters)
	and connections to other VOP nodes.
*/
class vop : public exporter
{
public:
	vop( const context&, VOP_Node *);

	void create( void ) const override;
	void set_attributes( void ) const override;
	void set_attributes_at_time( double i_time ) const override;
	void connect( void ) const override;
};
