#pragma once

#include "primitive.h"

/**
	\brief Exporter for a GT_PrimPointMesh.
*/
class pointmesh : public primitive
{
public:
	pointmesh( const context&, OBJ_Node *, const GT_PrimitiveHandle & );

	void create( void ) const override;
	void set_attributes( void ) const override;

private:
	/// Exports time-dependent attributes to NSI
	void set_attributes_at_time( double i_time ) const;
};
