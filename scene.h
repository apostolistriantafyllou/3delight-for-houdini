#pragma once

class context;
class exporter;
class OBJ_Node;
class OP_Node;
class UT_String;

namespace NSI { class Context; }

#include <vector>
#include <set>

/**
	\brief Converts the scene (geo, shaders, light, cameras) into its
	NSI representation.
*/
class scene
{
public:
	static void convert_to_nsi( const context & );
	static void find_lights( std::vector<OBJ_Node*>& o_lights );

private:
	static void scan(
		const context &i_context,
		std::vector< exporter * > &o_to_export );

	static void process_node(
		const context &i_context,
		OP_Node *i_node,
		std::vector<exporter *> &o_to_export );

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
};
