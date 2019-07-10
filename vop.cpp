#include "vop.h"
#include "context.h"
#include "3Delight/ShaderQuery.h"
#include "shader_library.h"

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

void vop::list_shader_parameters(
	const OP_Parameters *i_parameters,
	const char *i_shader,
	NSI::ArgumentList &o_list )
{
	const shader_library &library = shader_library::get_instance();
	std::string path = library.get_shader_path(i_shader);
	if( path.size() == 0 )
	{
		assert( false );
		return;
	}

	DlShaderInfo *shader_info = library.get_shader_info( path.c_str() );

	for( int i=0; i<shader_info->nparams(); i++ )
	{
		const DlShaderInfo::Parameter *parameter = shader_info->getparam(i);
		int index = i_parameters->getParmIndex( parameter->name.c_str() );

		if( index < 0 )
			continue;

		switch( parameter->type.type )
		{
		case NSITypeFloat:
			o_list.Add( new NSI::FloatArg(
				parameter->name.c_str(), i_parameters->evalFloat(index, 0, 0)));
			break;
		}
	}
}
