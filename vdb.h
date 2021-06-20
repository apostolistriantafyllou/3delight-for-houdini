#pragma once

#include "primitive.h"

#include <string>

class OBJ_Node;

/**
	\brief Exports an NSI "volume" node.
*/
class vdb_file : public primitive
{
public:
	vdb_file(
		const context&, OBJ_Node *i_obj,
		double i_time,
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

	/// Get existing grid names for a specific vdb.
	static bool get_grid_names(const char* i_vdb_path,
		int* num_grids,
		const char* const** grid_names);
protected:
	const std::string& path()const { return m_vdb_file; }

private:
	/* VDB file path. */
	std::string m_vdb_file;
};


/**
	\brief Exports a file node that points to a VDB file.

	This support a specific Houdini network which is one OBJ
	node with a SOP file inside that references a VDB file path.
*/
class vdb_file_loader : public vdb_file
{
public:

	vdb_file_loader(
		const context& i_ctx,
		OBJ_Node* i_obj,
		double i_time,
		const GT_PrimitiveHandle &i_handle,
		unsigned i_primitive_index);

	/**
		\brief Returns the path of the VDB file used by i_node.
		
		If i_node is not supported by vdb_file_loader, an empty string is
		returned instead.
	*/
	static std::string get_path(OBJ_Node *i_node, double i_time);

protected:

	/// Exports time-dependent attributes to NSI
	void set_attributes_at_time(
		double i_time,
		const GT_PrimitiveHandle i_gt_primitive)const override;

private:

	/**
		\brief Retrieves useful stuff all at once.
		This avoids scattering the same logic through the class by retrieving
		each value separately.
	*/
	static void get_geo(
		OBJ_Node* i_obj,
		double i_time,
		SOP_Node*& o_transform_sop,
		SOP_Node*& o_file_sop,
		std::string& o_vdb_path);
};


/**
	\brief Exports a VDB primitive through the use of a VDB file.
	
	This collects VDB grids and writes them to a file, then exports an NSI "vdb"
	node that will read it back.
*/
class vdb_file_writer : public vdb_file
{
public:
	vdb_file_writer(
		const context& i_ctx,
		OBJ_Node* i_obj,
		double i_time,
		const GT_PrimitiveHandle& i_handle,
		unsigned i_primitive_index,
		const std::string& i_vdb_path);
	
	/**
		\brief Adds a grid to be export to the temporary VDB file.

		\param i_handle
			Handle to a GT_PrimVDB object that holds a VDB grid.
		\returns
			true if the grid can be added to this primitive exporter, false if it
			needs to go into another primitive (this can happen if we already
			have a grid with the same name, or if the new grid's transform is
			not the same as the others).
		
		All calls to add_grid() must occur before create() is called, since the latter
		will also write them to the temporary VDB file.
	*/
	bool add_grid(const GT_PrimitiveHandle& i_handle);

	void create()const override;

private:
	// Holds VDB grids until they're exported to a temporary file
	mutable std::vector<GT_PrimitiveHandle> m_grids;
};
