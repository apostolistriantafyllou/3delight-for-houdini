#include "context.h"

bool context::object_displayed( const OBJ_Node& i_node ) const
{
	return m_object_visibility_resolver.object_displayed( i_node );
}

bool context::object_is_matte( const OBJ_Node& i_node ) const
{
	return m_object_visibility_resolver.object_is_matte( i_node );
}
