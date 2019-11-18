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

	void set_attributes()const override;

	/**
		\brief Declare this as an instnaced object. Such objects
		do not have to be connected to a parent transform because
		they will be using the 'instance' exporter.
	*/
	void set_as_instanced( void ) { m_instanced = true; }

	/// Specifies the GT primitive to use when exporting one more time sample
	bool add_time_sample(const GT_PrimitiveHandle& i_primitive);

	/// Returns true if the primitive should be rendered as a volume
	virtual bool is_volume()const;

protected:

	/// Exports time-dependent attributes to NSI
	virtual void set_attributes_at_time(
		double i_time,
		const GT_PrimitiveHandle i_gt_primitive) const = 0;

	/// Returns the GT primitive to use when exporting constant attributes
	const GT_PrimitiveHandle& default_gt_primitive()const
	{
		return m_gt_primitives.front();
	}

private:

	/// One GT primitive for each time sample
	std::vector<GT_PrimitiveHandle> m_gt_primitives;

	/** =true if instanced geo */
	bool m_instanced{false};
};