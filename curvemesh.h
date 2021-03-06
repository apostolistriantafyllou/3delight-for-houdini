#pragma once

#include "primitive.h"

class curvemesh : public primitive
{
public:
	curvemesh(
		const context&, OBJ_Node*, double, const GT_PrimitiveHandle&, unsigned);

	void create( void ) const override;
	void set_attributes( void ) const override;

protected:
	/// Exports time-dependent attributes to NSI
	void set_attributes_at_time(
		double i_time,
		const GT_PrimitiveHandle i_gt_primitive)const override;

private:
	/**
		\brief Exports "width" and possibly also "P" and "id" attributes.

		Not the cleanest design, but it allows re-using the logic for attributes
		export, as well as for the "width" attribute's default value.
	*/
	void export_basic_attributes(
		double i_time,
		const GT_PrimitiveHandle i_gt_primitive,
		bool i_width_only)const;
};
