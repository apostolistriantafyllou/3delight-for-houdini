#include "null.h"

#include "context.h"

#include <OBJ/OBJ_Node.h>

#include <GT/GT_Handles.h>

#include <assert.h>
#include <nsi.hpp>
#include <iostream>

null::null( const context& i_ctx, OBJ_Node *i_object )
:
	exporter( i_ctx, i_object )
{
	/*
		The null object has its full path as handle.
		\ref exporter::exporter
	*/
	m_handle = i_object->getFullPath();
}

void null::create( void ) const
{
	m_nsi.Create( m_handle.c_str(), "transform" );
}

void null::set_attributes( void ) const
{
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
	be the immediate parent in Houdini's scene hierary or NSI_NODE_ROOT
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
