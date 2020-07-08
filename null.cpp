#include "null.h"

#include "context.h"
#include "time_sampler.h"

#include <OBJ/OBJ_Node.h>
#include <OP/OP_Director.h>
#include <GT/GT_Handles.h>

#include <nsi.hpp>

null::null( const context& i_ctx, OBJ_Node *i_object )
:
	exporter( i_ctx, i_object )
{
	m_handle = handle(*m_object, i_ctx);
}

void null::create( void ) const
{
	m_nsi.Create( m_handle.c_str(), "transform" );
}

void null::set_attributes( void ) const
{
	if(m_context.m_ipr)
	{
		m_context.m_nsi.DeleteAttribute(
			m_handle,
			"transformationmatrix");
	}

	for(time_sampler t(m_context, *m_object, time_sampler::e_transformation); t; t++)
	{
		set_attributes_at_time(*t);
	}
}

void null::set_attributes_at_time( double i_time ) const
{
	OP_Context context( i_time );
	UT_DMatrix4 local;
	m_object->getLocalTransform( context, local );

	/* The stars are aligned for Houdini and NSI */
	m_nsi.SetAttributeAtTime(
		m_handle,
		i_time,
		NSI::DoubleMatrixArg( "transformationmatrix", local.data() ) );
}

/**
	\brief Connect to the parent object of this null object. This could
	be the immediate parent in Houdini's scene hierarchy or NSI_SCENE_ROOT
	if this null object has no parent.

	Note that Scene Hierarchy in Houdini is a very murky concept. Here we
	take the parent node that has the longest scene path. Why ? Because
	it's the one closest to us.
*/
void null::connect( void ) const
{
	if( m_instanced )
	{
		/* This transform is used by the OBJ-level instancer. Don't connect. */
		return;
	}

	assert( m_object );

	OP_Node *parent_node = m_object->getParent();
	OP_Node *parent_object = m_object->getParentObject();

	std::string parent_node_path(
		parent_node ?
			parent_node->getFullPath().toStdString() : std::string() );

	std::string parent_object_path(
		parent_object ?
			parent_object->getFullPath().toStdString() : std::string() );

	OP_Node* parent = parent_object_path.size() > parent_node_path.size() ?
		parent_object : parent_node;

	if( parent && parent->getFullPath() != "/obj" )
	{
		m_nsi.Connect(
			m_handle, "",
			null::handle(*parent, m_context), "objects" );
	}
	else
	{
		m_nsi.Connect( m_handle, "", NSI_SCENE_ROOT, "objects" );
	}
}

/**
	\brief Callback that should be connected to an OP_Node that has an
	associated null exporter.
*/
void null::changed_cb(
	OP_Node* i_caller,
	void* i_callee,
	OP_EventType i_type,
	void* i_data)
{
	context* ctx = (context*)i_callee;

	/*
		FIXME : We normally shouldn't react to OP_UI_MOVED because this type of
		event is supposed to be generated only when the node's location in the
		graph view changes, which has nothing to do with its actual position in
		the 3D scene. However, it seems to be the only event we can trap after
		the copy & paste of an object, once its transform has been updated.
		Otherwise, it remains (incorrectly) set to the identity transform in the
		NSI scene.
	*/
	if(i_type == OP_PARM_CHANGED || i_type == OP_UI_MOVED)
	{
		if(!is_transform_parameter_index(reinterpret_cast<intptr_t>(i_data)))
		{
			return;
		}

		null null_node(*ctx, i_caller->castToOBJNode());
		ctx->m_nsi.DeleteAttribute(null_node.m_handle, "transformationmatrix");
		null_node.set_attributes_at_time(ctx->m_current_time);

		ctx->m_nsi.RenderControl(NSI::CStringPArg("action", "synchronize"));
	}
	else if(i_type == OP_NODE_PREDELETE)
	{
		OBJ_Node* obj = i_caller->castToOBJNode();
		assert(obj);
		ctx->m_nsi.Delete(handle(*obj, *ctx));
	}
}

void null::Delete(OBJ_Node& i_node, const context& i_context)
{
	i_context.m_nsi.Delete(handle(i_node, i_context));
}
