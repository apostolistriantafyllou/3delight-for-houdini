#pragma once

#include "exporter.h"

/**
	\brief camera exporter.
*/
class camera : public exporter
{
public:
	camera( NSI::Context &, OBJ_Node *, const GT_PrimitiveHandle & );

	void create( void ) const override;
	void set_attributes( void ) const override;
	void set_attributes_at_time( double i_time ) const override;
};