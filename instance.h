#pragma once

#include "exporter.h"
#include <string>

/**
	\brief exports Houdini's instance as an NSI instances node.
*/
class instance : public exporter
{
public:
	instance(
		const context&,
		OBJ_Node *,
		const GT_PrimitiveHandle &,
		const std::string &i_geometry_handle );

	void create( void ) const override;
	void set_attributes( void ) const override;
	void set_attributes_at_time( double i_time ) const override;
	void connect( void ) const override;

private:
	std::string m_geomtry_handle;
};
