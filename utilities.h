#pragma once

class OP_Node;
class OBJ_Node;

namespace utilities
{
	extern "C" const char *k_nsi_transform;
	extern "C" const char *k_nsi_mesh;
	extern "C" const char *k_nsi_camera;

	/**
		\brief Returns the NSI handle for HOUDINI node.
	*/
	const char * handle( const OP_Node *i_node );

	/**
		\brief Returns "mesh"/"transform"/etc.
	*/
	const char *nsi_node_type( const OBJ_Node *i_node );

	/// Returns the global frame rate, per second
	float fps();
}
