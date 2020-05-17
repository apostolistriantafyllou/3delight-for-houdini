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
struct object_visibility_resolver;

namespace NSI { class Context; }

#include <deque>
#include <vector>
#include <set>
#include <string>
#include <unordered_set>

/**
	\brief Converts the scene (geo, shaders, light, cameras) into its
	NSI representation.
*/
class scene
{
public:
	static void convert_to_nsi( const context & );

	static void insert_obj_node(
		OBJ_Node& i_node,
		const context& i_context );

	static void export_materials(
		std::unordered_set<std::string>& i_materials,
		const context& i_context );

	static void find_lights(
		const OP_BundlePattern* i_lights,
		const char* i_rop_path,
		bool i_want_incandescence_lights,
		std::vector<OBJ_Node*>& o_lights );

	/**
		\brief Find the bind export nodes that can produce custom AOVs
	*/
	static void find_custom_aovs(
		const object_visibility_resolver &,
		std::vector<VOP_Node*>& o_custom_aovs );

private:
	static void obj_scan(
		const context &i_context,
		std::vector< exporter * > &o_to_export );

	static void vop_scan(
		const context &i_context,
		std::vector< exporter * > &o_to_export );

	static void get_material_vops(
		const std::unordered_set<std::string>& i_materials,
		std::vector<VOP_Node*> &o_vops );

	static void create_materials_exporters(
		const std::unordered_set<std::string>& i_materials,
		const context &i_context,
		std::vector<exporter *> &io_to_export );
	static void create_atmosphere_shader_exporter(
		const context& i_context,
		std::vector<exporter *>& io_to_export );

	static void export_nsi(
		const context &i_context,
		const std::vector<exporter*>& i_to_export);

	static void scan_for_instanced(
		const context &i_context,
		std::vector<exporter *> &io_to_export );

	static void process_obj_node(
		const context &i_context,
		OBJ_Node *,
		bool i_re_export_instanced,
		std::vector<exporter *> &o_to_export );

	/**
		\brief Exports connections to the proper sets to implement light linking.

		The required NSI sets are created along the way.

		\param i_context
			Current rendering context.
		\param io_exported_lights_categories
			A cache for already exporter categories. Could be expended after
			this call if a new category has been found.
		\param i_lights_to_render
			The set of lights to render, to be lazily updated by the function
			when it's needed and still empty.
	*/
	static void export_light_categories(
		const context &i_context,
		exporter *,
		std::set<std::string> &io_exported_lights_categories,
		std::vector<OBJ_Node*> &io_lights_to_render );

};
