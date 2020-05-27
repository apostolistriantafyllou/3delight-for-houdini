#pragma once

#include "primitive.h"

#include <vector>
#include <string>
#include <unordered_set>

class scene;

/**
	\brief exports Houdini's instances. This deals with both SOP-level and
	OBJ-level instancers.

	* The SOP instancer will have a GT_PrimInstance whereas the OBJ instancer
	  has a GT_PrimPointMesh.
	* This SOP instancer has the transformation matrices easily accessible but
	  OBJ instancer doesn't do us any favors and we need to compute the
	  matrices ourselves.
	* The SOP instancer will only instance the same set of geometries whereas
	  the OBJ instancer can use the s@instance attribute.

	Note that for 3Delight it doesn't really matter what you instance: geometry
	or other instances. So we support the "Packed of Packed of ... " without
	any extra work. It just works.
*/
class instance : public primitive
{
	/** \ref scene::scan_for_instanced */
	friend scene;

public:
	instance(
		const context&,
		OBJ_Node *,
		double,
		const GT_PrimitiveHandle &,
		unsigned i_primitive_index,
		const std::vector<std::string> & );

	void create( void ) const override;
	void connect( void ) const override;

protected:
	/// Exports time-dependent attributes to NSI
	void set_attributes_at_time(
		double i_time,
		const GT_PrimitiveHandle i_gt_primitive)const override;

	void set_attributes( void ) const override;

	void get_instanced( std::unordered_set<std::string> & ) const;

private:

	/// An object/material combination being instanciated
	typedef std::pair<std::string, std::string> merge_point;

	void get_merge_points(
		std::map<merge_point, int> &o_modelindices,
		std::vector<const merge_point*> &o_merge_points ) const;

	void get_transforms(
		const GT_PrimitiveHandle i_gt_primitive,
		UT_Matrix4D *o_transforms ) const;

	std::string merge_handle(const merge_point& i_merge_point) const;
	int num_instances( void ) const;

	std::vector<std::string> m_source_models;
};
