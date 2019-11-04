#include "time_sampler.h"

#include <OBJ/OBJ_Node.h>
#include <OP/OP_Context.h>

namespace
{
	const char* k_add_deformation_samples = "_3dl_deformation";
	const char* k_nb_additional_samples = "_3dl_add_samples";

	/*
		Calls OP_Node::isTimeDependent on i_node without using a temporary
		OP_Context, which we can't because isTimeDependent's argument is
		non-const.
	*/
	bool IsTimeDependent(OBJ_Node& i_node, double i_time)
	{
		OP_Context op_ctx(i_time);
		return i_node.isTimeDependent(op_ctx);
	}
}

time_sampler::time_sampler(const context& i_context, OBJ_Node& i_node)
	:	m_first(i_context.ShutterOpen()),
		m_length(i_context.m_shutter),
		m_nb_intervals(
			i_context.MotionBlur() &&
				IsTimeDependent(i_node, i_context.m_current_time)
			? 1
			: 0),
		m_current_sample(0)
{
	/*
		Here, we presume that the presence of k_nb_additional_samples implies
		that our other motion blur parameters are present, too.
	*/
	if(i_context.MotionBlur() && i_node.hasParm(k_nb_additional_samples))
	{
		UT_String mb;
		i_node.evalString(
			mb, k_add_deformation_samples, 0, i_context.m_current_time);

		// This avoids testing i_node.isTimeDependent a second time
		bool time_dependent = m_nb_intervals > 0;

		if(mb == "off")
		{
			m_nb_intervals = 0;
		}
		else if(time_dependent || mb == "on2")
		{
			m_nb_intervals = 1 +
				i_node.evalInt(k_nb_additional_samples, 0, i_context.m_current_time);
		}
	}
}
