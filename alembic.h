#pragma once

#include "primitive.h"

class alembic : public primitive
{
public:
	alembic(
		const context&, OBJ_Node*, double, const GT_PrimitiveHandle&, unsigned);

	void create( void ) const override;
	void set_attributes( void ) const override;

protected:
	/// Exports time-dependent attributes to NSI
	void set_attributes_at_time(
		double i_time,
		const GT_PrimitiveHandle i_gt_primitive)const override;

private:

	// Filled by set_attributes_at_time, cleared by set_attributes afterwards
	mutable std::vector<UT_Matrix4D> m_transforms;
	mutable std::vector<double> m_transform_times;
};
