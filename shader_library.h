#pragma once

#include <nsi_dynamic.hpp>
#include <3Delight/ShaderQuery.h>

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
	DlShaderInfo *get_shader_info( const char *i_path ) const;


public:
	std::string m_rop_path;
	std::string m_plugin_path;

	NSI::DynamicAPI m_api;
	decltype(&DlGetShaderInfo) m_shader_info_ptr = nullptr;
};
