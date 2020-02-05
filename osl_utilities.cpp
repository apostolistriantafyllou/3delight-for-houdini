#include "osl_utilities.h"

#include <assert.h>

static const std::string k_empty;

const std::string osl_utilities::k_null = "null";
const std::string osl_utilities::k_check_box = "checkBox";
const std::string osl_utilities::k_filename = "filename";

const std::string osl_utilities::ramp::k_maya_color = "maya_colorRamp";
const std::string osl_utilities::ramp::k_maya_float = "maya_floatRamp";
const std::string osl_utilities::ramp::k_katana_color = "colorRamp";
const std::string osl_utilities::ramp::k_katana_float = "floatRamp";

const std::string osl_utilities::ramp::k_maya_position_suffix = "_Position";
const std::string osl_utilities::ramp::k_maya_color_suffix = "_ColorValue";
const std::string osl_utilities::ramp::k_maya_float_suffix = "_FloatValue";
const std::string osl_utilities::ramp::k_katana_position_suffix = "_Knots";
const std::string osl_utilities::ramp::k_katana_color_suffix = "_Colors";
const std::string osl_utilities::ramp::k_katana_float_suffix = "_Floats";

const std::string osl_utilities::ramp::k_interpolation_suffix = "_Interp";

const std::string osl_utilities::ramp::k_index_suffix = "_#_";
const std::string osl_utilities::ramp::k_index_format = "_%u_";

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

osl_utilities::ramp::eType
osl_utilities::ramp::GetType(const char* i_widget)
{
	if(!i_widget)
	{
		return kNotRamp;
	}

	if(i_widget == k_maya_color)
	{
		return kMayaColor;
	}

	if(i_widget == k_maya_float)
	{
		return kMayaFloat;
	}

	if(i_widget == k_katana_color)
	{
		return kKatanaColor;
	}

	if(i_widget == k_katana_float)
	{
		return kKatanaFloat;
	}

	return kNotRamp;
}

const std::string&
osl_utilities::ramp::GetPositionSuffix(osl_utilities::ramp::eType i_type)
{
	assert(i_type != kNotRamp);
	return
		(i_type & kMayaBit) != 0
		? k_maya_position_suffix
		: k_katana_position_suffix;
}

const std::string&
osl_utilities::ramp::GetValueSuffix(osl_utilities::ramp::eType i_type)
{
	assert(i_type != kNotRamp);
	switch(i_type)
	{
		case kMayaColor:
			return k_maya_color_suffix;
		case kMayaFloat:
			return k_maya_float_suffix;
		case kKatanaColor:
			return k_katana_color_suffix;
		case kKatanaFloat:
			return k_katana_float_suffix;
		default:
			assert(false);
			return k_empty;
	}
}

std::string
osl_utilities::ramp::RemoveSuffix(
	const std::string& i_name,
	const std::string& i_suffix)
{
	int root_length = int(i_name.length()) - int(i_suffix.length());
	if(root_length > 0 && i_name.substr(root_length) == i_suffix)
	{
		return i_name.substr(0, root_length);
	}

	return i_name;
}
