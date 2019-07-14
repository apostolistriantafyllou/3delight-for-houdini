#pragma once

#include <nsi_dynamic.hpp>
#include <3Delight/ShaderQuery.h>

#include <string>

class OP_OperatorTable;


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
	std::string get_shader_path( const char *i_vop_name ) const;
	DlShaderInfo *get_shader_info( const char *i_path ) const;

	void Register( OP_OperatorTable* io_table) const;

	/**
		\brief given an operator name, returns the OSL name.

		\param i_vop
			The name of the operator as returned by
			VOP_Node::getOperator()->getName().

		\returns a name usable on the file system.

		https://www.sidefx.com/docs/houdini/assets/namespaces.html
	*/
	static std::string vop_to_osl( const char *i_vop );

public:
	std::string m_plugin_path;

	NSI::DynamicAPI m_api;
	decltype(&DlGetShaderInfo) m_shader_info_ptr = nullptr;
};
