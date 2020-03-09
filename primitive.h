#pragma once

#include "exporter.h"

#include <vector>
#include <unordered_set>
#include <string>

/// Base class for exporters of refined GT primitives.
class primitive : public exporter
{
	friend class geometry;

public:

	/**
		Constructor.

		\param i_context
			Current rendering context.
		\param i_object
			The object from which the GT primitive was retrieved.
		\param i_time
			The time at which i_object was sampled to get i_gt_primitive.
		\param i_gt_primitive
			One of the GT primitives obtained from successive refinement of
			i_object.
		\param i_primitive_index
			The index of this primitive exporter inside the associated object's
			geometry exporter.
		\param i_requires_frame_aligned_sample
			Indicates whether the primitive requires a single sample aligned on
			the exact time of a frame, even when motion blur is enabled.
	*/
	primitive(
		const context& i_context,
		OBJ_Node* i_object,
		double i_time,
		const GT_PrimitiveHandle& i_gt_primitive,
		unsigned i_primitive_index,
		bool i_requires_frame_aligned_sample = false);

	void connect()const override;

	void set_attributes()const override;

	/// Specifies the GT primitive to use when exporting one more time sample
	bool add_time_sample(double i_time, const GT_PrimitiveHandle& i_primitive);

	/// Returns true if the primitive should be rendered as a volume
	virtual bool is_volume()const;

	/**
		Returns true when the primitive requires a single sample aligned on the
		exact time of a frame, even when motion blur is enabled. Otherwise, it
		will use as many samples as specified by a properly initialized
		time_sampler object.
	*/
	bool requires_frame_aligned_sample()const
	{
		return m_requires_frame_aligned_sample;
	}

protected:

	/// Exports time-dependent attributes to NSI
	virtual void set_attributes_at_time(
		double i_time,
		const GT_PrimitiveHandle i_gt_primitive) const = 0;

	/// Returns the GT primitive to use when exporting constant attributes
	const GT_PrimitiveHandle& default_gt_primitive()const
	{
		return m_gt_primitives.front().second;
	}

	/**
		Generates and export "P" at 2 time samples, using the velocity attribute
		to compute its value.

		\param i_vertices_list
			A optional vertex list used to export "P.indices".
	*/
	bool export_extrapolated_P(
		GT_DataArrayHandle i_vertices_list = GT_DataArrayHandle())const;

	/// Returns true if i_gt_prim has a velocity attribute "v".
	static bool has_velocity(const GT_PrimitiveHandle& i_gt_prim);

	/**
		\brief Export all the attributes that the user wishes to "bind".

		We use the word bind here quiet liberaely as we don't really support
		bind. We use dlAttributeRead.
	*/
	void export_bind_attributes( OP_Node *i_obj_level_material ) const;

	/**
		Return all the materials needed by this geometry.
	*/
	void get_all_material_paths(
		std::unordered_set< std::string > &o_materials ) const;
private:
	/**
		\brief Returns attributes that are needed by the given materials.
		\ref export_bind_attributes
	*/
	void get_bind_attributes(
		std::vector<OP_Node *> &i_to_scan,
		std::vector< std::string > &o_to_export ) const;

private:

	typedef std::pair<double, GT_PrimitiveHandle> TimedPrimitive;

	/// One GT primitive for each time sample
	std::vector<TimedPrimitive> m_gt_primitives;

	bool m_requires_frame_aligned_sample;
};
