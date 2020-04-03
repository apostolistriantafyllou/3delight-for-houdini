#pragma once

#include <3Delight/ShaderQuery.h>

#include <PRM/PRM_Type.h>

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

		/// Returns the parameter name's suffix at a specific index
		std::string ExpandedIndexSuffix(unsigned i_index);

		/// Returns true if the string describes a ramp-type widget
		bool IsRampWidget(const char* i_widget);

		/// Converts Houdini's ramp interpolation into ours (used in our OSLs).
		int FromHoudiniInterpolation(PRM_RampInterpType i_houdini_interpolation);
		/// Converts our ramp interpolation (used in our OSLs) into Houdini's.
		PRM_RampInterpType ToHoudiniInterpolation(int i_3delight_interpolation);
		/// Converts our ramp shared interpolation (used in our OSLs) into Houdini's.
		PRM_RampInterpType ToHoudiniInterpolation(
			const std::string& i_3delight_interpolation);

		/**
			\brief Finds information about a shader's ramp parameters.

			Given the main "value" parameter of a "ramp" contraption in a
			shader, this function identifies the other related parameters of the
			shader, based only on their name and type.

			\param i_shader
				Description of the shader containing the ramp thingy.
			\param i_value
				The main parameter of a ramp.
			\param o_knots
				To be set to the "knots" parameter of the ramp.
			\param o_interpolation
				To be set to the "interpolation" parameter of the ramp.
			\param o_shared_interpolation
				To be set to the "shared interpolation" parameter of the ramp,
				which is a string describing the interpolation to be used for
				all points. Not finding this one is not an error.
			\param o_base_name
				Will be set to the common prefix of the ramp's parameters names.
			\return
				True if o_knots and o_interpolation were found.
		*/
		bool FindMatchingRampParameters(
			const DlShaderInfo& i_shader,
			const DlShaderInfo::Parameter& i_value,
			const DlShaderInfo::Parameter*& o_knots,
			const DlShaderInfo::Parameter*& o_interpolation,
			const DlShaderInfo::Parameter*& o_shared_interpolation,
			std::string& o_base_name);
	}
}
