#pragma once

#include "time_sampler.h"

/* WARNING: The order of the following two includes is important  { */
#include <GT/GT_Primitive.h>
#include <GT/GT_Handles.h>
#include <OBJ/OBJ_Node.h>
/* }  */

#include <nsi.h>

#include <string>
#include <vector>

class context;
class VOP_Node;
namespace NSI { class Context; }

/**
	\brief A base class for any object export in 3delight-for-houdini (OBJ +
	VOP).

	All exporters derive from this class.
*/
class exporter
{
public:
	exporter( const context &, OBJ_Node * );

	exporter( const context &, VOP_Node * );

	virtual ~exporter();

	/**
		\brief Create the NSI node that corresponds to this object's handle.

		It might also create other, auxiliary nodes here (for example, a camera
		might need a "perspectivecamera" NSI node as well as a "screen" node),
		but this could also wait later, in set_attributes() or connect().

		We simply need to ensure that other nodes could be connected to this one
		after create() has been called.
	*/
	virtual void create( void  ) const = 0;

	/**
		\brief Set all attributes for this primitive.
	*/
	virtual void set_attributes( void  ) const = 0;

	/**
		\brief Connect to the parent object of this exporter.
	*/
	virtual void connect( void ) const = 0;

	/**
		\brief Returns the handle of this node.
	*/
	const std::string &handle( void ) const;

	OP_Node *node( void ) { return m_object; }

	/**
		\brief Declare this as an instanced object. Such objects
		do not have to be connected to a parent transform because
		they will be using the 'instance' exporter.
	*/
	void set_as_instanced( void ) { m_instanced = true; }

	/// Returns the NSI handle used for the node i_node
	static std::string handle(const OP_Node& i_node);

	/**
		\brief Returns the name of transparent shader attribute.
	*/
	static const char* transparent_surface_handle();

	/**
		\brief Resolves the [relative] material path to a VOP node.
	*/
	static VOP_Node *resolve_material_path(
		OP_Node* i_relative_path_root,
		const char *i_path);

protected:

	/**
		\brief Helper to export vertex/point/primitive/detail attributes lists
		to NSI.

		\param io_which_ones
			Which attribute names to output? Every time we find an attribute of
			the given name, we remove it from the list. This means that on
			return, the io_which_ones vector will contain the attributes names
			that were not found in i_attributes. This allows the exporter to
			take corrective measures (add some default attribute) or to issue
			error messages.
		\param i_primitive
			The primitive for which attributes output is required.
		\param i_time
			The time to communicate to NSISetAttributeAtTime
		\param i_vertices_list
			A vertex list to use with point attributes.

		\see pointmesh::set_attributes_at_time
		\see curvemesh::set_attributes_at_time
	*/
	void export_attributes(
		std::vector<std::string> &i_which_ones,
		const GT_Primitive &i_primitive,
		double i_time,
		GT_DataArrayHandle i_vertices_list = GT_DataArrayHandle()) const;

	VOP_Node *resolve_material_path( const char *i_path ) const
	{
		return resolve_material_path(m_object, i_path);
	}

	/**
		\brief Returns the context where to export (possibly static) attributes.

		The accessor chooses whether to return m_context.m_nsi or
		m_context.m_static_nsi, based on the time-dependency of m_object. This
		allows attributes that don't change from one frame to another to be
		exported in a different file, shared among every frame's main NSI file.

		\param i_type
			The source of animation that must be checked when computing
			time-dependency.
		\return
			An NSI context to be used when exporting the node's attributes. It
			could have an invalid handle, in which case it's safe to skip the
			export of those attributes (because they have already have been
			exported in a previous frame).
	*/
	NSI::Context& attributes_context(
		time_sampler::blur_source i_type = time_sampler::e_deformation)const;

	/**
		Depending on what we are exporting, an OBJ or a VOP node.

		FIXME : some functions in the exporter class use m_object
		indiscriminately, without regard for the fact that it could actually
		point to a VOP_Node.  This works, for now, because of two reasons :

		- Both OBJ_Node and VOP_Node inherit directly from OP_Network, without
		  multiple inheritance, so the pointer always points to an OP_Network
		  object, which contains most of the API we use.
		- The functions using m_object are actually called only when m_object
		  points to an OBJ_Node.

		It would be better to replace the union with a single OP_Network* and
		call castToOBJNode() or castToVOPNode() in the few cases where it's
		needed.
	*/
	union
	{
		VOP_Node *m_vop;
		OBJ_Node *m_object;
	};

	/** The export context. */
	const context& m_context;

	/** The NSI export context, which is redundant with m_context.m_nsi. */
	NSI::Context &m_nsi;

	/** The NSI handle */
	std::string m_handle;

	/** =true if instanced geo */
	bool m_instanced{false};
};
