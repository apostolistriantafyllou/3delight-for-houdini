#pragma once

#include "exporter.h"

#include <string>

/**
	\brief Exports a file node that points to a VDB file.

	This support a specific Houdini network which is one OBJ
	node with a SOP file inside that references a VDB file path.

	This is not a generic volume support for Houdini.
*/
class vdb : public exporter
{
public:
	vdb( const context&, OBJ_Node *i_obj, const std::string &i_vdb_path );

	void create( void ) const override;
	void set_attributes( void ) const override;
	void set_attributes_at_time( double i_time ) const override;

private:
	/* VDB file path. */
	std::string m_vdb_file;
};
