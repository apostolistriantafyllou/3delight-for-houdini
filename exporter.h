#pragma once

/* WARNING: The order of the following two includes is important  { */
#include <GT/GT_Primitive.h>
#include <GT/GT_Handles.h>
/* }  */

#include <nsi.h>

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
	static const char* transparent_surface_handle();

protected:

	/**
		\brief Retrieves an Houdini attribute by name from a GT primitive.

		\param i_primitive
			The GT primitive where the attribute is to be searched for.
		\param i_name
			The name of the requested attribute.
		\param o_data
			Will be set to a data handle which will allow access to the
			attribute's data.
		\param o_nsi_type
			Will be set to the NSI equivalent to the attribute's type.
		\param o_nsi_flags
			Will be set to NSI flags required when exporting the attribute.
		\param o_point_attribute
			Will be set to true if the attribute has been found in the
			primitive's set of "point" attributes.
		\return
			True if the attribute was found, false otherwise.
	*/
	bool find_attribute(
		const GT_Primitive& i_primitive,
		const std::string& i_name,
		GT_DataArrayHandle& o_data,
		NSIType_t& o_nsi_type,
		int& o_nsi_flags,
		bool& o_point_attribute)const;

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

	/**
		\brief Export all the attributes that the user wishes to "bind".

		We use the word bind here quiet liberaely as we don't really support
		bind :) We use dlAttributeRead.

		\param i_primitive
			The primitive for which to output the bind attributes.
		\param i_vertices_list
			A vertex list to use with point attributes.
	*/
	void export_bind_attributes(
		const GT_Primitive &i_primitive,
		GT_DataArrayHandle i_vertices_list = GT_DataArrayHandle()) const;

	void export_override_attributes() const;


private:
	VOP_Node *get_assigned_material( std::string &o_path ) const;

	/**
		\brief Returns attributes that are referenced by a "bind" or
		a PrimitiveAttribute VOP node.

		These attributes will be exported alongside with the geometry.
	*/
	void get_bind_attributes(
		std::vector< std::string > &o_to_export ) const;

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
