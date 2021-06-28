#include "vop.h"
#include "cop_utilities.h"
#include "context.h"
#include "osl_utilities.h"
#include "3Delight/ShaderQuery.h"
#include "scene.h"
#include "shader_library.h"
#include "geometry.h"

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
	std::string path = shader_path( m_vop );
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
		nullptr,
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
	
	//There can be more than one node connected to each other that are bypassed.
	while(source->getBypass())
	{
		UT_String bypass_node_output;
		int source_index = input_ref->getNodeOutputIndex();
		source->getOutputName(bypass_node_output, source_index);

		VOP_OutputInfoManager* vop_info = source->getOutputInfoManager();
		int idx = vop_info->findCorrespondingInputIndex(bypass_node_output,false,true);

		/*
			Skip the node that is bypassed by replacing it with the node connected to the
			it's input, which should also have pipethrough method on it's connected input.
		*/
		if (idx != -1)
		{
			input_ref = source->getInputReferenceConst(idx);
			source = CAST_VOPNODE(source->getInput(idx));
		}
		if(idx == -1 || !source)
			return;
	}

	//aov group connection is being handles on vop::add_and_connect_aov_group()
	if (!is_aov_definition(source) && input_ref)
	{
		UT_String source_name;
		int source_index = input_ref->getNodeOutputIndex();
		source->getOutputName(source_name, source_index);

		m_nsi.Connect(
			vop::handle(*source, m_context), source_name.toStdString(),
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
		else
		{
			/*
				We must connect to uvs when disconnecting from uvCoord input
				since we are disconnecting all nodes from this input once
				we delete the connection to it.
			*/
			if (input_name == "uvCoord")
			{
				ctx->m_nsi.Connect(
					"__uv_coordinates_reader", "uvs",
					v.m_handle, input_name.toStdString());
			}
		}

		ctx->m_nsi.RenderControl(NSI::CStringPArg("action", "synchronize"));
	}
	else if (i_type == OP_FLAG_CHANGED)
	{
		VOP_Node* flag_changed_material = i_caller->castToVOPNode();
		if (!flag_changed_material)
			return;

		vop v(*ctx, i_caller->castToVOPNode());
		if (!flag_changed_material->getDebug())
		{
			//Disconnect the node whose flag is being changed.
			ctx->m_nsi.Disconnect(
				v.m_handle, NSI_ALL_ATTRIBUTES,
				NSI_ALL_NODES, NSI_ALL_NODES);

			UT_Array<OP_Node*> outputs;
			flag_changed_material->getOutputNodes(outputs);

			/*
				connect the changed node as a source vop so we can
				check if it's bypassed or no in the connect() function.
				Don't connect vop nodes that are not assigned to any obj
				node as they are not created at all through nsi.
			*/
			for (int i = 0; i < outputs.size(); i++)
			{
				if (!ctx->material_to_objects[outputs[i]->castToVOPNode()].empty())
				{
					vop k(*ctx, outputs[i]->castToVOPNode());
					k.connect();
				}
			}
		}

		//Re-export only the geometries to where this material is connected.
		for (std::string obj_path : ctx->material_to_objects[flag_changed_material])
		{
			OBJ_Node* node = OPgetDirector()->getOBJNode(obj_path.c_str());
			if (node)
			{
				geometry::re_export(*ctx, *node);
			}
		}

		ctx->m_nsi.RenderControl(NSI::CStringPArg("action", "synchronize"));
	}
}

/**
	\brief Fill a list of NSI arguments from an OP_Parameters node.

	The only gotcha here is regarding texture parameters: we detect
	them by checking for the srccolorspace or texcolorspace attribute.
	Which means it's a "Texture" node.
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
	assert( i_parameters );

	const shader_library &library = shader_library::get_instance();
	std::string path;
	if(i_shader)
	{
		path = library.get_shader_path( i_shader );
	} else {
		const VOP_Node *vop = dynamic_cast<const VOP_Node*>(
				i_parameters );
		assert( vop );
		path = shader_path( vop );
	}
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
		if( parameter->type.arraylen == 2)
		{
			/*
				We don't define uvCoord as a parameter, so instead we
				check input connection with this name.
			*/
			int input_index =
				i_parameters->getInputFromName(parameter->name.c_str());
			if (i_parameters->castToVOPNode()
				->findSimpleInput(input_index) == nullptr)
			{
				/* Check for default uv coordintes */
				const char* default_connection = nullptr;
				osl_utilities::FindMetaData(
					default_connection,
					parameter->metadata,
					"default_connection");

				if (default_connection &&
					::strcmp("uvCoord", default_connection) == 0)
				{
					o_uv_connection = parameter->name.c_str();
				}
			}
		}


		if( index < 0 || (i_parm_index >= 0 && index != i_parm_index))
			continue;

		switch( parameter->type.elementtype )
		{
		case NSITypeFloat:
			/*
				Insert as an array if arraylen is greater then 0.
				Happens when the current parameter of the OSL
				shader is a float[] value.
			*/
			if (parameter->type.arraylen > 0)
			{
				std::vector<float> values;
				for (int j = 0; j < parameter->type.arraylen; j++)
					values.push_back(i_parameters->evalFloat(index, j, i_time));
				o_list.Add(NSI::Argument::New(parameter->name.c_str())
					->SetArrayType(NSITypeFloat, parameter->type.arraylen)
					->CopyValue(&values[0], values.size() * sizeof(values[0])));
			}
			else
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
			/*
				Insert as an array if arraylen is greater then 0.
				Happens when the current parameter of the OSL
				shader is an int[] value.
			*/
			if (parameter->type.arraylen > 0)
			{
				std::vector<int> values;
				for (int j = 0; j < parameter->type.arraylen; j++)
					values.push_back(i_parameters->evalInt(index, j, i_time));
				o_list.Add(NSI::Argument::New(parameter->name.c_str())
					->SetArrayType(NSITypeInteger, parameter->type.arraylen)
					->CopyValue(&values[0], values.size() * sizeof(values[0])));
			}
			else
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

			if( color_space.length() != 0 )
			{
				/*
					This is a workaround for houdini's texture node which does
					not have the "right" color space parameter like ours.
				*/
				std::string param( parameter->name.c_str() );
				param += ".meta.colorspace";
				o_list.Add( new NSI::StringArg(param, color_space.buffer()) );
			}

			break;
		}
		}
	}
}

