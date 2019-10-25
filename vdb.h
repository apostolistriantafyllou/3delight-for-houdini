#pragma once

#include "primitive.h"

#include <string>

/**
	\brief Exports a file node that points to a VDB file.

	This support a specific Houdini network which is one OBJ
	node with a SOP file inside that references a VDB file path.

	This is not a generic volume support for Houdini.
*/
class vdb : public primitive
{
public:
	vdb(
		const context&, OBJ_Node *i_obj,
		const GT_PrimitiveHandle &i_handle,
		unsigned i_primitive_index,
		const std::string &i_vdb_path );

	void create( void ) const override;
	void set_attributes( void ) const override;

	bool is_volume()const override;

	/// Exports time-dependent attributes to NSI
	void set_attributes_at_time(
		double i_time,
		const GT_PrimitiveHandle i_gt_primitive)const override;

private:
	/* VDB file path. */
	std::string m_vdb_file;
};
