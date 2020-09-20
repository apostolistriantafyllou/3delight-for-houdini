#pragma once

#include <string>
#include <vector>

class VOP_Node;

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

	// Update custom variables of the aov's description
	void updateCustomVariables(const std::vector<VOP_Node*>& i_custom_aovs);

	// Removes all custom variables from the aov's description
	void removeAllCustomVariables();

	// Adds custom variable into the aov's description
	void addCustomVariable(
		const std::string& i_ui_name,
		const std::string& i_filename_token,
		const std::string& i_variable_name,
		const std::string& i_variable_source,
		const std::string& i_layer_type);

	// Returns true if i_aov_name is found in the custom variables of
	// the aov's description
	bool findCustomVariable(const std::string& i_aov_name);

	/// Returns the number of predefined AOVs
	unsigned nbPredefined();

	/// Returns the description of the nth AOV
	const description& getDescription(unsigned i_index);

	// Returns the aov's description from the specified ui name
	const description& getDescription(const std::string& i_ui_name);

	// Returns the aov's active layer output string
	const char* getActiveLayerOutputStr();
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

	// Returns the aov's active layer output for the specified index
	const char* getAovActiveLayerToken(int index);
	// Returns the aov's label token for the specified index
	const char* getAovLabelToken(int index);
	// Returns the aov's string token for the specified index
	const char* getAovStrToken(int index);
}
