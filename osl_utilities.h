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
		/// Widget types
		extern const std::string k_maya_color;
		extern const std::string k_maya_float;
		extern const std::string k_katana_color;
		extern const std::string k_katana_float;

		/// Parameter name suffixes
		extern const std::string k_maya_position_suffix;
		extern const std::string k_maya_color_suffix;
		extern const std::string k_maya_float_suffix;
		extern const std::string k_katana_position_suffix;
		extern const std::string k_katana_color_suffix;
		extern const std::string k_katana_float_suffix;
		extern const std::string k_interpolation_suffix;
		extern const std::string k_index_suffix;

		/// Describes a ramp widget
		enum eType
		{
			kMayaBit = 0x01,
			kColorBit = 0x02,
			kRampBit = 0x04,

			kMayaColor = kRampBit | kMayaBit | kColorBit,
			kMayaFloat = kRampBit | kMayaBit,
			kKatanaColor = kRampBit | kColorBit,
			kKatanaFloat = kRampBit,

			kNotRamp = 0x0
		};

		/// Returns the type of ramp widget described by i_widget
		eType GetType(const char* i_widget);
		/// Returns true if i_type is a valid ramp type
		inline bool IsRamp(eType i_type) { return  (i_type & kRampBit) != 0; }
		/// Returns true if i_type is a color (ie : not scalar) ramp type
		inline bool IsColor(eType i_type) { return (i_type & kColorBit) != 0; }
		/// Returns the appropriate suffix for the "position" parameter
		const std::string& GetPositionSuffix(eType i_type);
		/// Returns the appropriate suffix for the "value" parameter
		const std::string& GetValueSuffix(eType i_type);
		std::string RemoveSuffix(
			const std::string& i_name,
			const std::string& i_suffix);
	}
}
