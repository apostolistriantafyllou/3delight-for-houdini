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
namespace NSI { class Context; }

/**
	\brief A base class for any object export in 3delight-for-houdini
*/
class exporter
{
public:
	exporter( const context &, OBJ_Node *, const GT_PrimitiveHandle & );

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
		\brief Returns the handle of this node.
	*/
	const std::string &handle( void ) const;

	/**
		\brief Returns the parent object for the purpose of the NSI
		Connect() call.

		read comments int scene.cpp to understand why this is so.
	*/
	virtual const OBJ_Node *parent( void ) const { return m_object; }

protected:
	OBJ_Node *m_object;

	/*
	*/
	exporter *m_parent_transform{nullptr};

	GT_PrimitiveHandle m_gt_primitive;

	/* The export context. */
	const context& m_context;

	/* The NSI export context, which is redundant with m_context.m_nsi. */
	NSI::Context &m_nsi;

	/** The NSI handle */
	std::string m_handle;
};
