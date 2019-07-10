#include "vop.h"
#include "context.h"
#include "3Delight/ShaderQuery.h"
#include "shader_library.h"

#include <VOP/VOP_Node.h>
#include <OP/OP_Input.h>

#include <assert.h>
#include <nsi.hpp>
#include <algorithm>
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
	m_nsi.SetAttribute( m_handle.c_str(),
		NSI::CStringPArg( "shaderfilename", osl_name().c_str()) );
}

void vop::set_attributes( void ) const
{
}

void vop::set_attributes_at_time( double i_time ) const
{
	NSI::ArgumentList list;

	list_shader_parameters(
		m_vop,
		m_vop->getOperator()->getName(),
		i_time,
		list );

	if( !list.empty() )
	{
		m_nsi.SetAttributeAtTime( m_handle.c_str(), i_time, list );
	}
}

/**
	Connect m_vop to all its input nodes.

	Note that we don't do any verification on the parameters: our OSL
	implementation might lack one or more parameters and thic will show as
	3Delight rendering errors.

	Here "input" means the destination of the connection and "output" means the
	source of the connection.
*/
void vop::connect( void ) const
{
	for( int i = 0, n = m_vop->nInputs(); i<n; ++i )
	{
		OP_Input *input_ref = m_vop->getInputReferenceConst(i);
		if( !input_ref )
			continue;

		int output_index = input_ref->getNodeOutputIndex();
		VOP_Node *output = CAST_VOPNODE( m_vop->getInput(i) );

		if( !output )
		{
			continue;
		}

		UT_String input_name, output_name;

		output->getOutputName(output_name, output_index);
		m_vop->getInputName(input_name, i);

		m_nsi.Connect(
			output->getFullPath().buffer(), output_name.buffer(),
			m_handle.c_str(), input_name.buffer() );
	}
}

void vop::list_shader_parameters(
	const OP_Parameters *i_parameters,
	const char *i_shader,
	float i_time,
	NSI::ArgumentList &o_list )
{
	const shader_library &library = shader_library::get_instance();
	std::string path = library.get_shader_path(i_shader);
	if( path.size() == 0 )
	{
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
			o_list.Add(
				new NSI::FloatArg(
					parameter->name.c_str(),
					i_parameters->evalFloat(index, 0, i_time)) );
			break;
		}
	}
}

std::string vop::osl_name( void ) const
{
	std::string legalized( m_vop->getOperator()->getName().buffer() );

	std::replace( legalized.begin(), legalized.end(), ':', '-' );

	return legalized;
}
