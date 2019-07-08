#pragma once

#include "exporter.h"

/**
	\brief camera exporter.
*/
class camera : public exporter
{
public:
	camera( const context&, OBJ_Node *, const GT_PrimitiveHandle & );

	void create( void ) const override;
	void set_attributes( void ) const override;
	void set_attributes_at_time( double i_time ) const override;

	/// Returns the NSI handle used for the camera i_camera
	static std::string get_nsi_handle(OBJ_Node& i_camera);
};
