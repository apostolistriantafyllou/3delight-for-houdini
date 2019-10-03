#include "vop.h"
#include "context.h"
#include "osl_utilities.h"
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

		if ( is_aov_definition(output) )
		{
			/*
				Special case for 'bind' export node: if we use output_name,
				this will be unrecognized by nsi since output_name is modifued
				each time user specify a new name for its export (custom) name
			*/
			m_nsi.Connect(
				output->getFullPath().buffer(), "outColor",
				m_handle, input_name.buffer() );
		}
		else
		{
			m_nsi.Connect(
				output->getFullPath().buffer(), output_name.buffer(),
				m_handle, input_name.buffer() );
		}
	}

	// If needed, add and connect our aov group to our material
	add_and_connect_aov_group();
}

/**
	\brief Fill a list of NSI arguments from an OP_Parameters node.

	The only gotcha here is regarding texture parameters: we detect
	them by checking for the srccolorspace or texcolorspace attribute.
	Which means	it's a "Texture" node. If these attributes are not found,
	we check if "texture" or "file" is in the operator's name. Also we
	check if "texture" or "map" appears in parameter's name. I think we
	need something better here but for now this will do.
*/
void vop::list_shader_parameters(
	const OP_Parameters *i_parameters,
	const char *i_shader,
	float i_time,
	NSI::ArgumentList &o_list )
{
	assert( i_shader );
	assert( i_parameters );

	const shader_library &library = shader_library::get_instance();
	std::string path = library.get_shader_path( i_shader );
	if( path.size() == 0 )
	{
		return;
	}

	DlShaderInfo *shader_info = library.get_shader_info( path.c_str() );

	const char *k_srccolorspace = "srccolorspace";
	const char *k_texcolorspace = "texcolorspace";
	UT_String color_space;
	int colorspace_index = i_parameters->getParmIndex( k_srccolorspace );
	if( colorspace_index >= 0 )
	{
		i_parameters->evalString( color_space, k_srccolorspace, 0, i_time );
	}
	else
	{
		colorspace_index = i_parameters->getParmIndex( k_texcolorspace );
		if( colorspace_index >= 0 )
		{
			i_parameters->evalString( color_space, k_texcolorspace, 0, i_time );
		}
		else
		{
			color_space = "auto";
		}
	}

	bool isTextureNode = colorspace_index >= 0;
	if (!isTextureNode)
	{
		// Check if the operator's name contains "texture"
		OP_Operator* op = i_parameters->getOperator();
		assert(op);
		const UT_StringHolder& sh = op->getName();

		UT_String name = sh.c_str();
		name.toLower();

		std::string::size_type pos = name.toStdString().find("texture");
		isTextureNode = pos != std::string::npos;

		// Check if the operator's name contains "file"
		if (!isTextureNode)
		{
			pos = name.toStdString().find("file");
			isTextureNode = pos != std::string::npos;
		}
	}

	for( int i=0; i<shader_info->nparams(); i++ )
	{
		const DlShaderInfo::Parameter *parameter = shader_info->getparam(i);

		// Skip auxiliary shader parameters
		const char* related_to_widget = nullptr;
		osl_utilities::FindMetaData(
			related_to_widget,
			parameter->metadata,
			"related_to_widget");
		if(related_to_widget)
		{
			// This parameter will be exported as part of a group
			continue;
		}

		// Process ramp parameters differently
		const char* widget = "";
		osl_utilities::FindMetaData(widget, parameter->metadata, "widget");
		osl_utilities::ramp::eType ramp_type =
			osl_utilities::ramp::GetType(widget);
		if(osl_utilities::ramp::IsRamp(ramp_type))
		{
			list_ramp_parameters(
				i_parameters,
				*parameter,
				ramp_type,
				i_time,
				o_list);
			continue;
		}

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

			if( isTextureNode || is_texture_path( parameter->name.c_str() ) )
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

bool vop::is_texture_path( const char* i_param_name )
{
	UT_String name = i_param_name;
	name.toLower();

	std::string str = name.toStdString();
	return
		str.find("texture") != std::string::npos ||
		str.find("map") != std::string::npos;
}

void vop::add_and_connect_aov_group() const
{
	// First, check if this node represents our material

	const shader_library &library = shader_library::get_instance();
	std::string path = library.get_shader_path( vop_name().c_str() );
	if( path.size() == 0 )
	{
		return;
	}

	DlShaderInfo *shader_info = library.get_shader_info( path.c_str() );
	bool ourMaterial = false;
	for( int i=0; i<shader_info->nparams(); i++ )
	{
		const DlShaderInfo::Parameter *parameter = shader_info->getparam(i);
		if ( strcmp( parameter->name.c_str(), "aovGroup" ) == 0 )
		{
			ourMaterial = true;
			break;
		}
	}

	if ( !ourMaterial )
		return;

	// Second, check if any 'bind' node is connected to our material

	std::vector<VOP_Node*> bind_nodes;
	/* A traversal stack to avoid recursion */
	std::vector<OP_Node*> traversal;

	traversal.push_back( m_vop );

	while( traversal.size() )
	{
		OP_Node* node = traversal.back();
		traversal.pop_back();

		int ninputs = node->nInputs();
		for( int i = 0; i < ninputs; i++ )
		{
			OP_Input *input_ref = node->getInputReferenceConst(i);
			if( !input_ref )
			{
				continue;
			}

			VOP_Node *output = CAST_VOPNODE( node->getInput(i) );
			if( !output )
			{
				continue;
			}

			if( is_aov_definition(output) )
			{
				bind_nodes.push_back( output );
			}
			else
			{
				traversal.push_back( output );
			}
		}
	}

	// Third, add and connect our aov group to deal with bind export
	if (bind_nodes.size() > 0)
	{
		std::string handle = m_handle;
		handle += "|dlAOVGroup|shader";

		const shader_library& library = shader_library::get_instance();
		std::string path = library.get_shader_path("dlAOVGroup");
		assert( path.length() > 0 );

		m_nsi.Create( handle, "shader" );
		m_nsi.SetAttribute( handle,
			NSI::CStringPArg( "shaderfilename", path.c_str()) );
		
		m_nsi.Connect( handle, "outColor", m_handle, "aovGroup" );

		std::vector<std::string> aov_names;
		std::vector<float> aov_values;

		for ( unsigned i = 0; i < bind_nodes.size(); i++ )
		{
			UT_String aov_name;
			bind_nodes[i]->evalString( aov_name, "parmname", 0, 0.0f );
			aov_names.push_back( aov_name.toStdString() );
			// Arbitrary values since the connection overrides it
			aov_values.push_back( 0.0f );
			aov_values.push_back( 0.0f );
			aov_values.push_back( 0.0f );
		}

		NSI::ArgumentList list;

		list.Add( NSI::Argument::New( "colorAOVNames" )
			->SetArrayType( NSITypeString, aov_names.size() )
			->CopyValue(&aov_names[0], aov_names.size()*sizeof(aov_names[0])));

		list.Add( NSI::Argument::New( "colorAOVValues" )
			->SetArrayType( NSITypeColor, aov_values.size() / 3 )
			->CopyValue(&aov_values[0], aov_values.size()*sizeof(aov_values[0])));

		m_nsi.SetAttributeAtTime( handle, 0, list );

		for ( unsigned i = 0; i < bind_nodes.size(); i++ )
		{
			UT_String aov_value;
			aov_value.sprintf("colorAOVValues[%u]", i);
			m_nsi.Connect(
				bind_nodes[i]->getFullPath().c_str(), "outColor",
				handle, aov_value.c_str() );
		}
	}
}

void vop::list_ramp_parameters(
	const OP_Parameters* i_opp,
	const DlShaderInfo::Parameter& i_param,
	osl_utilities::ramp::eType i_type,
	float i_time,
	NSI::ArgumentList& o_list)
{
	using namespace osl_utilities::ramp;

	// Remove the value suffix from the variable name
	const std::string& value_suffix = GetValueSuffix(i_type);
	std::string root_name = RemoveSuffix(i_param.name.string(), value_suffix);

	// Create the names of the other shader parameters
	const std::string& position_suffix = GetPositionSuffix(i_type);
	std::string pos_string = root_name + position_suffix;
	std::string value_string = root_name + value_suffix;
	std::string inter_string = root_name + k_interpolation_suffix;

	/*
		Re-create arrays of values, since Houdini's "arrays" are actually just
		numbered parameters.
	*/
	bool color = IsColor(i_type);
	int num_points = i_opp->evalInt(root_name.c_str(), 0, i_time);
	std::vector<float> positions;
	std::vector<float> values;
	std::vector<int> interpolations;
	char* index_buffer = new char[k_index_format.length() + 10];
	for(int p = 0; p < num_points; p++)
	{
		sprintf(index_buffer, k_index_format.c_str(), p);

		std::string pos_item = pos_string + index_buffer;
		positions.push_back(i_opp->evalFloat(pos_item.c_str(), 0, i_time));

		std::string value_item = value_string + index_buffer;
		values.push_back(i_opp->evalFloat(value_item.c_str(), 0, i_time));
		if(color)
		{
			values.push_back(i_opp->evalFloat(value_item.c_str(), 1, i_time));
			values.push_back(i_opp->evalFloat(value_item.c_str(), 2, i_time));
		}

		std::string inter_item = inter_string + index_buffer;
		int inter = i_opp->evalInt(inter_item.c_str(), 0, i_time);

		// Try remapping Houdini's interpolation mode to Maya's
		switch(inter)
		{
			case PRM_RAMP_INTERP_CONSTANT:
				// None
				inter = 0;
				break;
			case PRM_RAMP_INTERP_LINEAR:
				// Linear
				inter = 1;
				break;
			case PRM_RAMP_INTERP_MONOTONECUBIC:
				// Smooth
				inter = 2;
				break;
			case PRM_RAMP_INTERP_CATMULLROM:
			case PRM_RAMP_INTERP_BEZIER:
			case PRM_RAMP_INTERP_BSPLINE:
			case PRM_RAMP_INTERP_HERMITE:
				// Spline
				inter = 3;
				break;
			default:
				assert(false);
				inter = 1;
		}

		interpolations.push_back(inter);
	}
	delete[] index_buffer;

	// Add new arguments to the list
	o_list.Add(NSI::Argument::New(pos_string)
		->SetArrayType(NSITypeFloat, num_points)
		->CopyValue(&positions[0], positions.size()*sizeof(positions[0])));
	o_list.Add(NSI::Argument::New(value_string)
		->SetArrayType(color ? NSITypeColor : NSITypeFloat, num_points)
		->CopyValue(&values[0], values.size()*sizeof(values[0])));
	o_list.Add(NSI::Argument::New(inter_string)
		->SetArrayType(NSITypeInteger, num_points)
		->CopyValue(
			&interpolations[0],
			interpolations.size()*sizeof(interpolations[0])));
}

/**
	\brief returns true if a given vop defines an AOV.

	For now, wee recogznie Mantra's "bind export" node as a valid
	AOV definition but we should really support dlAOVColor too
	(FIXME)
*/
bool vop::is_aov_definition( VOP_Node *i_vop )
{
	if( !i_vop )
		return false;

	OP_Operator* op = i_vop->getOperator();

	if( op && op->getName().toStdString() == "bind" )
	{
		bool use_export = i_vop->evalInt("useasparmdefiner", 0, 0.0f);
		int export_mode = i_vop->evalInt("exportparm", 0, 0.0f);
		// 0 means "Never" for export_mode
		return use_export && export_mode != 0;
	}

	return false;
}
