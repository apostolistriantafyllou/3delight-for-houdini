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
	m_object_visibility_resolver(
		i_rop->getFullPath().toStdString(), i_rop->get_settings(), i_time),
	m_nsi(s_bad_context),
	m_static_nsi(s_bad_context)
{
}


bool context::object_displayed( const OBJ_Node& i_node ) const
{
	return m_object_visibility_resolver.object_displayed( i_node );
}

bool context::object_is_matte( const OBJ_Node& i_node ) const
{
	return m_object_visibility_resolver.object_is_matte( i_node );
}
