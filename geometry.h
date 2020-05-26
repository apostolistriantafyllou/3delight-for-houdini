#pragma once

#include "exporter.h"

#include <OP/OP_Value.h>

#include <vector>
#include <string>
#include <unordered_set>

class primitive;
class instance;

/**
	\brief An exporter for an OBJGeometry Houdini node.

	It manages a list of primitives obtained through recursive refinement, using
	the GT library, of the object's main GT primitive. It simply forwards calls
	to the exporter's interface to each primitive in its list.
*/
class geometry : public exporter
{
public:
	/**
		\brief Constructor.

		\param i_context
			Current rendering context.
		\param i_object
			The object from which the GT primitives should be retrieved.
	*/
	geometry(const context& i_context, OBJ_Node* i_object);
	~geometry();

	void create()const override;
	void set_attributes()const override;
	void connect()const override;

	/** \ref scene::scan_for_instanced */
	void get_instances( std::vector<const instance *> & ) const;

	/** \brief Return all the materials needed by this geometry. */
	void get_all_material_paths(
		std::unordered_set< std::string > &o_materials ) const;

	static void changed_cb(
		OP_Node* i_caller,
		void* i_callee,
		OP_EventType i_type,
		void* i_data);
	static void sop_changed_cb(
		OP_Node* i_caller,
		void* i_callee,
		OP_EventType i_type,
		void* i_data);

	/// Returns the handle of the hub transform all primitives should connect to
	static std::string hub_handle(const OBJ_Node& i_node)
	{
		return exporter::handle(i_node) + "|hub";
	}

private:

	/**
		\brief Returns the material assigned to this geometry, either as an OBJ
		property or as a detail attribute.

		\param o_path
			Will contain the path of the assigned shader on successful
			run

		If successful, returns the pointer to the VOP node. Note that primitive
		level assignments are done in each sepcific exporter.
	*/
	VOP_Node *get_assigned_material( std::string &o_path ) const;

	/**
		\brief When this geometry is used as an NSI space override.
	*/
	void export_override_attributes( void ) const;

	/**
		\brief Re-exports the node to an NSI scene (during an IPR render).
		
		\param i_ctx
			The IPR rendering context.
		\param i_node
			The node to export.
		\param i_new_material
			Indicates whether material assignment has changed, which requires
			re-exporting their NSI shader networks to ensure they exist.
	*/
	static void re_export(
		const context& i_ctx,
		OBJ_Node& i_node,
		bool i_new_material = false);
	
	/// Returns the handle of the object's main NSI transform node
	std::string hub_handle()const
	{
		return m_handle + "|hub";
	}

	/// Returns the handle of the object's NSI attributes node
	std::string attributes_handle()const
	{
		return m_handle + "|attributes";
	}

	/// Returns the handle of the object's overrides NSI attributes node
	std::string overrides_handle()const
	{
		return m_handle + "|overrides";
	}

private:

	/// List of refined primitives
	std::vector<primitive*> m_primitives;
};
