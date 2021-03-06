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
	*/
	primitive(
		const context& i_context,
		OBJ_Node* i_object,
		double i_time,
		const GT_PrimitiveHandle& i_gt_primitive,
		unsigned i_primitive_index );

	void connect()const override;

	void set_attributes()const override;

	/**
		\brief Specifies the GT primitive to use when exporting a time sample.

		\param i_time
			The time at which i_primitive was generated. This additional time
			sample might be ignored if a sample at i_time is not needed, either
			because the primitive only requires a single sample at the current
			frame's time or, conversely because the primitive already has all
			multiple samples and the additional sample was meant for those
			primitive that require a frame-aligned sample.
		\param i_primitive
			The GT primitive holding geometry at time i_time.

		\returns
			false when one of the time samples is incompatible with those
			already present in the primitive, true otherwise.
	*/
	bool add_time_sample(double i_time, const GT_PrimitiveHandle& i_primitive);

	/**
		\brief Merges all time samples of i_primitive into this one's.

		\returns
			false when one of the time samples is incompatible with those
			already present in the primitive, true otherwise.
	*/
	bool merge_time_samples(const primitive& i_primitive);

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
		/*
			We use the time sample that is the closest to the middle of the
			motion interval (which should normally be the closest to the exact
			frame time). In case of an even number of samples, we prefer the
			sample that comes just after the frame, because its attributes have
			a greater probability of matching those of the primitive at the
			exact frame time (in discrete animations that update once per
			frame, for example).
		*/
		return m_gt_primitives[m_gt_primitives.size()/2].second;
	}

	/**
		Generates and export "P" at 2 time samples, using the velocity attribute
		to compute its value.

		\param i_vertices_list
			A optional vertex list used to export "P.indices".
	*/
	bool export_extrapolated_P(
		GT_DataArrayHandle i_vertices_list = GT_DataArrayHandle())const;

	/*
		\brief Returns true if thi primitive has the "v" attribute specified
		_and_ motion blur is enabled.
	*/
	bool has_velocity_blur( void ) const;

	/**
		\brief Export all the attributes that the user wishes to "bind".

		We use the word bind here quiet liberaely as we don't really support
		bind. We use dlAttributeRead.
	*/
	void export_bind_attributes( VOP_Node *i_obj_level_materials[3] ) const;

	/**
		Return all the materials needed by this geometry.
	*/
	virtual void get_all_material_paths(
		std::unordered_set< std::string > &o_materials ) const;
private:
	/**
		\brief Returns attributes that are needed by the given materials.
		\ref export_bind_attributes
	*/
	void get_bind_attributes(
		std::vector<OP_Node *> &i_to_scan,
		std::vector< std::string > &o_to_export ) const;

	void assign_sop_materials( void ) const;

private:

	typedef std::pair<double, GT_PrimitiveHandle> TimedPrimitive;

	/// One GT primitive for each time sample
	std::vector<TimedPrimitive> m_gt_primitives;
};
