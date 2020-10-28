#include "placement_matrix.h"
#include "context.h"
#include "osl_utilities.h"
#include "3Delight/ShaderQuery.h"
#include "scene.h"
#include "shader_library.h"

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

	std::string transform_handle = m_handle + "|transform";
	m_nsi.Create(m_handle, "shader");
	m_nsi.Create(transform_handle, "transform");

	m_nsi.SetAttribute(m_handle,
		NSI::CStringPArg("shaderfilename", path.c_str()));

	//Set the space of the shader to the name of the transform.
	m_nsi.SetAttribute(m_handle,
		NSI::CStringPArg("space", transform_handle.c_str()));
}

void placement_matrix::connect(void) const
{
	std::string transform_handle = m_handle + "|transform";
	m_nsi.Connect(transform_handle, "", NSI_SCENE_ROOT, "objects");
}

void placement_matrix::set_attributes(void) const
{
	set_attributes_at_time(m_context.m_current_time);
}


void placement_matrix::set_attributes_at_time(double i_time) const
{
	UT_Matrix4D transform_mat(1.0); //Identity Matrix
	UT_String translate_param = "trans";
	UT_String scale_param = "scale";
	UT_String rotate_param = "rot";
	UT_String shear_param = "shear";
	UT_String rot_order_param = "xyz";
	UT_String transform_order_param = "trs";

	float transx = m_vop->evalFloat(translate_param, 0, i_time);
	float transy = m_vop->evalFloat(translate_param, 1, i_time);
	float transz = m_vop->evalFloat(translate_param, 2, i_time);

	float scalex = m_vop->evalFloat(scale_param, 0, i_time);
	float scaley = m_vop->evalFloat(scale_param, 1, i_time);
	float scalez = m_vop->evalFloat(scale_param, 2, i_time);

	float rotx = m_vop->evalFloat(rotate_param, 0, i_time);
	float roty = m_vop->evalFloat(rotate_param, 1, i_time);
	float rotz = m_vop->evalFloat(rotate_param, 2, i_time);

	float shearx = m_vop->evalFloat(shear_param, 0, i_time);
	float sheary = m_vop->evalFloat(shear_param, 1, i_time);
	float shearz = m_vop->evalFloat(shear_param, 2, i_time);

	int rotation_order = m_vop->evalInt(rot_order_param, 0, i_time);
	int transform_order = m_vop->evalInt(transform_order_param, 0, i_time);

	UT_XformOrder form;
	form.reorder(UT_XformOrder::rstOrder(transform_order), UT_XformOrder::xyzOrder(rotation_order));

	//Build transform matrix from the points calculated above.
	transform_mat.xform(form, transx, transy, transz,rotx, roty, rotx,
			scalex, scaley, scalez, shearx, sheary, shearz,0,0,0);
	std::string transform_handle = m_handle + "|transform";

	m_nsi.SetAttributeAtTime(transform_handle,i_time,
		NSI::DoubleMatrixArg("transformationmatrix", transform_mat.data()));
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