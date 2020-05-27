#include <OP/OP_BundlePattern.h>
#include <OBJ/OBJ_Node.h>

#include "object_visibility_resolver.h"
#include "ui/settings.h"

object_visibility_resolver::object_visibility_resolver(
	const std::string &i_rop_path, const settings &i_settings,
	double i_time )
:
	m_current_time(i_time),
	m_rop_path(i_rop_path)
{
	UT_String lights_to_render, objects_to_render;
	bool override_display_flags = i_settings.OverrideDisplayFlags();

	if( override_display_flags )
	{
		m_objects_to_render_pattern =
			OP_BundlePattern::allocPattern( i_settings.GetObjectsToRender() );

		m_lights_to_render_pattern =
			OP_BundlePattern::allocPattern( i_settings.GetLightsToRender() );
	}

	m_mattes_pattern =
		OP_BundlePattern::allocPattern( i_settings.get_matte_objects() );
}

object_visibility_resolver::~object_visibility_resolver()
{
	if( m_lights_to_render_pattern &&
		m_objects_to_render_pattern )
	{
		OP_BundlePattern::freePattern( m_lights_to_render_pattern );
		OP_BundlePattern::freePattern( m_objects_to_render_pattern );
	}

	OP_BundlePattern::freePattern( m_mattes_pattern );
}

bool object_visibility_resolver::object_displayed(const OBJ_Node& i_node)const
{
	OP_BundlePattern* pattern =
		const_cast<OBJ_Node&>(i_node).castToOBJLight()
		? m_lights_to_render_pattern
		: m_objects_to_render_pattern;

	if(pattern)
	{
		return pattern->match(&i_node, m_rop_path.c_str(), true);
	}

	return i_node.getObjectDisplay( m_current_time );
}

bool object_visibility_resolver::object_is_matte(const OBJ_Node& i_node)const
{
	return
		m_mattes_pattern &&
		m_mattes_pattern->match(&i_node, m_rop_path.c_str(), true);
}
