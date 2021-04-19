#include "time_sampler.h"

#include <OBJ/OBJ_Node.h>
#include <OP/OP_Context.h>
#include <SOP/SOP_Node.h>

namespace
{
	const char* k_add_transformation_samples = "_3dl_transformation_blur";
	const char* k_nb_transformation_samples = "_3dl_transformation_extra_samples";
	const char* k_add_deformation_samples = "_3dl_deformation";
	const char* k_nb_deformation_samples = "_3dl_add_samples";
}

bool time_sampler::is_time_dependent(
	OBJ_Node& i_node,
	double i_time,
	time_sampler::blur_source i_type)
{
	OP_Context op_ctx(i_time);
	SOP_Node* sop =
		i_type == time_sampler::e_deformation
		?	i_node.getRenderSopPtr()
		:	nullptr;
	
	/*
		When the Display flag is turned off (which means we're rendering this
		object because it's part of the "objects to render" or "lights to
		render" set), Houdini needs to be shaken up a little bit in order to be
		able to answer the "isTimeDependent" request.
	*/
	if(!i_node.getObjectDisplay(i_time))
	{
		if(sop)
		{
			sop->getCookedGeoHandle(op_ctx);
		}
		else
		{
			UT_DMatrix4 local;
			i_node.getLocalTransform(op_ctx, local);
		}
	}

	return sop ? sop->isTimeDependent(op_ctx) : i_node.isTimeDependent(op_ctx);
}

time_sampler::time_sampler(
	const context& i_context,
	OBJ_Node& i_node,
	time_sampler::blur_source i_type)
	:	m_first(i_context.ShutterOpen()),
		m_last(i_context.ShutterClose()),
		m_nb_intervals(
			i_context.MotionBlur() &&
				is_time_dependent(i_node, i_context.m_current_time, i_type)
			? 1
			: 0),
		m_current_sample(0)
{
	const char* add_samples_attr =
		i_type == e_deformation
		? k_add_deformation_samples
		: k_add_transformation_samples;
	const char* nb_samples_attr =
		i_type == e_deformation
		? k_nb_deformation_samples
		: k_nb_transformation_samples;
	if(i_context.MotionBlur() && i_node.hasParm(add_samples_attr))
	{
		UT_String mb;
		i_node.evalString(
			mb,
			add_samples_attr,
			0,
			i_context.m_current_time);

		// This avoids calling is_time_dependent a second time
		bool time_dependent = m_nb_intervals > 0;

		if(mb == "off")
		{
			m_nb_intervals = 0;
		}
		else if(time_dependent || mb == "on2" && i_node.hasParm(nb_samples_attr))
		{
			m_nb_intervals =
				1 +
				i_node.evalInt(nb_samples_attr, 0, i_context.m_current_time);
		}
	}

	if( i_type == e_deformation )
	{
		/*
			If we have a "v" vector, we don't need any time segments we will
			be extrapolating P/matrices instead.
		*/
		OP_Context op_ctx( i_context.m_start_time );
		GU_DetailHandle gdh = i_node.getRenderGeometryHandle(op_ctx);
		GU_DetailHandleAutoReadLock rlock(gdh);
		const GU_Detail *gdp = rlock.getGdp();

		if (gdp &&
			(gdp->findAttribute(GA_ATTRIB_PRIMITIVE, "v") ||
			gdp->findAttribute(GA_ATTRIB_POINT, "v") ||
			gdp->findAttribute(GA_ATTRIB_DETAIL, "v")) )
		{
			m_nb_intervals = 0;
		}
	}

	if( m_nb_intervals == 0 )
	{
		/*
			This is needed for velocity blur to make sure we centered
			on the frame.
		*/
		m_first = m_last = i_context.m_current_time;
	}
}