std::string vop::vop_name( const VOP_Node *i_vop )
{
	return i_vop->getOperator()->getName().toStdString();
}


bool vop::ignore_subnetworks( void ) const
{
	const shader_library &library = shader_library::get_instance();
	std::string path = shader_path( m_vop );
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

void vop::add_and_connect_aov_group() const
{
	/*
		First, check if this node represents a final surface such
		as DlPrincipled or DlMetal. These surfaces have a special
		aovGroup input to which we can connect AOV nodes.
	*/
	const shader_library &library = shader_library::get_instance();
	std::string path = shader_path( m_vop );
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
			//Get Values of all the AOV Colors from the dlAOVGroup
			for (int j = 0; j < aov_export_nodes[i]->getNumVisibleInputs(); j++)
			{
				UT_String aov_name = aov_export_nodes[i]->inputLabel(j);
				char* aov_name_buffer = new char[aov_name.toStdString().size() + 1];
				std::strncpy(aov_name_buffer, aov_name.toStdString().c_str(), aov_name.toStdString().size() + 1);
				aov_names.push_back(aov_name_buffer);

				aov_values.push_back(0.0f);
				aov_values.push_back(0.0f);
				aov_values.push_back(0.0f);
			}
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
			for (int j = 0; j < aov_export_nodes[i]->getNumVisibleInputs(); j++)
			{
				VOP_Node* current_vop = aov_export_nodes[i];

				OP_Input* input_ref = current_vop->getInputReferenceConst(j);
				if (!input_ref)
					continue;

				VOP_Node* source = CAST_VOPNODE(current_vop->getInput(j));
				if (!source)
					continue;

				UT_String source_output_name;
				int source_index = input_ref->getNodeOutputIndex();
				source->getOutputName(source_output_name, source_index);

				UT_String aov_value;
				aov_value.sprintf("colorAOVValues[%u]", j);
				m_nsi.Connect(
					vop::handle(*source, m_context), source_output_name.toStdString(),
					handle, aov_value.toStdString());
			}
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
		nullptr,
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
	\brief returns true if a given vop defines an AOV Group.
*/
bool vop::is_aov_definition( VOP_Node *i_vop )
{
	if( !i_vop )
		return false;

	OP_Operator* op = i_vop->getOperator();

	if( op && op->getName().toStdString() == "dlAOVGroup" )
	{
		return true;
	}
	return false;
}


bool vop::is_renderable( VOP_Node *i_vop )
{
	return !shader_path( i_vop ).empty();
}

std::string vop::shader_path( const VOP_Node *i_vop )
{
	std::string path;

	UT_StringHolder shaderParmName("_3dl_osl_shader");

	if( i_vop->hasParm( shaderParmName ) )
	{
		UT_String shaderfilename;
		i_vop->evalString( shaderfilename, shaderParmName, 0, 0 );
		path = shaderfilename;
	}

	else
	{
		const shader_library &library = shader_library::get_instance();
		path = library.get_shader_path(vop_name(i_vop).c_str());
	}

	return path;
}

bool vop::is_texture( VOP_Node *vop )
{
	/* texture/pattern is marked as other */
	return shader_type( vop ) == osl_type::e_other;
}

vop::osl_type vop::shader_type( VOP_Node *shader )
{
	std::string mat_path = vop::shader_path(shader);
	const shader_library& library = shader_library::get_instance();

	DlShaderInfo* shader_info = library.get_shader_info(mat_path.c_str());

	if( !shader_info )
		return osl_type::e_unknown;

	std::vector< std::string > shader_tags;
	osl_utilities::get_shader_tags(*shader_info, shader_tags);

	//Check if the attached material is a texture shader or not.
	for (const auto& tag : shader_tags)
	{
		if (tag == "texture/2d" || tag == "texture/3d" || tag == "utility" )
		{
			return osl_type::e_other;
		}

		if( tag == "surface" )
			return osl_type::e_surface;

		if( tag == "displacement" )
			return osl_type::e_displacement;

		if( tag == "volume" )
			return osl_type::e_volume;
	}

	return osl_type::e_unknown;
}


