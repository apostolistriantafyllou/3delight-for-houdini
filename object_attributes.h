#pragma once

#include <nsi.hpp>

class context;
class OBJ_Node;
class UT_String;

#include <string>

/**
	Utilities to export the 3Delight supported attributes attached to OBJ
	nodes.
*/
struct object_attributes
{
public:
	/**
		\brief Geometric Object attributes types

		These attributes are handled by a connection to global
		attribute nodes that define the non-default NSI value.
	*/
	enum geo_attribute
	{
		e_matte = 0,
		e_prelit,
		e_visCamera,
		e_visShadow,
		e_visDiffuse,
		e_visReflection,
		e_visRefraction,
		e_invalidAttribute
	};

public:

	/**
		\brief Export NSI nodes that contain geometric object attributes.

		Those nodes are meant to be connected to the objects that
		have the attributes enabled.
	*/
	static void export_object_attribute_nodes( const context& i_ctx );

	/**
		\brief Connect the specified object to relevant object attribute nodes.
	*/
	static void connect_to_object_attributes_nodes(
		const context& i_ctx,
		OBJ_Node* i_object,
		const std::string& i_handle	);

private:
	/**
		\brief The NSI handle of the global attribute node
	*/
	static const char* geo_attribute_node_handle( geo_attribute i_type );

	/**
		\brief Givem a geo_attribute, returns the name of the NSI attribute
	*/
	static const char* geo_attribute_nsi_name( geo_attribute i_type );

	/**
		\brief The name of the Houdini attribute
	*/
	static const char* geo_attribute_houdini_name( geo_attribute i_type );

	/**
		\brief The attribute's default NSI value
	*/
	static bool geo_attribute_nsi_default_value( geo_attribute i_type );

	/**
		\brief Returns true if the Houdini attribute has its default value
	*/
	static bool geo_attribute_has_houdini_default_value(
		geo_attribute i_type,
		OBJ_Node* i_object,
		double i_time );

	/**
		\brief Connect or disconnect the speficied object to the attribute node
	*/
	static void adjust_geo_attribute_connection(
		const context& i_ctx,
		OBJ_Node* i_object,
		const std::string& i_handle,
		geo_attribute i_type,
		bool i_first_time );
};
