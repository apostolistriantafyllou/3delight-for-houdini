#include "incandescence_light.h"

#include "context.h"
#include "shader_library.h"

#include <OBJ/OBJ_Node.h>
#include <OP/OP_Director.h>
#include <UT/UT_String.h>
#include <UT/UT_WorkArgs.h>
#include <VOP/VOP_Node.h>

#include <assert.h>
#include <nsi.hpp>

#define NSI_LIGHT_PRIORITY 1

namespace
{
	/**
		Returns true if param_name exists into the specified i_shader_info.
	*/
	bool ParameterExist(DlShaderInfo* i_shader_info, const char* param_name)
	{
		for (int i = 0; i < i_shader_info->nparams(); i++)
		{
			const DlShaderInfo::Parameter* parameter = i_shader_info->getparam(i);
			if (parameter->name == param_name)
			{
				return true;
			}
		}
		return false;
	}
}

incandescence_light::incandescence_light(
	const context& i_ctx, OBJ_Node* i_object )
:
	exporter( i_ctx, i_object )
{
	m_handle += "|incandescence_light";
}

void incandescence_light::create() const
{
	/* Needed for multi-light layer */
	std::string set = set_handle();
	m_nsi.Create( set, "set" );

	/*
		Create the attribute node so that we can connect the surface
		shader used for the light source.
	*/
	std::string attributes = attributes_handle();
	m_nsi.Create( attributes, "attributes" );

	m_nsi.Connect( attributes, "",	set, "geometryattributes" );

	/*
		All our lights are created invisible to all ray types. Their membership
		status in the defaultLightSet will override this visibility. The case
		of the incandescent light is different as it doesn't have "camera"
		visibility.
	*/
	m_nsi.SetAttribute( attributes,
		(
			NSI::IntegerArg("visibility.diffuse", 0),
			NSI::IntegerArg("visibility.diffuse.priority", NSI_LIGHT_PRIORITY),
			NSI::IntegerArg("visibility.specular", 0),
			NSI::IntegerArg("visibility.specular.priority", NSI_LIGHT_PRIORITY),
			NSI::IntegerArg("visibility.reflection", 0),
			NSI::IntegerArg("visibility.reflection.priority", NSI_LIGHT_PRIORITY),
			NSI::IntegerArg("visibility.refraction", 0),
			NSI::IntegerArg("visibility.refraction.priority", NSI_LIGHT_PRIORITY),
			NSI::IntegerArg("visibility.shadow", 0),
			NSI::IntegerArg("visibility.shadow.priority", NSI_LIGHT_PRIORITY)
		) );
}

void incandescence_light::set_attributes() const
{
}

void incandescence_light::connect() const
{
	float incandescenceColor[3];
	compute_output_color(incandescenceColor);

	fpreal time = m_context.m_current_time;
	UT_String geo_str;
	m_object->evalString(geo_str, "object_selection", 0, time);

	UT_WorkArgs obj_paths;
	geo_str.tokenize(obj_paths, ' ');

	const shader_library& library = shader_library::get_instance();
	const char* incandescence_multiplier = "incandescence_multiplier";

	// Process each obj_path in the selection
	for(int i = 0; i < obj_paths.getArgc(); i++)
	{
		OBJ_Node* obj_node = OPgetDirector()->findOBJNode(obj_paths(i));
		if (!obj_node)
		{
			// Case relative path
			obj_node = m_object->findOBJNode(obj_paths(i));
		}
		if (obj_node)
		{
			UT_String mat_path;
			obj_node->evalString(mat_path, "shop_materialpath", 0, time);

			VOP_Node* vop_node = OPgetDirector()->findVOPNode(mat_path);
			// Do we got a material node?
			if (vop_node)
			{
				OP_Operator* op = vop_node->getOperator();
				std::string path =
					library.get_shader_path(op->getName().c_str());

				DlShaderInfo* shader_info =
					library.get_shader_info( path.c_str() );
				if (shader_info)
				{
					// Check if incandescence_multiplier exists in this shader
					if (ParameterExist(shader_info, incandescence_multiplier))
					{
						m_nsi.SetAttribute(
							vop_node->getFullPath().toStdString(),
							NSI::ColorArg(incandescence_multiplier,
								incandescenceColor));

						m_current_multipliers.push_back(
							vop_node->getFullPath().toStdString() );

						m_nsi.Connect( obj_node->getFullPath().c_str(), "",
										set_handle().c_str(), "objects" );
					}
				}
			}
		}
	}
}

/**
	\brief Callback that should be connected to an OP_Node that has an
	associated light exporter.
*/
void incandescence_light::changed_cb(
	OP_Node* i_caller,
	void* i_callee,
	OP_EventType i_type,
	void* i_data)
{
	context* ctx = (context*)i_callee;
	if(i_type != OP_PARM_CHANGED)
	{
		return;
	}

	intptr_t parm_index = reinterpret_cast<intptr_t>(i_data);
	incandescence_light node(*ctx, i_caller->castToOBJNode());

	PRM_Parm& parm = node.m_object->getParm(parm_index);
	std::string name = parm.getToken();

	if(name == "color" || name == "intensity" ||
		name == "exposure" || name == "object_selection")
	{
		node.disconnect();
		// Will connect only if required
		node.connect();
	}
	else
	{
		return;
	}

	ctx->m_nsi.RenderControl(NSI::CStringPArg("action", "synchronize"));
}

void incandescence_light::disconnect()const
{
	if( !m_current_multipliers.empty() )
	{
		/*
			Before setting a new state we need to reset the current state
			to a neutral position. This means:

			1- All multipliers are 1.
			2- The lightset is empty.
		*/

		/* This will take care of shader parameter "incandescence_multiplier" */
		float white[3] = { 1.f, 1.f, 1.f };

		for( const std::string& handle : m_current_multipliers )
		{
			m_nsi.SetAttribute(
				handle.c_str(),
				NSI::ColorArg("incandescence_multiplier", white));
		}

		/* This will take care of the Multi-Light output */
		m_nsi.Disconnect( NSI_ALL_NODES, "", set_handle().c_str(), "objects" );

		m_current_multipliers.clear();
	}
}

void incandescence_light::compute_output_color(float o_color[]) const
{
	fpreal time = m_context.m_current_time;

	fpreal color_r = m_object->evalFloat( "color", 0, time );
	fpreal color_g = m_object->evalFloat( "color", 1, time );
	fpreal color_b = m_object->evalFloat( "color", 2, time );

	fpreal intensity = m_object->evalFloat( "intensity", 0, time );
	fpreal exposure = m_object->evalFloat( "exposure", 0, time );

	o_color[0] = color_r * intensity * exp2f(exposure);
	o_color[1] = color_g * intensity * exp2f(exposure);
	o_color[2] = color_b * intensity * exp2f(exposure);
}
