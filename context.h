#pragma once

#include <nsi.hpp>

#include <OP/OP_BundlePattern.h>
#include <SYS/SYS_Types.h>

#include <assert.h>

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
		const UT_String& i_objects_to_render,
		const UT_String& i_lights_to_render)
	:
		m_nsi(i_nsi),
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
			OP_BundlePattern::allocPattern(i_objects_to_render)),
		m_lights_to_render_pattern(
			OP_BundlePattern::allocPattern(i_lights_to_render))
	{
		assert(!m_ipr || !m_export_nsi);
	}

	~context()
	{
		OP_BundlePattern::freePattern(m_lights_to_render_pattern);
		OP_BundlePattern::freePattern(m_objects_to_render_pattern);
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

public:
	NSI::Context &m_nsi;
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
};
