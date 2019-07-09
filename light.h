#pragma once

#include "exporter.h"

/**
	\brief Poly and poly soupe exporter.
*/
class light : public exporter
{
	/*
		Those value follow the order in 'light_type' drop down menu.
	*/
	enum type
	{
		e_point = 0,
		e_line,
		e_grid,
		e_disk,
		e_sphere,
		e_tube,
		e_geometry,
		e_distant,
		e_sun
	};

public:
	light( const context&, OBJ_Node *);

	void create( void ) const override;
	void set_attributes( void ) const override;
	void set_attributes_at_time( double i_time ) const override;

private:
	/**
		\brief Creates the geometry for Houdini's "light_type".
	*/
	void create_default_geometry( void ) const;
};
