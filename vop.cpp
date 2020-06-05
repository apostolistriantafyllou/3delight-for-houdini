#include "vop.h"
#include "cop_utilities.h"
#include "context.h"
#include "osl_utilities.h"
#include "3Delight/ShaderQuery.h"
#include "scene.h"
#include "shader_library.h"

#include <VOP/VOP_Node.h>
#include <OP/OP_Input.h>

#include <assert.h>
#include <nsi.hpp>
#include <algorithm>
#include <iostream>
#include <unordered_set>

vop::vop(
	const context& i_ctx,
	VOP_Node *i_vop )
:
	exporter( i_ctx, i_vop )
{
	assert(is_renderable(i_vop));
}

void vop::create( void ) const
{
	const shader_library &library = shader_library::get_instance();
	std::string path = library.get_shader_path( vop_name().c_str() );
	assert( !path.empty() );

	m_nsi.Create( m_handle, "shader" );
	m_nsi.SetAttribute( m_handle,
		NSI::CStringPArg( "shaderfilename", path.c_str()) );
}

void vop::set_attributes( void ) const
{
	set_attributes_at_time(m_context.m_current_time);
}

void vop::set_attributes_at_time( double i_time ) const
{
	NSI::ArgumentList list;
	std::string uv_coord_connection;

	list_shader_parameters(
		m_context,
		m_vop,
		m_vop->getOperator()->getName(),
		i_time,
		-1,
		list, uv_coord_connection );

	if( !list.empty() )
	{
		m_nsi.SetAttributeAtTime( m_handle, i_time, list );
	}

	if( !uv_coord_connection.empty() )
	{
		const shader_library &library = shader_library::get_instance();
		std::string uv_shader( library.get_shader_path( "uv_reader") );

		m_nsi.Create( "__uv_coordinates_reader", "shader" );
		m_nsi.SetAttribute( "__uv_coordinates_reader",
			NSI::StringArg( "shaderfilename", uv_shader) );

		m_nsi.Connect(
			"__uv_coordinates_reader", "uvs",
			m_handle, uv_coord_connection );
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
*/
void vop::connect( void ) const
{
	if( ignore_subnetworks() )
		return;

	for( int i = 0, n = m_vop->nInputs(); i<n; ++i )
	{
		connect_input(i);
	}

	// If needed, add and connect our aov group to our material
	add_and_connect_aov_group();
}		

void vop::connect_input(int i_input_index)const
{
	OP_Input *input_ref = m_vop->getInputReferenceConst(i_input_index);

	if( !input_ref )
		return;

	VOP_Node *source = CAST_VOPNODE( m_vop->getInput(i_input_index) );

	if( !source )
		return;

	UT_String input_name;
	m_vop->getInputName(input_name, i_input_index);
	
	if ( is_aov_definition(source) )
	{
		/*
			Special case for 'bind' export node: if we use source_name,
			this will be unrecognized by NSI since source_name changes
			each time user specifies a new name for its export (custom)
			name.
		*/
		m_nsi.Connect(
			source->getFullPath().toStdString(), "outColor",
			m_handle, input_name.toStdString() );
	}
	else
	{
		UT_String source_name;
		int source_index = input_ref->getNodeOutputIndex();
		source->getOutputName(source_name, source_index);

		m_nsi.Connect(
			source->getFullPath().toStdString(), source_name.toStdString(),
			m_handle, input_name.toStdString() );
	}
}

/**
	\brief Callback that should be connected to an OP_Node that has an
	associated vop exporter.
*/
void vop::changed_cb(
	OP_Node* i_caller,
	void* i_callee,
	OP_EventType i_type,
	void* i_data)
{
	context* ctx = (context*)i_callee;
	if(i_type == OP_PARM_CHANGED)
	{
		intptr_t parm_index = reinterpret_cast<intptr_t>(i_data);

		NSI::ArgumentList list;
		if(vop(*ctx, i_caller->castToVOPNode()).set_single_attribute(parm_index))
		{
			ctx->m_nsi.RenderControl(NSI::CStringPArg("action", "synchronize"));
		}
	}
	else if(i_type == OP_INPUT_REWIRED)
	{
		intptr_t input_index = reinterpret_cast<intptr_t>(i_data);
		UT_String input_name;
		i_caller->getInputName(input_name, input_index);

		vop v(*ctx, i_caller->castToVOPNode());

		// Disconnect the previous source node from the input parameter
		ctx->m_nsi.Disconnect(
			NSI_ALL_NODES, NSI_ALL_ATTRIBUTES,
			v.m_handle, input_name.toStdString());

		OP_Node* source = i_caller->getInput(input_index);
		if(source)
		{
			/*
				Export the source node and all its sub-network, in case it
				hadn't been exported yet.
			*/
			std::unordered_set<std::string> materials;
			materials.insert(source->getFullPath().toStdString());
			scene::export_materials(materials, *ctx);

			// Connect the source node to the input parameter
			v.connect_input(input_index);
		}

		ctx->m_nsi.RenderControl(NSI::CStringPArg("action", "synchronize"));
	}
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
	const context &i_context,
	const OP_Node *i_parameters,
	const char *i_shader,
	float i_time,
	int i_parm_index,
	NSI::ArgumentList &o_list,
	std::string &o_uv_connection )
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
		if(osl_utilities::ramp::IsRampWidget(widget))
		{
			list_ramp_parameters(
				i_parameters,
				*shader_info,
				*parameter,
				i_time,
				o_list);
			continue;
		}

		int index = i_parameters->getParmIndex( parameter->name.c_str() );

		/*
			Special check for uv coordinates.

			We are looking for something like float uv[2] that
			is not connected to anything.
		*/
		if( parameter->type.arraylen == 2 &&
			i_parameters->getInput(index) == nullptr  )
		{
			/* Check for default uv coordintes */
			const char *default_connection = nullptr;
			osl_utilities::FindMetaData(
				default_connection,
				parameter->metadata,
				"default_connection" );

			if( default_connection &&
				::strcmp("uvCoord", default_connection) == 0 )
			{
				o_uv_connection = parameter->name.c_str();
			}
		}


		if( index < 0 || (i_parm_index >= 0 && index != i_parm_index))
			continue;

		switch( parameter->type.elementtype )
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

			std::string potential_cop( str.toStdString() );
			if( potential_cop.find("op:", 0) == 0 )
			{
				str = cop_utilities::convert_to_texture(
					i_context,
					potential_cop.substr(3) /* remove ":op" prefix */ );

				if( str.length() == 0 )
				{
					/*
						Something is wrong or we are doing stdout NSI export,
						put back original string.
					*/
					str = potential_cop;
				}
			}

			/*
				3Delight nows about UDIM, but Houdini users prefere <UDIM>. Note
				UT_String class seems to provide changeWord and changeString but
				I amnot going to use a method that has zero  comments and no
				docs, so std::string it is.
			*/
			std::string stdstr( str );
			size_t start_pos = stdstr.find( "<UDIM>" );
			if( start_pos != std::string::npos )
				stdstr.replace( start_pos, 6, "UDIM" );

			o_list.Add( new NSI::StringArg(parameter->name.c_str(), stdstr) );

			if( isTextureNode || is_texture_path( parameter->name.c_str() ) )
			{
				std::string param( parameter->name.c_str() );
				param += ".meta.colorspace";
				/*
					If the value of the current texture parameter is not detected 
					by checking for srccolorspace or texcolorspace attribute we do 
					an additional check by checking its parameter name concantenated 
					with .meta.colorspace. This is because when we add the color 
					space attribute on Houdini for texture nodes we set the colorspace
					parameter name to texture_param_name+".meta.colorspace"
				*/
				colorspace_index = i_parameters->getParmIndex(param.c_str());
				if (colorspace_index >= 0)
				{
					/*
						Here we update the value of color_space if the colorspace 
						attribute named after the texture node parameter exists.
					*/
					i_parameters->evalString(color_space, param.c_str(), 0, i_time);
				}

				o_list.Add( new NSI::StringArg(param, color_space.buffer()) );			}

			break;
		}
		}
	}
}

