#include "null.h"

#include "context.h"

#include <OBJ/OBJ_Node.h>

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
	set_attributes_at_time(m_context.m_current_time);
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
*/
void null::connect( void ) const
{
	assert( m_object );
	OBJ_Node *parent = m_object->getParentObject();

	if( parent )
	{
		m_nsi.Connect(
			m_handle.c_str(), "",
			parent->getFullPath().buffer(), "objects" );
	}
	else
	{
		m_nsi.Connect(
			m_handle.c_str(), "",
			NSI_SCENE_ROOT, "objects" );
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
		intptr_t parm_index = reinterpret_cast<intptr_t>(i_data);
		if(parm_index > 13)
		{
			/*
				Those parameters seem to have no effect on the transform : they
				are	also displayed in other tabs of the object's parameters
				sheet.
			*/
			return;
		}

		null null_node(*ctx, i_caller->castToOBJNode());
		ctx->m_nsi.DeleteAttribute(null_node.m_handle, "transformationmatrix");
		null_node.set_attributes_at_time(ctx->m_current_time);

		ctx->m_nsi.RenderControl(NSI::CStringPArg("action", "synchronize"));
	}
}
