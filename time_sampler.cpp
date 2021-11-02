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

	bool is_alembic(const OBJ_Node& i_node)
	{
		/*
			This is a very poor way of checking whether the node contains
			Alembic primitives. However, it works without having to refine the
			node into GT primitives first, which is what we need.
		*/
		return i_node.hasParm("_3dl_use_alembic_procedural");
	}
}

bool time_sampler::is_time_dependent(
	OBJ_Node& i_node,
	const context& i_context,
	time_sampler::blur_source i_type)
{
	OP_Context op_ctx(i_context.m_current_time);

	SOP_Node* sop =
		i_type == time_sampler::e_deformation
		?	i_node.getRenderSopPtr()
		:	nullptr;
	
	/*
		When the node hasn't been displayed by Houdini yet, but we need to
		render it, Houdini needs to be shaken up a little bit in order to be
		able to answer the "isTimeDependent" request.
		This happens when the node's Display flag is turned off and we still
		want to render it because it's part of the "objects to render" or
		"lights to render", or when rendering in batch mode, which means no
		node will ever be displayed.
	*/
	if(i_context.object_displayed(i_node) || i_node.castToOBJCamera())
	{
		if(sop)
		{
			if(!sop->isTimeDependent(op_ctx))
			{
				sop->getCookedGeoHandle(op_ctx);
			}
		}
		else
		{
			if(!i_node.isTimeDependent(op_ctx))
			{
				UT_DMatrix4 local;
				i_node.getLocalTransform(op_ctx, local);
			}
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
				is_time_dependent(i_node, i_context, i_type)
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
			We either have velocity-based blur or no motion blur at all, so we
			need to sample the exact time of the frame.
		*/
		m_first = m_last = i_context.m_current_time;
	}

	/*
		Alembic archives need an odd number of samples (which means an even
		number of intervals) so primitive::default_gt_primitive() returns a
		primitive that is aligned on the frame, which is the one that should be
		used when computing the procedural's "abc_time". This depends on
		context::ShutterOpen() and context::ShutterClose() being equidistant
		from context::current_time() (which will be the case as long as their
		implementation doesn't change).
	*/
	if(i_type == e_deformation && is_alembic(i_node))
	{
		m_nb_intervals += m_nb_intervals % 2;
	}
}
