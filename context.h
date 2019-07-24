#pragma once

#include <nsi.hpp>

#include <SYS/SYS_Types.h>

#include <set>

class OBJ_Node;
class OP_BundlePattern;

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
		bool i_preview,
		OP_BundlePattern* i_lights_to_render_pattern)
	:
		m_nsi(i_nsi),
		m_start_time(i_start_time),
		m_end_time(i_end_time),
		m_current_time(i_start_time),
		m_frame_duration(1.0f / i_fps),
		m_shutter(i_shutter_interval * m_frame_duration),
		m_dof(i_dof),
		m_preview(i_preview),
		m_lights_to_render_pattern(i_lights_to_render_pattern)
	{
	}

	float ShutterOpen()const { return m_current_time - m_shutter/2.0f; }
	float ShutterClose()const { return m_current_time + m_shutter/2.0f; }

public:
	NSI::Context &m_nsi;
	fpreal m_start_time, m_end_time;
	fpreal m_current_time;
	fpreal m_shutter;
	fpreal m_frame_duration;
	// True if depth-of-field is enabled
	bool m_dof;
	bool m_preview;

	OP_BundlePattern* m_lights_to_render_pattern;
	std::vector<OBJ_Node*> m_lights_to_render;

	/*
		Cache of "lightcategories" expressions that already have a matching NSI
		"set" node in the scene. All lights are on by default, so light linking
		is used to turn them off. This requires that the NSI sets we have
		exported contains the *complement* of their corresponding expression.
		Without this cache, we would have to test every object's light
		categories expression against every light's categories list, which would
		be a waste because those expressions tend be re-used on multiple
		objects.
	*/
	mutable std::set<std::string> m_exported_lights_categories;
};
