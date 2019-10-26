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

};
