#pragma once

#include "primitive.h"

/**
	\brief exports Houdini's instance as an NSI instances node.
*/
class instance : public primitive
{
public:
	instance(
		const context&,
		OBJ_Node *,
		const GT_PrimitiveHandle &,
		const std::string &i_geometry_handle );

	void create( void ) const override;
	void set_attributes( void ) const override;
	void connect( void ) const override;

private:
	/// Exports time-dependent attributes to NSI
	void set_attributes_at_time( double i_time ) const;

	std::string m_geometry_handle;
};
