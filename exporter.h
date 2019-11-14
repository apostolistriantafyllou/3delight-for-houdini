#pragma once

/* WARNING: The order of the following two includes is important  { */
#include <GT/GT_Primitive.h>
#include <GT/GT_Handles.h>
/* }  */

#include <string>
#include <vector>

class context;
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
	exporter( const context &, OBJ_Node * );

	exporter( const context &, VOP_Node * );

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

	OBJ_Node *obj( void ) { return m_object; }

	/**
		\brief Returns the name of transparent shader attribute.
	*/
	static const char* TransparentSurfaceHandle();

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
		\param i_which_flags
			If not empty, points to an array of length io_which_ones.size()
			containing the NSI attribute flags for each attribute in
			i_which_ones.

		\see pointmesh::set_attributes_at_time
		\see curvemesh::set_attributes_at_time
	*/
	void export_attributes(
		const GT_AttributeListHandle *i_attributes,
		int i_n,
		double i_time,
		std::vector<const char *> &i_which_ones,
		const int* i_which_flags = nullptr) const;

	void export_override_attributes() const;

protected:
	/** Depending on what we are exporting, an OBJ or a VOP node */
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
};
