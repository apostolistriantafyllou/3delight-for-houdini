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
	static void convert_to_nsi( const context & );

	static void find_lights_and_mattes(
		const OP_BundlePattern* i_lights,
		const OP_BundlePattern* i_mattes,
		const char* i_rop_path,
		bool i_want_incandescence_lights,
		std::vector<OBJ_Node*>& o_lights,
		std::vector<OBJ_Node*>& o_mattes );

	/**
		\brief Find the bind export nodes that can produce custom AOVs
	*/
	static void find_custom_aovs( std::vector<VOP_Node*>& o_custom_aovs );

private:
	static void obj_scan(
		const context &i_context,
		std::vector< exporter * > &o_to_export );

	static void vop_scan(
		const context &i_context,
		std::vector< exporter * > &o_to_export );

	static void scan_for_instanced(
		const context &i_context,
		std::vector<exporter *> &io_to_export );

	static void process_obj_node(
		const context &i_context,
		OBJ_Node *,
		bool i_check_visibility,
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
