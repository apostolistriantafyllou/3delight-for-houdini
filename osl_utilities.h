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
	extern const std::string k_filename;

	/// Sets o_value to the value of string-type metadata i_name, if it exists
	void FindMetaData(
		const char*& o_value,
		const DlShaderInfo::constvector<DlShaderInfo::Parameter>& i_metadata,
		const char* i_name);

	/// Retrieves a bunch of useful meta-data
	void GetParameterMetaData(
		ParameterMetaData& o_data,
		const DlShaderInfo::constvector<DlShaderInfo::Parameter>& i_metadata);

	namespace ramp
	{
		/// Parameter name suffixes
		extern const std::string k_index_suffix;
		extern const std::string k_index_format;

		/// Returns true if the string describes a ramp-type widget
		bool IsRampWidget(const char* i_widget);

		/**
			\brief Finds the knots and interpolation 

			Given the main "value" parameter of a "ramp" contraption in a
			shader, this function identifies the other related parameters of the
			shader, based only on their name and type.

			\param i_shader
			\param i_value
			\param o_knots
			\param o_interpolation
			\param o_base_name
			\return
		*/
		bool FindMatchingRampParameters(
			const DlShaderInfo& i_shader,
			const DlShaderInfo::Parameter& i_value,
			const DlShaderInfo::Parameter*& o_knots,
			const DlShaderInfo::Parameter*& o_interpolation,
			std::string& o_base_name);
	}
}
