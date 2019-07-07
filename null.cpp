
#include "null.h"

#include <OBJ/OBJ_Node.h>

#include <GT/GT_Handles.h>

#include <assert.h>
#include <nsi.hpp>
#include <iostream>

null::null(
	NSI::Context &i_nsi,
	OBJ_Node *i_object,
	const GT_PrimitiveHandle &i_gt_primitive )
:
	exporter( i_nsi, i_object, i_gt_primitive )
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
	Unlike geo, the parent of a null node it's really the parent in
	the scene hierarchy.
*/
const OBJ_Node *null::parent( void ) const
{
	assert( m_object );
	return m_object->getParentObject();
}
