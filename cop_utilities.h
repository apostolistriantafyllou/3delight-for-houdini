#pragma once

#include <string>

class context;
class ROP_3Delight;

/**
	Utility class to convet COPs into texture files rendeable by 3DelightNSI.
*/
struct cop_utilities
{
	static std::string convert_to_texture(
		const context &i_context, const std::string & );

private:
	static std::string create_cop_directory( const ROP_3Delight * );
};
