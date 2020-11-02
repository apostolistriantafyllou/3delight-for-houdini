#include "placement_matrix.h"
#include "context.h"
#include "vop.h"
#include "osl_utilities.h"
#include "3Delight/ShaderQuery.h"
#include "scene.h"
#include "shader_library.h"
#include <OP/OP_Input.h>

#include <VOP/VOP_Node.h>

#include <nsi.hpp>
#include <algorithm>
#include <iostream>

placement_matrix::placement_matrix(
	const context& i_ctx,
	VOP_Node* i_vop)
	:
	exporter(i_ctx, i_vop)
{
}

void placement_matrix::create(void) const
{
	const shader_library& library = shader_library::get_instance();

	//OSL shader responsible for the makexform node in Houdini
	std::string oslPlacementMatrix = "makexform";
	std::string path = library.get_shader_path(oslPlacementMatrix.c_str());

	m_nsi.Create(m_handle, "shader");

	m_nsi.SetAttribute(m_handle,
		NSI::CStringPArg("shaderfilename", path.c_str()));

}

void placement_matrix::connect(void) const
{
	for (int i = 0, n = m_vop->nInputs(); i < n; ++i)
	{
		OP_Input* input_ref = m_vop->getInputReferenceConst(i);

		if (!input_ref)
			return;

		VOP_Node* source = CAST_VOPNODE(m_vop->getInput(i));

		if (!source)
			continue;

		UT_String input_name;
		m_vop->getInputName(input_name, i);
		{
			UT_String source_name;
			int source_index = input_ref->getNodeOutputIndex();
			source->getOutputName(source_name, source_index);

			m_nsi.Connect(
				vop::handle(*source, m_context), source_name.toStdString(),
				m_handle, input_name.toStdString());
		}
	}
}

void placement_matrix::set_attributes(void) const
{
	set_attributes_at_time(m_context.m_current_time);
}


void placement_matrix::set_attributes_at_time(double i_time) const
{
	NSI::ArgumentList list;
	std::string dummy;

	vop::list_shader_parameters(
		m_context,
		m_vop,
		"makexform",
		i_time, -1, list, dummy);

	if (!list.empty())
	{
		m_nsi.SetAttributeAtTime(m_handle, i_time, list);
	}
}

void placement_matrix::changed_cb(
	OP_Node* i_caller,
	void* i_callee,
	OP_EventType i_type,
	void* i_data)
{
	context* ctx = (context*)i_callee;
	if (i_type == OP_PARM_CHANGED)
	{
		/*
			Because our parameters on tranform matrix does not have a proper OSL shader
			we can not use vop::changed_cb to set the attribute for the changed parameter
			only.
		*/
		placement_matrix v(*ctx, i_caller->castToVOPNode());
		v.set_attributes();
		ctx->m_nsi.RenderControl(NSI::CStringPArg("action", "synchronize"));
		return;
	}
	else
	{
		vop::changed_cb(i_caller, i_callee, i_type, i_data);
	}
}