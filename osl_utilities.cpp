#include "osl_utilities.h"

#include <assert.h>

static const std::string k_empty;
static const std::string k_ramp = "Ramp";

const std::string osl_utilities::k_null = "null";
const std::string osl_utilities::k_check_box = "checkBox";
const std::string osl_utilities::k_filename = "filename";

const std::string osl_utilities::ramp::k_index_suffix = "_#_";

namespace
{
	const std::string k_interpolation_none = "none";
	const std::string k_interpolation_constant = "constant";
	const std::string k_interpolation_linear = "linear";
	const std::string k_interpolation_smooth = "smooth";
	const std::string k_interpolation_catmullrom = "catmull-rom";
	const std::string k_interpolation_bspline = "bspline";
}

static const PRM_RampInterpType k_3delight_interpolation_to_houdini[] =
{
	PRM_RAMP_INTERP_CONSTANT,
	PRM_RAMP_INTERP_LINEAR,
	PRM_RAMP_INTERP_MONOTONECUBIC,
	PRM_RAMP_INTERP_CATMULLROM
};


void
osl_utilities::FindMetaData(
	const char*& o_value,
	const DlShaderInfo::constvector<DlShaderInfo::Parameter>& i_metadata,
	const char* i_name)
{
	for(const DlShaderInfo::Parameter& meta : i_metadata)
	{
		if(meta.name == i_name)
		{
			o_value = meta.sdefault[0].c_str();
			return;
		}
	}
}

void
osl_utilities::GetParameterMetaData(
	osl_utilities::ParameterMetaData& o_data,
	const DlShaderInfo::constvector<DlShaderInfo::Parameter>& i_metadata)
{
	for(const DlShaderInfo::Parameter& meta : i_metadata)
	{
		if(meta.name == "label")
		{
			if(!meta.sdefault.empty())
			{
				o_data.m_label = meta.sdefault[0].c_str();
			}
		}
		if(meta.name == "widget")
		{
			if(!meta.sdefault.empty())
			{
				o_data.m_widget = meta.sdefault[0].c_str();
			}
		}
		if(meta.name == "options")
		{
			if(!meta.sdefault.empty())
			{
				o_data.m_options = meta.sdefault[0].c_str();
			}
		}
		else if(meta.name == "min")
		{
			if(!meta.idefault.empty())
			{
				o_data.m_imin = &meta.idefault[0];
			}
			if(!meta.fdefault.empty())
			{
				o_data.m_fmin = &meta.fdefault[0];
			}
		}
		else if(meta.name == "max")
		{
			if(!meta.idefault.empty())
			{
				o_data.m_imax = &meta.idefault[0];
			}
			if(!meta.fdefault.empty())
			{
				o_data.m_fmax = &meta.fdefault[0];
			}
		}
		else if(meta.name == "slidermin")
		{
			if(!meta.idefault.empty())
			{
				o_data.m_slider_imin = &meta.idefault[0];
			}
			if(!meta.fdefault.empty())
			{
				o_data.m_slider_fmin = &meta.fdefault[0];
			}
		}
		else if(meta.name == "slidermax")
		{
			if(!meta.idefault.empty())
			{
				o_data.m_slider_imax = &meta.idefault[0];
			}
			if(!meta.fdefault.empty())
			{
				o_data.m_slider_fmax = &meta.fdefault[0];
			}
		}
	}
}

std::string
osl_utilities::ramp::ExpandedIndexSuffix(unsigned i_index)
{
	return "_" + std::to_string(i_index) + "_";
}

bool
osl_utilities::ramp::IsRampWidget(const char* i_widget)
{
	return i_widget && std::string(i_widget).find(k_ramp) != std::string::npos;
}

