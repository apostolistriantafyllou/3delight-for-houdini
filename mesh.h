#pragma once

#include "exporter.h"

/**
	\brief Poly and poly soupe exporter.
*/
class mesh : public exporter
{
public:
	mesh( NSI::Context &, OBJ_Node *, const GT_PrimitiveHandle & );

	void create( void ) const override;
	void set_attributes( void ) const override;
	void set_attributes_at_time( double i_time ) const override;

	void set_as_subdiv( void )
	{
		m_is_subdiv = true;
	}

private:
	bool m_is_subdiv{false};
};
