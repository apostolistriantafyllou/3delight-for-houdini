#pragma once

#include "exporter.h"

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

private:

	/// List of refined primitives
	std::vector<primitive*> m_primitives;
};
