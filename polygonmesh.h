#pragma once

#include "primitive.h"

/**
	\brief Poly and poly soup exporter.
*/
class polygonmesh : public primitive
{
public:
	polygonmesh(
		const context&,
		OBJ_Node *,
		double,
		const GT_PrimitiveHandle &,
		unsigned,
		bool subdiv );

	void create( void ) const override;
	void set_attributes( void ) const override;
	void connect( void ) const override;

protected:
	/// Exports time-dependent attributes to NSI
	void set_attributes_at_time(
		double i_time,
		const GT_PrimitiveHandle i_gt_primitive)const override;

	void assign_primitive_materials( void ) const;

private:
	bool m_is_subdiv{false};
};
