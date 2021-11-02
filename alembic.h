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

	/**
		\brief Returns all the materials needed by this geometry.

		This needs to be overriden here because, when materials are assigned at
		SOP-level on shapes from an Alembic archive, they are not found by
		the base class's version of the function.
	*/
	void get_all_material_paths(
		std::unordered_set<std::string>& o_materials)const override;

private:

	struct material
	{
		VOP_Node* m_vops[3] { nullptr, nullptr, nullptr };
	};

	// Retrieves all material references hidden inside the Alembic primitive
	void get_shape_materials(std::vector<material>& o_materials)const;

	// Returns the time at which the archive should be evaluated
	double get_abc_time()const;

	// Filled by set_attributes_at_time, cleared by set_attributes afterwards
	mutable std::vector<UT_Matrix4D> m_transforms;
	mutable std::vector<double> m_transform_times;
};
