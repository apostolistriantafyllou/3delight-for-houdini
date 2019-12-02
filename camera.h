#pragma once

#include "exporter.h"

class OBJ_Camera;

/**
	\brief camera exporter.
*/
class camera : public exporter
{
public:
	camera( const context&, OBJ_Node *);

	void create( void ) const override;
	void set_attributes( void ) const override;
	void connect( void ) const override;

	/// Returns the NSI handle used for the camera i_camera
	static std::string get_nsi_handle(OBJ_Node& i_camera);

	static double get_shutter_duration(
		OBJ_Camera& i_camera,
		double i_time);

private:
	/// Exports time-dependent attributes to NSI
	void set_attributes_at_time( double i_time ) const;
};
