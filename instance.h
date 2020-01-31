#pragma once

#include "primitive.h"

#include <vector>
#include <string>

/**
	\brief exports Houdini's instance as an NSI instances node.
*/
class instance : public primitive
{
public:
	instance(
		const context&,
		OBJ_Node *,
		double,
		const GT_PrimitiveHandle &,
		unsigned i_primitive_index,
		const std::vector<std::string> & );

	void create( void ) const override;
	void connect( void ) const override;

protected:
	/// Exports time-dependent attributes to NSI
	void set_attributes_at_time(
		double i_time,
		const GT_PrimitiveHandle i_gt_primitive)const override;

	void set_attributes( void ) const override;

private:
	std::string merge_handle( void ) const;

	std::vector<std::string> m_source_models;
};
