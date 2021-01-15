#pragma once

#include "exporter.h"

#include <OP/OP_Value.h>

class OBJ_Camera;
class OP_Node;

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

	/**
		\brief Callback that should be connected to an OP_Node that has an
		associated camera exporter.
	*/
	static void changed_cb(
		OP_Node* i_caller,
		void* i_callee,
		OP_EventType i_type,
		void* i_data);

	/// Returns the NSI handle used for the camera i_camera
	static std::string handle(OBJ_Node& i_camera, const context& i_ctx);

	/// Deletes the NSI nodes associated to Houdini node i_node.
	static void Delete(OBJ_Node& i_node, const context& i_context);

	/// Returns the time during which i_camera's shutter is open, around i_time
	static double get_shutter_duration(
		OBJ_Camera& i_camera,
		double i_time);

	/**
		\brief Computes the "screenwindow" attribute of an NSI screen node.

		\param o_screen_window
			Pointer to 4 doubles where the screen window is to be output.
		\param i_camera
			Houdini camera from which to compute the screen window.
		\param i_time
			Time at which to retrieve the relevant camera attributes.
		\returns
			True if the screen window was output, false otherwise.
	*/
	static void get_screen_window(
		double* o_screen_window,
		OBJ_Camera& i_camera,
		double i_time,
		bool i_use_houdini_projection = false);

	/*
	*/
	static std::string screen_handle( OBJ_Node *i_cam, const context & );

private:
	std::string screen_handle( void ) const;

	/// Exports time-dependent attributes to NSI
	void set_attributes_at_time( double i_time ) const;

	/// Returns the NSI handle use for the camera's distortion shader
	std::string distortion_shader_handle()const
	{
		return m_handle + "|distortion";
	}

	// NSI camera node type
	std::string m_type;
	// NSI fisheye projection mapping
	std::string m_mapping;
};
