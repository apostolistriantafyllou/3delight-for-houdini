#pragma once

#include "primitive.h"

class curvemesh : public primitive
{
public:
	curvemesh( const context&, OBJ_Node *, const GT_PrimitiveHandle &, unsigned);

	void create( void ) const override;
	void set_attributes( void ) const override;

private:
	/// Exports time-dependent attributes to NSI
	void set_attributes_at_time( double i_time ) const;
};
