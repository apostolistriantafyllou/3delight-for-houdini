#pragma once

#include <3Delight/ShaderQuery.h>

namespace osl_utilities
{
	/// Contains pointers to some useful meta-data in the shader's description
	struct ParameterMetaData
	{
		const char* m_label = nullptr;
		const char* m_widget = nullptr;
		const char* m_options = nullptr;
		const int* m_imin = nullptr;
		const int* m_imax = nullptr;
		const int* m_slider_imin = nullptr;
		const int* m_slider_imax = nullptr;
		const float* m_fmin = nullptr;
		const float* m_fmax = nullptr;
		const float* m_slider_fmin = nullptr;
		const float* m_slider_fmax = nullptr;
	};

	/// Widget types
	extern const std::string k_null;
	extern const std::string k_check_box;
	extern const std::string k_maya_color_ramp;
	extern const std::string k_maya_float_ramp;
	extern const std::string k_katana_float_ramp;

	/// Parameter name suffixes
	extern const std::string k_position_suffix;
	extern const std::string k_color_value_suffix;
	extern const std::string k_float_value_suffix;
	extern const std::string k_floats_suffix;
	extern const std::string k_interpolation_suffix;
	extern const std::string k_index_suffix;

	/// Sets o_value to the value of string-type metadata i_name, if it exists
	void FindMetaData(
		const char*& o_value,
		const DlShaderInfo::constvector<DlShaderInfo::Parameter>& i_metadata,
		const char* i_name);

	/// Retrieves a bunch of useful meta-data
	void GetParameterMetaData(
		ParameterMetaData& o_data,
		const DlShaderInfo::constvector<DlShaderInfo::Parameter>& i_metadata);

	/// Returns true if a widget name is one of the "ramp" types
	bool IsRamp(const char* i_widget);
}
