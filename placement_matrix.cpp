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
	std::string oslPlacementMatrix = "place3dTexture";
	std::string path = library.get_shader_path(oslPlacementMatrix.c_str());

	std::string transform_handle = m_handle + "|transform";
	m_nsi.Create(m_handle, "shader");
	m_nsi.Create(transform_handle, "transform");

	m_nsi.SetAttribute(m_handle,
		NSI::CStringPArg("shaderfilename", path.c_str()));

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
	//Evaluate transformation matrix from the makexform node parameters.
	std::vector<double> matrix =
	{
	};

	std::string transform_handle = m_handle + "|transform";

	m_nsi.SetAttributeAtTime(transform_handle,i_time,
		NSI::DoubleMatrixArg("transformationmatrix", &matrix[0]));
}