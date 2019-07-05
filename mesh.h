#pragma once

class OBJ_Node;
class context;

/**
	\brief Poly and poly soupe exporter.
*/
struct mesh
{
	static void export_object( const context &, OBJ_Node * );
};
