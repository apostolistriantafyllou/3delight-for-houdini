#include "vop.h"
#include "context.h"
#include "3Delight/ShaderQuery.h"


#include <VOP/VOP_Node.h>

#include <assert.h>
#include <nsi.hpp>
#include <iostream>

vop::vop(
	const context& i_ctx,
	VOP_Node *i_vop )
:
	exporter( i_ctx, i_vop )
{
	m_handle = i_vop->getFullPath();
}

void vop::create( void ) const
{
	m_nsi.Create( m_handle.c_str(), "shader" );
}

void vop::set_attributes( void ) const
{
}

void vop::set_attributes_at_time( double i_time ) const
{
	OP_Context context( i_time );
}

void vop::connect( void ) const
{
}
