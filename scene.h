#pragma once

class context;
class exporter;
class OBJ_Node;
class OP_Node;
class OP_Network;

#include <vector>

/**
	\brief a scene traversal class.
*/
class scene
{
public:
	static void export_scene( const context & );

private:
	static void scan_geo_network(
		const context &i_context,
		OP_Network *i_network,
		std::vector< exporter * > &o_to_export );

	static void process_obj(
		const context &i_context,
		OP_Node *i_node,
		std::vector<exporter *> &o_to_export );
};

