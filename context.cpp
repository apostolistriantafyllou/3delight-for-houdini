#include "context.h"
#include "ROP_3Delight.h"
#include <nsi_dynamic.hpp>

static NSI::DynamicAPI s_api;
static NSI::Context s_bad_context(s_api);

context::context(
	ROP_3Delight *i_rop, fpreal i_time )
:
	m_start_time(i_time),
	m_end_time(i_time), m_current_time(i_time),
	m_rop_path(i_rop->getFullPath().toStdString()),
	m_nsi(s_bad_context),
	m_static_nsi(s_bad_context),
	m_settings(i_rop->get_settings())
{
	m_object_visibility_resolver =
		new object_visibility_resolver(m_rop_path, m_settings, i_time);
}

bool context::object_displayed( const OBJ_Node& i_node ) const
{
	return m_object_visibility_resolver->object_displayed( i_node );
}

bool context::object_is_matte( const OBJ_Node& i_node ) const
{
	return m_object_visibility_resolver->object_is_matte( i_node );
}

const OP_BundlePattern* context::lights_to_render()const
{
	assert(m_object_visibility_resolver);
	return m_object_visibility_resolver->m_lights_to_render_pattern;
}

void context::set_current_time(fpreal i_time)
{
	/*
		m_current_time was made const to ensure that it's not modified directly
		from outside the class, while still keeping it public because it was
		already widely used. But here is the right place to modify it.
	*/
	const_cast<fpreal&>(m_current_time) = i_time;

	delete m_object_visibility_resolver;
	m_object_visibility_resolver =
		new object_visibility_resolver(m_rop_path, m_settings, i_time);
}
