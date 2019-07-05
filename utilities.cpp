#include "utilities.h"

#include <assert.h>

#include <OP/OP_Node.h>
#include <OBJ/OBJ_Node.h>
#include <string>

namespace utilities
{

const char *k_nsi_transform = "transform";
const char *k_nsi_mesh = "mesh";
const char *k_nsi_camera = "camera";

const char *handle( const OP_Node *i_node )
{
	return i_node->getFullPath().buffer();
}

const char *nsi_node_type( const OBJ_Node *i_node )
{
	int type = i_node->getObjectType();

	if( type & OBJ_NULL )
		return k_nsi_transform;
	else if( type & OBJ_GEOMETRY )
		return k_nsi_mesh;
	else if( type & OBJ_CAMERA )
		return k_nsi_camera;

	assert( !"utilities::nsi_node_type" );
	return "unknown";
}

}
