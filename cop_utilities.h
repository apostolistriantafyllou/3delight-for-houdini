#pragma once

#include <string>

class context;

/**
	Utility class to convet COPs into texture files rendeable by 3DelightNSI.
*/
struct cop_utilities
{
	static std::string convert_to_texture(
		const context &i_context, const std::string & );

private:
	static std::string create_cop_directory( const std::string &i_rop );
};
