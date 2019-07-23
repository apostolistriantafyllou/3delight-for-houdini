#include <string>

namespace aovs
{
	enum e_type
	{
		e_shading,
		e_auxiliary,
		e_custom
	};

	struct desc_aov
	{
		e_type m_type;
		std::string m_ui_name;
		std::string m_variable_name;
		std::string m_variable_source;
		std::string m_layer_type;
		bool m_with_alpha;
		bool m_support_multilight;
	};

	const desc_aov& getDescAov(const std::string& i_ui_name);
}
