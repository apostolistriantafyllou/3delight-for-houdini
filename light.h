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

	/**
		Returns object path of the geometry. If this light is not a geo,
		this will return an empty string.
	*/
	std::string get_geometry_path( void ) const;

	/// Returns the NSI handle used for the light node i_light
	static std::string handle(const OP_Node& i_light, const context& i_ctx);

	/// Deletes the NSI nodes associated to Houdini node i_node.
	static void Delete(OBJ_Node& i_node, const context& i_context);

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

	/// Connects the selected filter to it's light parent.
	void connect_filter()const;

	///Sets attributes for light filters.
	void set_filter_attributes(double i_time)const;

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
	const char* shader_name()const;

	/** = true if this is an environment light */
	bool m_is_env_light{false};
	bool m_is_sky_map{false};
};
