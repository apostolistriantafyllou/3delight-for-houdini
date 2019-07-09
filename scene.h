#pragma once

class context;
class exporter;
class OP_Node;

#include <vector>

/**
	\brief Converts the scene (geo, shaders, light, cameras) into its
	NSI representation.
*/
class scene
{
public:
	static void convert_to_nsi( const context & );

private:
	static void scan(
		const context &i_context,
		std::vector< exporter * > &o_to_export );

	static void process_node(
		const context &i_context,
		OP_Node *i_node,
		std::vector<exporter *> &o_to_export );
};
