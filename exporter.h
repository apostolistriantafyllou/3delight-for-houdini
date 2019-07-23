#pragma once

#include "context.h"

/* WARNING: The order of the following two includes is important  { */
#include <GT/GT_Primitive.h>
#include <GT/GT_Handles.h>
/* }  */

#include <assert.h>
#include <nsi.hpp>

#include <string>
#include <vector>

class OBJ_Node;
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
	exporter(
		const context &,
		OBJ_Node *,
		const GT_PrimitiveHandle & = sm_invalid_gt_primitive);

	exporter( const context &, VOP_Node * );

	/**
		\brief Create any NSI node required for this object. For example,
		a camera might need a "perspectivecamera" NSI node as well as a
		"screen" node.
	*/
	virtual void create( void  ) const = 0;

	/**
		\brief Set all attributes for this primitive that do not change with
		time. For example, the number of faces of a polygon.
	*/
	virtual void set_attributes( void  ) const = 0;

	/**
		\brief Set all the time valuing attributes for this primitive. For
		example the "P" attribute for a mesh.
	*/
	virtual void set_attributes_at_time( double i_time ) const = 0;

	/**
		\brief Connect.
	*/
	virtual void connect( void ) const;

	/**
		\brief Returns the handle of this node.
	*/
	const std::string &handle( void ) const;

	/**
		\brief Declare this as an instnaced object. Such objects
		do not have to be connected to a parent transform because
		they will be using the 'instance' exporter.
	*/
	void set_as_instanced( void ) { m_instanced = true; }

protected:
	/**
		\brief Helper to export attributes lists to NSI.

		\param i_attributes
			An array of attrbute lists.
		\param i_n
			Total number of attrbute lists in the array.
		\param i_time
			The time to communicate to NSISetAttributeAtTime
		\param io_which_ones
			Which attribute names to output? Every time we find an attribute of
			the given name, we remove it from the list. This means that on
			return, the io_which_ones vector will contain the attributes names
			that were not found in i_attributes. This allows the exporter to
			take corrective measures (add some default attribute) or to issue
			error messages.

		\see pointmesh::set_attributes_at_time
		\see curvemesh::set_attributes_at_time
	*/
	void export_attributes(
		const GT_AttributeListHandle *i_attributes,
		int i_n,
		double i_time,
		std::vector<const char *> &i_which_ones ) const;

	/**
		\brief Exports connections to the proper sets to implement light linking.

		The required NSI sets are created along the way.

		\param i_categories
			The expression that describes the categories of lights that light
			this object.
		\param i_attributes_handle
			The handle of this object's "attributes" NSI node.
	*/
	void export_light_categories(
		const UT_String& i_categories,
		const std::string& i_attributes_handle)const;

protected:
	/** Depending on what we are exporting, an OBJ or a VOP node */
	union
	{
		VOP_Node *m_vop;
		OBJ_Node *m_object;
	};

	/**
		Will be invalid if GT primitives are not requried. Such as for the
		null node and VOP nodes.
	*/
	GT_PrimitiveHandle m_gt_primitive;

	/** = for the default parameter in the ctor */
	static GT_PrimitiveHandle sm_invalid_gt_primitive;

	/** The export context. */
	const context& m_context;

	/** The NSI export context, which is redundant with m_context.m_nsi. */
	NSI::Context &m_nsi;

	/** The NSI handle */
	std::string m_handle;

	/** =true if instanced geo */
	bool m_instanced{false};
};
