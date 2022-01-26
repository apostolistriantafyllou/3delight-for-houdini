#include "context.h"

#include "ROP_3Delight.h"
#include <nsi_dynamic.hpp>

#include <UT/UT_TempFileManager.h>

static NSI::DynamicAPI s_api;
static NSI::Context s_bad_context(s_api);

context::context(
	ROP_3Delight *i_rop,
	const settings &i_settings,
	NSI::Context &i_nsi,
	NSI::Context &i_static_nsi,
	fpreal i_start_time,
	fpreal i_end_time,
	fpreal i_shutter_interval,
	fpreal i_fps,
	bool i_dof,
	bool i_batch,
	bool i_ipr,
	bool i_export_nsi,
	const std::string& i_export_path,
	rop_type i_rop_type)
:
	m_rop(i_rop),
	m_nsi(i_nsi),
	m_static_nsi(i_static_nsi),
	m_start_time(i_start_time),
	m_end_time(i_end_time),
	m_current_time(i_start_time),
	m_frame_duration(1.0f / i_fps),
	m_shutter(i_shutter_interval * m_frame_duration),
	m_dof(i_dof),
	m_batch(i_batch),
	m_ipr(i_ipr),
	m_export_nsi(i_export_nsi),
	m_rop_type(i_rop_type),
	m_rop_path(i_rop->getFullPath().c_str()),
	m_settings(i_settings),
	m_fps(i_fps)
{
	m_object_visibility_resolver =
		new object_visibility_resolver(m_rop_path, i_settings, i_start_time);
	set_export_path(i_export_path);
}

void context::set_export_path(const std::string& i_path)
{
	m_export_path_prefix = i_path;

	// Remove ".nsi" prefix
	std::string nsi_extension = ".nsi";
	if(m_export_path_prefix.length() > nsi_extension.length())
	{
		size_t prefix_length =
			m_export_path_prefix.length() - nsi_extension.length();
		if(m_export_path_prefix.substr(prefix_length) == nsi_extension)
		{
			m_export_path_prefix.resize(prefix_length);
		}
	}
}


context::context(
	ROP_3Delight *i_rop, fpreal i_time )
:
	m_rop(i_rop),
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


context::~context()
{
	for( const auto &f : m_temp_filenames )
	{
		UT_TempFileManager::removeTempFile( f.data() );
	}

	delete m_object_visibility_resolver;
}


bool context::object_displayed( const OBJ_Node& i_node ) const
{
	return m_object_visibility_resolver->object_displayed( i_node )
		|| m_object_visibility_resolver->object_is_matte( i_node )
		|| m_object_visibility_resolver->object_is_phantom(i_node);
}

bool context::object_is_matte( const OBJ_Node& i_node ) const
{
	return m_object_visibility_resolver->object_is_matte( i_node );
}

bool context::object_is_phantom(const OBJ_Node& i_node) const
{
	return m_object_visibility_resolver->object_is_phantom(i_node);
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

/**
	This can only happen if a user fires a single frame to be rendered
	(not exported) and that this not is not a dependency for some
	downstream node.
*/
bool context::BackgroundThreadRendering()const
{
	return SingleFrame() && m_rop_type != cloud && !m_export_nsi && !m_batch &&
		m_rop->nOutputItems() == 0;
}
