#pragma once

#include "utilities.h"

#include <nsi.hpp>

#include <SYS/SYS_Types.h>

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
		bool i_dof)
	:
		m_nsi(i_nsi),
		m_start_time(i_start_time),
		m_end_time(i_end_time),
		m_current_time(i_start_time),
		m_frame_duration(1.0f / utilities::fps()),
		m_shutter(i_shutter_interval * m_frame_duration),
		m_dof(i_dof)
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
};