std::string vop::vop_name( void ) const
{
	return m_vop->getOperator()->getName().toStdString();
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
		if( P.name != "tags" || !P.type.IsStringArray() )
			continue;

		for( auto &tag : P.sdefault )
		{
			if( tag == "ignoresubnetworks" )
				return true;
		}
	}

	return false;
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
	/*
		First, check if this node represents a final surface such
		as DlPrincipled or DlMetal. These surfaces have a special
		aovGroup input to which we can connect AOV nodes.
	*/
	const shader_library &library = shader_library::get_instance();
	std::string path = library.get_shader_path( vop_name().c_str() );
	assert( !path.empty() );

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

	/*
		Second, check if any aov node is connected to this material
		and accumulate it into a list.
	*/
	std::vector<VOP_Node*> aov_export_nodes;

	std::vector<OP_Node*> traversal; traversal.push_back( m_vop );
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
				aov_export_nodes.push_back( output );
			}
			else
			{
				traversal.push_back( output );
			}
		}
	}

	/*
		Last, add and connect our aov group to the material and connect the
		aov export nodes to it.
	*/
	if (aov_export_nodes.size() > 0)
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

		std::vector<std::string> aov_names_storage;
		std::vector<const char *> aov_names;
		std::vector<float> aov_values;

		for ( unsigned i = 0; i < aov_export_nodes.size(); i++ )
		{
			UT_String aov_name;
			aov_export_nodes[i]->evalString( aov_name, "parmname", 0, 0.0f );
			aov_names_storage.push_back( aov_name.toStdString() );
			aov_names.push_back( aov_names_storage.back().c_str() );

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

		m_nsi.SetAttribute( handle, list );

		for ( unsigned i = 0; i < aov_export_nodes.size(); i++ )
		{
			UT_String aov_value;
			aov_value.sprintf("colorAOVValues[%u]", i);
			m_nsi.Connect(
				aov_export_nodes[i]->getFullPath().c_str(), "outColor",
				handle, aov_value.c_str() );
		}
	}
}

