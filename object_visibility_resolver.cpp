#include <OP/OP_BundlePattern.h>
#include <OBJ/OBJ_Node.h>
#include <OP/OP_BundleList.h>
#include <OP/OP_Bundle.h>
#include <OP/OP_Director.h>

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
	bool override_display_flags = i_settings.OverrideDisplayFlags(i_time);

	if( override_display_flags )
	{
		m_objects_to_render_pattern =
			OP_BundlePattern::allocPattern( i_settings.GetObjectsToRender(i_time) );

		m_lights_to_render_pattern =
			OP_BundlePattern::allocPattern(i_settings.GetLightsToRender(i_time));
	}

	m_mattes_pattern =
		OP_BundlePattern::allocPattern( i_settings.get_matte_objects(i_time) );
}

object_visibility_resolver::~object_visibility_resolver()
{
	if(m_lights_to_render_pattern  && m_objects_to_render_pattern )
	{
		OP_BundlePattern::freePattern(m_lights_to_render_pattern);
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
		/*
			Patterns can also contains bundles, so we check for bundles,
			and if any bundle is used as a pattern, we check the node
			included on bundle patterns if they match with nodes on the
			scene. OP_BundleList::getBundle() function needs bundle name
			as an argument but without the first character @.
		*/
		OP_BundleList* blist = OPgetDirector()->getBundles();
		assert(blist);
		const char* pattern_str = pattern->argv(0)+1;
		OP_Bundle* bundle = blist->getBundle(pattern_str);

		if (bundle && bundle->contains(i_node.castToOPNode(), false))
		{
			pattern = OP_BundlePattern::allocPattern(i_node.getFullPath());
			return pattern->match(&i_node, m_rop_path.c_str(), true);
		}
		return pattern->match(&i_node, m_rop_path.c_str(), true);
	}

	/*
		Check only the display channel (light_enable parameter)
		for light sources and ignore the display flag.
	*/
	if (const_cast<OBJ_Node&>(i_node).castToOBJLight())
		return !i_node.isDisplayDisabled( m_current_time );

	return i_node.getObjectDisplay( m_current_time );
}

bool object_visibility_resolver::object_is_matte(const OBJ_Node& i_node)const
{
	return
		m_mattes_pattern &&
		m_mattes_pattern->match(&i_node, m_rop_path.c_str(), true);
}
