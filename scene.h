#pragma once

class context;
class exporter;
class safe_interest;
class OBJ_Node;
class OP_Node;
class SOP_Node;
class UT_String;
class VOP_Node;
class OP_BundlePattern;

namespace NSI { class Context; }

#include <deque>
#include <vector>
#include <set>
#include <string>

/**
	\brief Converts the scene (geo, shaders, light, cameras) into its
	NSI representation.
*/
class scene
{
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

	static void convert_to_nsi(
		const context &,
		std::deque<safe_interest>& io_interests );
	static void find_lights(
		const OP_BundlePattern &i_pattern,
		const char* i_rop_path,
		std::vector<OBJ_Node*>& o_lights );

	/**
		\brief Find the bind export nodes that can produce custom AOVs
	*/
	static void find_custom_aovs( std::vector<VOP_Node*>& o_custom_aovs );

	/**
		\brief Returns the path of a VDB file if this node is a "VDB loader".

		\param i_cobj
			The obj node to analyse.

		\param i_time
			The time to use for the parameter eval operation.

		\returns path to VDB file if the file SOP indeed loads a VDB file;
	*/
	static std::string node_is_vdb_loader(
		OBJ_Node *i_obj, double i_time );

private:
	static void scan(
		const context &i_context,
		std::vector< exporter * > &o_to_export,
		std::deque<safe_interest>& io_interests );

	static void process_node(
		const context &i_context,
		OP_Node *i_node,
		std::vector<exporter *> &o_to_export,
		std::deque<safe_interest>& io_interests );

	/**
		\brief Exports connections to the proper sets to implement light linking.

		The required NSI sets are created along the way.

		\param i_context
			The NSI contest to export to.
		\param io_exported_lights_categories
			A cache for already exporter categories. Could be expended after
			this call if a new category has been found.
		\param i_lights_to_render
			The set of lights to render.
	*/
	static void export_light_categories(
		NSI::Context &i_context,
		exporter *,
		std::set<std::string> &io_exported_lights_categories,
		const std::vector<OBJ_Node*> &i_lights_to_render );

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

	/**
		\brief The NSI handle of the global attribute node
	*/
	static const char* geo_attribute_node_handle( geo_attribute i_type );
	/**
		\brief The name of the NSI attribute
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
