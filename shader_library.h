#pragma once

#include <string>

/**
	\brief This class (singleton) is reponsible for loading OSO shaders and
	creating VOP nodes. It also gives the full path to any shader (for light
	sources, materials, etc...) through get_shader_path.
*/
class shader_library
{

public:
	static const shader_library &get_instance( void );
	shader_library();

public:
	std::string get_shader_path( const char *i_name ) const;

public:
	std::string m_plugin_path;
};
