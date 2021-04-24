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
	const char* k_use_alembic_procedural = "_3dl_use_alembic_procedural";
}

/** We rely on the 3Delight alembic procedural parameter */
bool time_sampler::is_alembic( OBJ_Node &i_node, double t )
{
	/*
		Alembic archives do their own motion blur. We rely on the
		alembic archive flag for now.
	*/
	return i_node.hasParm(k_use_alembic_procedural) &&
		i_node.evalInt(k_use_alembic_procedural, 0, t);
}

bool time_sampler::is_time_dependent(
	OBJ_Node& i_node,
	const context& i_context,
	time_sampler::blur_source i_type)
{
	OP_Context op_ctx(i_context.m_current_time);
	if( is_alembic(i_node, i_context.m_current_time) )
	{
		/* Alembic archives do their own motion blur. */
		printf( "is_time_dependent: ALEMBIC!\n" );
		return false;
	}

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
	/* Alembic archives do their own motion blur. */
	if( is_alembic(i_node, i_context.m_current_time) )
	{
		m_nb_intervals = 0;
		return;
	}

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
