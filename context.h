#pragma once

#include "safe_interest.h"

#include <nsi.hpp>

#include <OP/OP_BundlePattern.h>
#include <SYS/SYS_Types.h>
#include <UT/UT_TempFileManager.h>

#include <assert.h>
#include <string>
#include <vector>

class OBJ_Node;

/**
	\brief An export context passed around to each exporter. Allows us
	to be less verbose when passing parameters around.

	One such instance should be valid throughout the entire scene export.
*/
class context
{
public:
	context(
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
		bool i_cloud,
		const std::string& i_rop_path,
		bool i_override_display_flags,
		const UT_String& i_objects_to_render,
		const UT_String& i_lights_to_render,
		const UT_String& i_matte_objects )
	:
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
		m_cloud(i_cloud),
		m_rop_path(i_rop_path),
		m_objects_to_render_pattern(
			i_override_display_flags
			? OP_BundlePattern::allocPattern(i_objects_to_render)
			: nullptr),
		m_lights_to_render_pattern(
			i_override_display_flags
			? OP_BundlePattern::allocPattern(i_lights_to_render)
			: nullptr),
		m_mattes_pattern( OP_BundlePattern::allocPattern(i_matte_objects) )
	{
		assert(!m_ipr || !m_export_nsi);
	}

	~context()
	{
		if( m_lights_to_render_pattern &&
			m_objects_to_render_pattern &&
			m_mattes_pattern )
		{
			OP_BundlePattern::freePattern(m_lights_to_render_pattern);
			OP_BundlePattern::freePattern(m_objects_to_render_pattern);
			OP_BundlePattern::freePattern(m_mattes_pattern);
		}

		for( const auto &f : m_temp_filenames )
		{
			UT_TempFileManager::removeTempFile( f.data() );
		}
	}

	/// Returns true if motion blur is required for this render
	bool MotionBlur()const { return m_shutter > 0.0f; }
	/// Returns the time at which the shutter opens
	double ShutterOpen()const { return m_current_time - m_shutter/2.0f; }
	/// Returns the time at which the shutter closes
	double ShutterClose()const { return m_current_time + m_shutter/2.0f; }

	/// Returns true if a single frame is to be rendered
	bool SingleFrame()const { return m_start_time == m_end_time; }

	/// Returns true if rendering should be done in a background thread
	bool BackgroundThreadRendering()const
	{
		return SingleFrame() && !m_cloud && !m_export_nsi && !m_batch;
	}

	/**
		\brief Returns true if rendering should be done in a background process.

		We use a renderdl process in order to start rendering as soon as the
		first frame is exported.
	*/
	bool BackgroundProcessRendering()const
	{
		return (!SingleFrame() || m_cloud) && !m_export_nsi;
	}

	/// Returns true if an object is to be rendered.
	bool object_displayed(const OBJ_Node& i_node)const;

public:
	NSI::Context &m_nsi;
	NSI::Context &m_static_nsi;
	fpreal m_start_time, m_end_time;
	fpreal m_current_time;
	fpreal m_frame_duration;
	fpreal m_shutter;
	// True if depth-of-field is enabled
	bool m_dof;
	bool m_batch;
	bool m_ipr;
	bool m_export_nsi;
	bool m_cloud;

	// Full path of the 3Delight ROP from where rendering originates
	std::string m_rop_path;

	// Objects export filter
	OP_BundlePattern* m_objects_to_render_pattern;
	// Lights export filter
	OP_BundlePattern* m_lights_to_render_pattern;

	/** All these objects will be tagged as matte */
	OP_BundlePattern* m_mattes_pattern;

	/** files to be deleted at render end. */
	mutable std::vector< std::string > m_temp_filenames;
	
	/*
		List of interests (callbacks) created in IPR mode.
		We don't use a vector because there might be a lot of items (1 or 2 per
		node) and the re-allocation pattern of std::vector implies copying its
		into a bigger array. In the case of our safe_interest class, this would
		mean additional connections (in copy-constructor) and disconnections (in
		destructor) from nodes, so the callbacks point to the right
		safe_interest object.
	*/
	mutable std::deque<safe_interest> m_interests;
};
