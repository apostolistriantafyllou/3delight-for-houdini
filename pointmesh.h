#pragma once

#include "primitive.h"

/**
	\brief Exporter for a GT_PrimPointMesh.
*/
class pointmesh : public primitive
{
public:
	pointmesh( const context&, OBJ_Node *, const GT_PrimitiveHandle &, unsigned );

	void create( void ) const override;

protected:
	/// Exports time-dependent attributes to NSI
	void set_attributes_at_time(
		double i_time,
		const GT_PrimitiveHandle i_gt_primitive)const override;
};
