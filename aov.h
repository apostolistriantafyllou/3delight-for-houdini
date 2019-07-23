#include <string>

namespace aov
{
	enum e_type // aov's type
	{
		e_shading,   // Shading component
		e_auxiliary, // Auxiliary variable
		e_custom     // Custom variable
	};

	struct description
	{
		e_type m_type;
		std::string m_ui_name; // must be unique
		std::string m_filename_token;
		std::string m_variable_name;
		std::string m_variable_source;
		std::string m_layer_type;
		bool m_with_alpha;
		bool m_support_multilight;
	};

	// Returns the aov's description from the specified ui name
	const description& getDescription(const std::string& i_ui_name);
}