bool vop::set_single_attribute(int i_parm_index)const
{
	NSI::ArgumentList list;
	std::string dummy;

	list_shader_parameters(
		m_context,
		m_vop,
		m_vop->getOperator()->getName(),
		m_context.m_current_time,
		i_parm_index,
		list, dummy );

	if(list.empty())
	{
		return false;
	}

	m_nsi.SetAttribute( m_handle, list );

	return true;
}


void vop::list_ramp_parameters(
	const OP_Parameters* i_opp,
	const DlShaderInfo& i_shader,
	const DlShaderInfo::Parameter& i_param,
	float i_time,
	NSI::ArgumentList& o_list)
{
	using namespace osl_utilities::ramp;

	const DlShaderInfo::Parameter* knots = nullptr;
	const DlShaderInfo::Parameter* interpolation = nullptr;
	const DlShaderInfo::Parameter* shared_interpolation = nullptr;
	std::string base_name;
	if(!FindMatchingRampParameters(
			i_shader,
			i_param,
			knots,
			interpolation,
			shared_interpolation,
			base_name))
	{
		return;
	}

	// Create the names of the other shader parameters
	std::string pos_string = knots->name.string();
	std::string value_string = i_param.name.string();
	std::string inter_string = interpolation->name.string();

	/*
		Re-create arrays of values, since Houdini's "arrays" are actually just
		numbered parameters.
	*/
	int num_points = i_opp->evalInt(base_name.c_str(), 0, i_time);
	std::vector<float> positions;
	std::vector<float> values;
	std::vector<int> interpolations;
	for(int p = 0; p < num_points; p++)
	{
		std::string index = ExpandedIndexSuffix(p);

		std::string pos_item = pos_string + index;
		positions.push_back(i_opp->evalFloat(pos_item.c_str(), 0, i_time));

		std::string value_item = value_string + index;
		unsigned nc = i_param.type.elementtype == NSITypeColor ? 3 : 1;
		for(unsigned c = 0; c < nc; c++)
		{
			values.push_back(i_opp->evalFloat(value_item.c_str(), c, i_time));
		}

		std::string inter_item = inter_string + index;
		int inter = i_opp->evalInt(inter_item.c_str(), 0, i_time);
		interpolations.push_back(
			FromHoudiniInterpolation((PRM_RampInterpType)inter));
	}

	// Add new arguments to the list
	o_list.Add(NSI::Argument::New(pos_string)
		->SetArrayType(NSITypeFloat, num_points)
		->CopyValue(&positions[0], positions.size()*sizeof(positions[0])));
	o_list.Add(NSI::Argument::New(value_string)
		->SetArrayType((NSIType_t)i_param.type.elementtype, num_points)
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


bool vop::is_renderable( VOP_Node *i_vop )
{
	const shader_library &library = shader_library::get_instance();
	std::string vop_name( i_vop->getOperator()->getName().toStdString() );
	std::string path = library.get_shader_path( vop_name.c_str() );

	return !path.empty();
}
