#pragma once

#include "exporter.h"

#include <OP/OP_Value.h>

class OP_Node;

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
	void connect( void ) const override;

	static void changed_cb(
		OP_Node* i_caller,
		void* i_callee,
		OP_EventType i_type,
		void* i_data);

private:

	/// Exports time-dependent attributes to NSI
	void set_attributes_at_time( double i_time ) const;

	/// Disconnects the light from the scene.
	void disconnect()const;

	/// Creates the light's geometry according to Houdini's "light_type".
	void create_geometry( void ) const;
	/// Deletes the light's geometry nodes
	void delete_geometry( void ) const;

	/**
		Sets the light shader attribute corresponding to parameter i_parm_index,
		if any. Returns true if successful.
	*/
	bool set_single_shader_attribute(int i_parm_index)const;

	/// Sets the visibility.camera attribute
	void set_visibility_to_camera()const;

	/// Returns the handle of the light's NSI attributes node
	std::string attributes_handle()const
	{
		return m_handle + "|attributes";
	}

	/// Returns the handle of the light's NSI geometry node
	std::string geometry_handle()const
	{
		return m_handle + "|geometry";
	}

	/// Returns the handle of the light's NSI shader node
	std::string shader_handle()const
	{
		return m_handle + "|shader";
	}

	/// Returns the name of the shader to use for this light
	const char* shader_name()const
	{
		return m_is_env_light ? "environmentlight" : "hlight";
	}

	/** = true if this is an environment light */
	bool m_is_env_light{false};
};
