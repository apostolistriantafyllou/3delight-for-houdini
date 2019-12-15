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
}

void null::create( void ) const
{
	m_nsi.Create( m_handle.c_str(), "transform" );
}

void null::set_attributes( void ) const
{
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
		m_handle.c_str(),
		i_time,
		NSI::DoubleMatrixArg( "transformationmatrix", local.data() ) );
}

/**
	\brief Connect to the parent object of this null object. This could
	be the immediate parent in Houdini's scene hierarchy or NSI_SCENE_ROOT
	if this null object has no parent.

	Note that Scene Hierarchy in Houdini is a very murky concept. Here we
	need to check both getParentObject(), which seems to be one way to
	do scene hierarchies, and getParent(), which seems to be the other
	way (OBJ-level nested hierarchies).
*/
void null::connect( void ) const
{
	assert( m_object );
	OP_Node *parent = m_object->getParentObject();

	if( parent )
	{
		m_nsi.Connect(
			m_handle, "",
			parent->getFullPath().buffer(), "objects" );
	}
	else
	{
		OP_Node *obj_node = OPgetDirector()->findNode("/obj");
		parent = m_object->getParent();
		assert( parent );
		if( parent == obj_node || parent == nullptr )
		{
			m_nsi.Connect(
				m_handle, "",
				NSI_SCENE_ROOT, "objects" );
		}
		else
		{
			m_nsi.Connect(
				m_handle, "",
				parent->getFullPath().c_str(), "objects" );
		}
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
	if(i_type == OP_PARM_CHANGED)
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
}
