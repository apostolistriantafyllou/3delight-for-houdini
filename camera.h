#pragma once

class OBJ_Node;
class context;

/**
	\brief Exports the camera attached to the OBJ_Node.
*/
struct camera
{
	static void export_object(const context& i_ctx, OBJ_Node* i_node);
};
