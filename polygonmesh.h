#pragma once

#include "primitive.h"

/**
	\brief Poly and poly soup exporter.
*/
class polygonmesh : public primitive
{
public:
	polygonmesh( const context&, OBJ_Node *, const GT_PrimitiveHandle &, unsigned, bool subdiv );

	void create( void ) const override;
	void set_attributes( void ) const override;

private:
	/// Exports time-dependent attributes to NSI
	void set_attributes_at_time( double i_time ) const;
	bool m_is_subdiv{false};
};