int
osl_utilities::ramp::FromHoudiniInterpolation(
	PRM_RampInterpType i_houdini_interpolation)
{
	switch(i_houdini_interpolation)
	{
		case PRM_RAMP_INTERP_CONSTANT:
			// None
			return 0;
		case PRM_RAMP_INTERP_LINEAR:
			// Linear
			return 1;
		case PRM_RAMP_INTERP_MONOTONECUBIC:
			// Smooth
			return 2;
		case PRM_RAMP_INTERP_CATMULLROM:
		case PRM_RAMP_INTERP_BEZIER:
		case PRM_RAMP_INTERP_BSPLINE:
		case PRM_RAMP_INTERP_HERMITE:
			// Spline
			return 3;
		default:
			assert(false);
			return 1;
	}
}

PRM_RampInterpType
osl_utilities::ramp::ToHoudiniInterpolation(int i_3delight_interpolation)
{
	unsigned nb_3delight_interpolations =
		sizeof(k_3delight_interpolation_to_houdini) /
		sizeof(k_3delight_interpolation_to_houdini[0]);
	return
		0 <= i_3delight_interpolation &&
			i_3delight_interpolation < nb_3delight_interpolations
		? k_3delight_interpolation_to_houdini[i_3delight_interpolation]
		: PRM_RAMP_INTERP_CONSTANT;
}

PRM_RampInterpType
osl_utilities::ramp::ToHoudiniInterpolation(
	const std::string& i_3delight_interpolation)
{
	if(i_3delight_interpolation == k_interpolation_none ||
		i_3delight_interpolation == k_interpolation_constant)
	{
		return PRM_RAMP_INTERP_CONSTANT;
	}

	if(i_3delight_interpolation == k_interpolation_linear)
	{
		return PRM_RAMP_INTERP_LINEAR;
	}

	if(i_3delight_interpolation == k_interpolation_smooth)
	{
		return PRM_RAMP_INTERP_MONOTONECUBIC;
	}

	if(i_3delight_interpolation == k_interpolation_catmullrom)
	{
		return PRM_RAMP_INTERP_CATMULLROM;
	}

	if(i_3delight_interpolation == k_interpolation_bspline)
	{
		return PRM_RAMP_INTERP_BSPLINE;
	}

	return PRM_RAMP_INTERP_CONSTANT;
}

bool
osl_utilities::ramp::FindMatchingRampParameters(
	const DlShaderInfo& i_shader,
	const DlShaderInfo::Parameter& i_value,
	const DlShaderInfo::Parameter*& o_knots,
	const DlShaderInfo::Parameter*& o_interpolation,
	const DlShaderInfo::Parameter*& o_shared_interpolation,
	std::string& o_base_name)
{
	o_knots = nullptr;
	o_interpolation = nullptr;
	o_base_name.clear();

	if(!i_value.type.is_array())
	{
		return false;
	}

	/*
		Extract the common prefix of all the ramp's parameters by removing the
		"value" parameter's suffix, separated by an underscore, which we keep.
	*/
	std::string name = i_value.name.string();
	size_t end = name.rfind('_');
	if(end == std::string::npos || end == 0)
	{
		return false;
	}
	std::string prefix = name.substr(0, end+1);
	o_base_name = name.substr(0, end);

	/*
		Search for "knots", "interpolation" and "shared interpolation"
		parameters among the other shader parameters.
	*/
	unsigned nparams = i_shader.nparams();
	unsigned found = 0;
	for(unsigned p = 0; p < nparams && found < 3; p++)
	{
		const DlShaderInfo::Parameter* param = i_shader.getparam(p);
		assert(param);
		if(param == &i_value ||
			param->isoutput != i_value.isoutput ||
			param->name.string().substr(0, prefix.length()) != prefix)
		{
			continue;
		}

		if(!o_shared_interpolation && param->type.IsOneString())
		{
			o_shared_interpolation = param;
			found++;
		}

		if(param->type.is_array())
		{
			if(!o_knots && param->type.elementtype == NSITypeFloat)
			{
				o_knots = param;
				found++;
			}
			else if(!o_interpolation &&
				param->type.elementtype == NSITypeInteger)
			{
				o_interpolation = param;
				found++;
			}
		}
	}

	return o_knots && o_interpolation;
}
