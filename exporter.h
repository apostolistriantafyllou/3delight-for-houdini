#pragma once

#include "context.h"

/* WARNING: The order of the following two includes is important  { */
#include <GT/GT_Primitive.h>
#include <GT/GT_Handles.h>
/* }  */

#include <assert.h>
#include <nsi.hpp>

#include <string>

class OBJ_Node;
class VOP_Node;
namespace NSI { class Context; }

/**
	\brief A base class for any object export in 3delight-for-houdini (OBJ +
	VOP)
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
