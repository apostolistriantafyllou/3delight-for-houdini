#include "incandescence_light.h"

#include "context.h"
#include "geometry.h"
#include "shader_library.h"
#include "vop.h"

#include <OBJ/OBJ_Node.h>
#include <OP/OP_Director.h>
#include <UT/UT_String.h>
#include <UT/UT_WorkArgs.h>
#include <VOP/VOP_Node.h>
#include <GA/GA_AIFSharedStringArray.h>
#include <GA/GA_Attribute.h>
#include <GU/GU_Detail.h>

#include <assert.h>
#include <nsi.hpp>

#include <algorithm>

const char* k_incandescence_multiplier = "incandescence_multiplier";
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
}

/**
	We do not create anything for the incandescence light. We will be using
	the parent's handle, which is a null, to connect to.
*/
void incandescence_light::create() const
{
	/*
		This light is  really just a set to connect objects to. We do not
		use the object handle here as in IPR this is a meaningless name
		(uuid) and it also creates problem down the road.
		\ref ROP_3Delight::ExportLightCategories
	*/
	std::string category = m_object->getFullPath().toStdString();
	m_nsi.Create( category, "set" );
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
	std::string category = m_object->getFullPath().toStdString();

	// Process each obj_path in the selection
	for(int i = 0; i < obj_paths.getArgc(); i++)
	{
		OBJ_Node* obj_node = OPgetDirector()->findOBJNode(obj_paths(i));
		if (!obj_node)
		{
			// Case relative path
			obj_node = m_object->findOBJNode(obj_paths(i));
		}

		if (!obj_node)
			continue;

		std::vector< std::string > material_paths;

		OP_Context op_ctx( time );
		GU_DetailHandle gdh = obj_node->getRenderGeometryHandle(op_ctx);
		GU_DetailHandleAutoReadLock rlock(gdh);
		const GU_Detail *gdp = rlock.getGdp();

		if( gdp )
		{
			const GA_Attribute *sop[2] =
			{
				gdp->findAttribute(GA_ATTRIB_PRIMITIVE, "shop_materialpath"),
				gdp->findAttribute(GA_ATTRIB_DETAIL, "shop_materialpath")
			};

			int counts[2] = { (int)gdp->getNumPrimitives(), 1 };

			for (int i = 0; i < sizeof(sop) / sizeof(sop[0]); i++)
			{
				if( !sop[i] )
					continue;

				const GA_AIFStringTuple *strings = sop[i]->getAIFStringTuple();

				if (!strings)
					continue;

				for (int j = 0; j < counts[i]; j++)
				{
					const char *mat = strings->getString(sop[i], j);
					if (!mat)
						continue;

					material_paths.push_back(mat);
				}
			}
		}

		UT_String obj_mat_path;
		obj_node->evalString(obj_mat_path, "shop_materialpath", 0, time);
		material_paths.push_back( obj_mat_path.toStdString() );

		auto end = std::unique( material_paths.begin(), material_paths.end() );
		auto current = material_paths.begin();

		for( ; current != end; current ++ )
		{
			auto mat_path = *current;
			VOP_Node *mats[3] = {nullptr};
			resolve_material_path(obj_node, mat_path.c_str(), mats);

			VOP_Node *surface = mats[0];
			if (!surface)
				continue;

			DlShaderInfo *shader_info = library.get_shader_info(surface);
			if (!shader_info)
				continue;

			// Check if incandescence_multiplier exists in this shader
			if (ParameterExist(shader_info, k_incandescence_multiplier) && m_context.object_displayed(*obj_node))
			{
				m_nsi.SetAttribute(
					vop::handle(*surface, m_context),
					NSI::ColorArg(k_incandescence_multiplier,
								  incandescenceColor));

				m_current_multipliers.push_back(vop::handle(*surface, m_context));

				m_nsi.Connect(
					geometry::handle(*obj_node, m_context), "",
					category, "members");
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

	if(name == "light_color" || name == "light_intensity" ||
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

void incandescence_light::Delete(OBJ_Node& i_node, const context& i_context)
{
	i_context.m_nsi.Delete(handle(i_node, i_context));
}

void incandescence_light::disconnect()const
{
	/*
		FIXME : will always be false since m_current_multipliers is only filled
		by connect(), which is never called before disconnect(). (Exporters are
		not kept during IPR rendering : disconnect() is only ever called on a
		local exporter from changed_cb()).
	*/
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
				handle,
				NSI::ColorArg(k_incandescence_multiplier, white));
		}

		/* This will take care of the Multi-Light output */
		// FIXME : we connect to "members" but we disconnect from "objects"
		m_nsi.Disconnect( NSI_ALL_NODES, "", handle().c_str(), "objects" );

		m_current_multipliers.clear();
	}
}

void incandescence_light::compute_output_color(float o_color[]) const
{
	fpreal time = m_context.m_current_time;

	fpreal color_r = m_object->evalFloat( "light_color", 0, time );
	fpreal color_g = m_object->evalFloat( "light_color", 1, time );
	fpreal color_b = m_object->evalFloat( "light_color", 2, time );

	fpreal intensity = m_object->evalFloat( "light_intensity", 0, time );
	fpreal exposure = m_object->evalFloat( "exposure", 0, time );

	o_color[0] = color_r * intensity * exp2f(exposure);
	o_color[1] = color_g * intensity * exp2f(exposure);
	o_color[2] = color_b * intensity * exp2f(exposure);
}
