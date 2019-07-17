#include "osl_utilities.h"

const std::string osl_utilities::k_maya_color_ramp = "maya_colorRamp";
const std::string osl_utilities::k_maya_float_ramp = "maya_floatRamp";
const std::string osl_utilities::k_katana_float_ramp = "floatRamp";

const std::string osl_utilities::k_position_suffix = "_Position";
const std::string osl_utilities::k_color_value_suffix = "_ColorValue";
const std::string osl_utilities::k_float_value_suffix = "_FloatValue";
const std::string osl_utilities::k_floats_suffix = "_Floats";
const std::string osl_utilities::k_interpolation_suffix = "_Interp";
const std::string osl_utilities::k_index_suffix = "_#_";

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

bool
osl_utilities::IsRamp(const char* i_widget)
{
	return
		i_widget &&
		(i_widget == k_maya_color_ramp || i_widget == k_maya_float_ramp ||
		i_widget == k_katana_float_ramp);
}

