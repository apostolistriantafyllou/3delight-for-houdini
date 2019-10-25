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
		\param i_primitive_index
			The index of this primitive exporter inside the associated object's
			geometry exporter.
	*/
	primitive(
		const context& i_context,
		OBJ_Node* i_object,
		const GT_PrimitiveHandle& i_gt_primitive,
		unsigned i_primitive_index);

	void connect()const override;

	/**
		\brief Declare this as an instnaced object. Such objects
		do not have to be connected to a parent transform because
		they will be using the 'instance' exporter.
	*/
	void set_as_instanced( void ) { m_instanced = true; }

	/// Returns true if the primitive should be rendered as a volume
	virtual bool is_volume()const;

protected:

	GT_PrimitiveHandle m_gt_primitive;

private:

	/** =true if instanced geo */
	bool m_instanced{false};
};
