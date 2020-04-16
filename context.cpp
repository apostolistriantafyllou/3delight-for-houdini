#include "context.h"

#include "OBJ/OBJ_Node.h"

bool
context::object_displayed(const OBJ_Node& i_node)const
{
	OP_BundlePattern* pattern =
		const_cast<OBJ_Node&>(i_node).castToOBJLight()
		?	m_lights_to_render_pattern
		:	m_objects_to_render_pattern;

	if(pattern)
	{
		return pattern->match(&i_node, m_rop_path.c_str(), true);
	}

	return i_node.getObjectDisplay(m_current_time);
}

bool
context::object_is_matte(const OBJ_Node& i_node)const
{
	return
		m_mattes_pattern &&
		m_mattes_pattern->match(&i_node, m_rop_path.c_str(), true);
}
