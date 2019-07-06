#pragma once

#include "exporter.h"

/**
	\brief Poly and poly soupe exporter.
*/
class null : public exporter
{
public:
	null( NSI::Context &, OBJ_Node *, const GT_PrimitiveHandle & );

	void create( void ) const override;
	void set_attributes( void ) const override;
	void set_attributes_at_time( double i_time ) const override;
	const OBJ_Node *parent( void ) const override;
};
