#pragma once

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
		bool i_motion )
	:
		m_nsi(i_nsi),
		m_start_time(i_start_time),
		m_end_time(i_end_time),
		m_motion(i_motion)
	{
	}

public:
	NSI::Context &m_nsi;
	fpreal m_start_time, m_end_time;
	bool m_motion;
};
