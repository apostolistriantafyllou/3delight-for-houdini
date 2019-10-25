#include "time_sampler.h"

#include <OBJ/OBJ_Node.h>
#include <OP/OP_Context.h>

namespace
{
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
}
