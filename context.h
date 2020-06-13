#pragma once

#include "safe_interest.h"
#include "object_visibility_resolver.h"

#include <nsi.hpp>

#include <SYS/SYS_Types.h>
#include <UT/UT_TempFileManager.h>

#include <assert.h>
#include <deque>
#include <string>
#include <vector>

class OBJ_Node;
class ROP_3Delight;

/**
	\brief An export context passed around to each exporter. Allows us
	to be less verbose when passing parameters around.

	One such instance should be valid throughout the entire scene export.
*/
class context
{
public:

	/**
		\brief A non-rendering context. Could be used in various scene-
		scanning methods for other purposes than expot.

		\ref scene::find_custom_aovs
	*/
	context( ROP_3Delight *, fpreal t );

	context(
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
		bool i_cloud,
		const std::string& i_rop_path)
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
		m_object_visibility_resolver(i_rop_path, i_settings, i_start_time)
	{
		assert(!m_ipr || !m_export_nsi);
	}

	~context()
	{

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
	/// Returns true if an object is in the mattes bundle.
	bool object_is_matte(const OBJ_Node& i_node)const;

	/**
		\brief Registers a callback to be notified of changes to a node.

		\param i_node
			Pointer to the node being watched.
		\param i_cb
			Pointer to the function callback.
	*/
	void register_interest(OP_Node* i_node, OP_EventMethod i_cb)const
	{
		assert(m_ipr);
		m_interests.emplace_back(i_node, const_cast<context*>(this), i_cb);
	}

public:
	NSI::Context &m_nsi;
	NSI::Context &m_static_nsi;
	fpreal m_start_time{0.0f}, m_end_time{.0f};
	fpreal m_current_time{.0f};
	fpreal m_frame_duration{.0f};
	fpreal m_shutter{.0f};
	// True if depth-of-field is enabled
	bool m_dof{false};
	bool m_batch{false};
	bool m_ipr{false};
	bool m_export_nsi{false};
	bool m_cloud{false};

	// Full path of the 3Delight ROP from where rendering originates
	std::string m_rop_path;

	/** files to be deleted at render end. */
	mutable std::vector< std::string > m_temp_filenames;

	object_visibility_resolver m_object_visibility_resolver;

private:
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
