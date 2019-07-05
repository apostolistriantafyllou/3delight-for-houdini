#pragma once

class OBJ_Node;
class context;

/**
	\brief Exports the transform attached the OBJ_Node.
*/
struct transform
{
	static void export_object( const context &, OBJ_Node * );
};
