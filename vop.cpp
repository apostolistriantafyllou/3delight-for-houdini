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
	const shader_library &library = shader_library::get_instance();
	std::string path = library.get_shader_path( vop_name().c_str() );
	if( path.length() == 0 )
		return;

	m_nsi.Create( m_handle, "shader" );
	m_nsi.SetAttribute( m_handle,
		NSI::CStringPArg( "shaderfilename", path.c_str()) );
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
		m_nsi.SetAttributeAtTime( m_handle, i_time, list );
	}
}

/**
	Connect m_vop to all its input nodes.

	Note that we don't do any verification on the parameters: our OSL
	implementation might lack one or more parameters.

	Some VEX operators have subnetworks (e.g. texture) but can't be supported
	with subnetworks because inline code. In that case their OSL should be
	monolithic and specify the "igonresubnetworks" tag.

		\ref osl/texture__2_0.osl

	Here "input" means the destination of the connection and "output" means the
	source of the connection.
*/
void vop::connect( void ) const
{
	if( unsupported() )
		return;

	if( ignore_subnetworks() )
		return;

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
			m_handle, input_name.buffer() );
	}
}

/**
	\brief Fill a list of NSI arguments from an OP_Parameters node.

	The only gotcha here is regarding texture parameteres: we detect
	them by checking for the srccolorspace attribute. Which means
	it's a "Texture" node. I think we need something better here but
	for now this will do.
*/
void vop::list_shader_parameters(
	const OP_Parameters *i_parameters,
	const char *i_shader,
	float i_time,
	NSI::ArgumentList &o_list )
{
	const shader_library &library = shader_library::get_instance();
	std::string path = library.get_shader_path( i_shader );
	if( path.size() == 0 )
	{
		return;
	}

	DlShaderInfo *shader_info = library.get_shader_info( path.c_str() );

	const char *k_srccolorspace = "srccolorspace";
	UT_String color_space;
	int srccolorspace_index = i_parameters->getParmIndex( k_srccolorspace );
	if( srccolorspace_index >= 0 )
	{
		i_parameters->evalString( color_space, k_srccolorspace, 0, i_time );
	}

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

		case NSITypeColor:
		case NSITypePoint:
		case NSITypeNormal:
		case NSITypeVector:
		{
			float c[3] =
			{
				(float)i_parameters->evalFloat(index, 0, i_time),
				(float)i_parameters->evalFloat(index, 1, i_time),
				(float)i_parameters->evalFloat(index, 2, i_time)
			};

			o_list.Add( new NSI::ColorArg( parameter->name.c_str(), c ) );
			break;
		}

		case NSITypeInteger:
			o_list.Add(
				new NSI::IntegerArg(
					parameter->name.c_str(),
					i_parameters->evalInt(index, 0, i_time)) );
			break;

		case NSITypeString:
		{
			UT_String str;
			i_parameters->evalString( str, parameter->name.c_str(), 0, i_time);
			o_list.Add(
				new NSI::StringArg( parameter->name.c_str(), str.buffer()) );

			if( color_space != "" )
			{
				std::string param( parameter->name.c_str() );
				param += ".meta.colorspace";
				o_list.Add( new NSI::StringArg(param, color_space.buffer()) );
			}
			break;
		}
		}
	}
}

std::string vop::vop_name( void ) const
{
	return m_vop->getOperator()->getName().buffer();
}


bool vop::ignore_subnetworks( void ) const
{
	const shader_library &library = shader_library::get_instance();
	std::string path = library.get_shader_path( vop_name().c_str() );
	if( path.size() == 0 )
	{
		return false;
	}
	DlShaderInfo *shader_info = library.get_shader_info( path.c_str() );
	const DlShaderInfo::constvector<DlShaderInfo::Parameter> &tags =
		shader_info->metadata();

	for( auto &P : tags )
	{
		if( P.name != "tags" || P.type != NSITypeString )
			continue;

		for( auto &tag : P.sdefault )
		{
			if( tag == "ignoresubnetworks" )
				return true;
		}
	}

	return false;
}

bool vop::unsupported( void ) const
{
	const shader_library &library = shader_library::get_instance();
	std::string path = library.get_shader_path( vop_name().c_str() );
	return path.size() == 0;
}
