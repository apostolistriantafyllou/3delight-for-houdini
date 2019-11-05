#pragma once

#include "exporter.h"

#include <vector>

/// Base class for exporters of refined GT primitives.
class primitive : public exporter
{
public:

	/**
		Constructor.

		\param i_context
			Current rendering context.
		\param i_object
			The object from which the GT primitive was retrieved.
		\param i_gt_primitive
			One of the GT primitives obtained from successive refinement of
			i_object.
	*/
	primitive(
		const context& i_context,
		OBJ_Node* i_object,
		const GT_PrimitiveHandle& i_gt_primitive);

	void connect()const override;

	/**
		\brief Declare this as an instnaced object. Such objects
		do not have to be connected to a parent transform because
		they will be using the 'instance' exporter.
	*/
	void set_as_instanced( void ) { m_instanced = true; }

protected:

	GT_PrimitiveHandle m_gt_primitive;

private:

	/** =true if instanced geo */
	bool m_instanced{false};
};
