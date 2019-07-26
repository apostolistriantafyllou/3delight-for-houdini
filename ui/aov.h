#pragma once

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

	// Returns the aov's framebuffer output string
	const char* getAovFrameBufferOutputStr();
	// Returns the aov's file output string
	const char* getAovFileOutputStr();
	// Returns the aov's jpeg output string
	const char* getAovJpegOutputStr();
	// Returns the aov's label string
	const char* getAovLabelStr();
	// Returns the aov's string string
	const char* getAovStrStr();

	// Returns the aov's framebuffer output token for the specified index
	const char* getAovFrameBufferOutputToken(int index);
	// Returns the aov's file output token for the specified index
	const char* getAovFileOutputToken(int index);
	// Returns the aov's jpeg output token for the specified index
	const char* getAovJpegOutputToken(int index);
	// Returns the aov's label token for the specified index
	const char* getAovLabelToken(int index);
	// Returns the aov's string token for the specified index
	const char* getAovStrToken(int index);
}
