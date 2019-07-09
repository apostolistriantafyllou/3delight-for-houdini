#pragma once

#include "exporter.h"

/**
	\brief Poly and poly soupe exporter.
*/
class null : public exporter
{
public:
	null( const context&, OBJ_Node * );

	void create( void ) const override;
	void set_attributes( void ) const override;
	void set_attributes_at_time( double i_time ) const override;
	void connect( void ) const override;
};
