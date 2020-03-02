#include "shader_library.h"
#include "dl_system.h"

#include "delight.h"


#include "VOP_ExternalOSL.h"

#include <UT/UT_UI.h>
#include <VOP/VOP_Operator.h>

#include <vector>
#include <functional>

#include <assert.h>

/* Our only instance */
static shader_library g_shader_library;

using namespace dl_system;

/**
	Find the path of the plugin's dso and guess the shader path from there
	for all Houdini shaders.
*/
shader_library::shader_library()
{
	m_plugin_path = dir_name( library_path() );

	assert( m_plugin_path.size() != 0 );

	m_api.LoadFunction(m_shader_info_ptr, "DlGetShaderInfo");
	assert( m_shader_info_ptr );

	decltype( &DlGetInstallRoot) get_install_root = nullptr;
	m_api.LoadFunction(get_install_root, "DlGetInstallRoot" );

	if( get_install_root )
	{
		find_all_shaders( get_install_root() );
	}

	if( !m_shader_info_ptr || !get_install_root )
	{
		const char *delight = get_env("DELIGHT" );
		const char *ld = get_env("LD_LIBRARY_PATH" );
		const char *dyld = get_env("DYLD_LIBRARY_PATH" );

		if( !delight )
			delight = "null";
		if( !ld )
			ld = "null";
		if( !dyld )
			dyld = "null";

		std::cerr
			<< "3Delight for Houdini: no 3Delight installation found\n\n"
			<< "\t$DELIGHT: " << delight << "\n"
			<< "\t$LD_LIBRARY_PATH: " << ld << "\n"
			<< "\t$DYLD_LIBRARY_PATH: " << dyld << "\n"
			<< std::endl;
	}

	assert( get_install_root );
	assert( m_shader_info_ptr );
}

const shader_library &shader_library::get_instance( void )
{
	return g_shader_library;
}

DlShaderInfo *shader_library::get_shader_info( const char *path ) const
{
	if( !m_shader_info_ptr )
	{
		assert( false );
		return nullptr;
	}

	return m_shader_info_ptr( path );
}

/**
	\param name
		The name of the operator to look for without extension

	Just a quick implementation to get us to the next step:
	- we check plug-ins installation path + OSO
	- and some developer path (to be removed later on).
*/
std::string shader_library::get_shader_path( const char *vop ) const
{
	std::string name = vop_to_osl(vop);
	if( name.size() == 0 )
		return {};

	name += ".oso";

	for(const ShadersGroup& g : m_shaders)
	{
		auto it = g.m_osos.find( name );
		if( it != g.m_osos.end() )
			return it->second;
	}

	return {};
}

/**
	FIXME: use regex match.
*/
std::string shader_library::vop_to_osl( const char *i_vop )
{
	std::string legalized( i_vop );

	auto p1 = legalized.find( "::" );
	if( p1 == std::string::npos )
	{
		/* no occurrences, nothing to do. */
		return legalized;
	}

	auto p2 = legalized.find( "::", p1+2 );

	/* Legalize for file system */
	std::replace( legalized.begin(), legalized.end(), '.', '_' );
	std::replace( legalized.begin(), legalized.end(), ':', '_' );

	if( p2 == std::string::npos )
	{
		/* only one occurrences, could be namespace or version */
		return std::isdigit(legalized[p1+2]) ?
			legalized : legalized.substr(p1+2);
	}

	/* two occurrences, skip namespace */
	return legalized.substr(p2 + 2);
}

void tokenize_path( const char *osl_path, std::vector<std::string> &o_paths )
{
	if( !osl_path )
		return;

#ifdef _WIN32
	const char *delimters = ";";
#else
	const char *delimters = ":";
#endif

	/*
		::strtok will modify the passed string so allocate a copy.
	*/
	osl_path = ::strdup( osl_path );
	char *token = ::strtok( (char *)osl_path, delimters );
	do
	{
		o_paths.push_back( token );
		token = ::strtok(nullptr, delimters);
	} while( token );

	free( (void *)osl_path );
}


void shader_library::find_all_shaders( const char *i_root)
{
	// Get 3Delight installation shaders.
	std::string root(i_root);

	std::string plugin_osl = root + "/osl/";
	const char *shaders[] =
	{
		"dlHairAndFur",
		"dlPrincipled",
		"dlMetal",
		"dlSkin",
		"dlGlass",
		"dlSubstance",
		"dlWorleyNoise",
		"dlBoxNoise",
		"dlCarPaint",
		"dlAtmosphere",
		"dlColorVariation",
		"dlColorToFloat",
		"dlFloatToColor",
		"dlRamp",
		"dlTexture",
		"dlToon",
		"dlTriplanar",
		"dlAttributeRead",
		"dlFlakes",
		"dlThin",
		"dlRandomInputColor",
		"dlRandomMaterial",
		"dlFloatMath",
		"dlFloatBlend",
		"dlColorBlend",
		"dlColorBlendMulti",
		"dlColorCorrection",
		"vdbVolume",
		"transparent"
	};

	m_shaders.emplace_back("3Delight", "3Delight");
	for( const auto &shader : shaders )
	{
		std::string oso( shader ); oso += ".oso";
		std::string path_to_oso = plugin_osl + oso;
		if( file_exists(path_to_oso.c_str()) )
		{
			m_shaders.back().m_osos[oso] = path_to_oso;
		}
	}

	// Get user specified shaders.
	std::vector<std::string> to_scan;
	const char *user_osos = get_env( "_3DELIGHT_USER_OSO_PATH" );
	tokenize_path( user_osos, to_scan );

	m_shaders.emplace_back("user-defined", "3Delight/User-defined");
	for( auto &path : to_scan )
	{
		scan_dir( path, m_shaders.back().m_osos );
	}

	// Get 3Delight for Houdini shaders.
	m_shaders.emplace_back("3Delight for Houdini");
	std::string installation_path = m_plugin_path + "/../osl";
	scan_dir( installation_path, m_shaders.back().m_osos );

	// Get 3Delight for Maya shaders.
	m_shaders.emplace_back("3Delight for Maya", "3Delight/Maya");
	std::string maya_path = root + "/maya/osl";
	scan_dir( maya_path, m_shaders.back().m_osos );
	remove_useless_maya_shaders( m_shaders.back().m_osos );

#ifndef NDEBUG
	if(UTisUIAvailable())
	{
		std::cout << "3Delight for Houdini: loaded";
		for(const ShadersGroup& g : m_shaders)
		{
			std::cout << "\n  " << g.m_osos.size() << " " << g.m_name << " shaders";
		}
		std::cout << std::endl;
	}
#endif
}

/// Registers one VOP for each .oso file found in the shaders path
void shader_library::Register(OP_OperatorTable* io_table)const
{
	for(const ShadersGroup& g : m_shaders)
	{
		if(g.m_menu.empty())
		{
			continue;
		}

		for( auto &oso : g.m_osos )
		{
			const DlShaderInfo* info = get_shader_info( oso.second.c_str() );
			if(!info)
			{
#ifdef NDEBUG
				std::cerr
					<< "3Delight for Houdini: unable to load " << g.m_name
					<< " shader " << oso.first;
#endif
				continue;
			}

			io_table->addOperator(
				new VOP_ExternalOSLOperator(
						StructuredShaderInfo(info),
						g.m_menu));
		}
	}
}

void shader_library::remove_useless_maya_shaders(
	std::unordered_map<std::string, std::string> &i_osos )
{
	for( auto it=i_osos.begin(); it!=i_osos.end(); )
	{
		const std::string &shader_name = it->first;

		bool useless =
			shader_name.find( "dl", 0) == 0 ||
			shader_name.find( "uv", 0) == 0 ||
			shader_name.find( "_", 0) == 0 ||
			shader_name.find( "material", 0) == 0 ||
			shader_name.find( "shadingEngine", 0) == 0 ||
			shader_name.find( "Light", 0 ) != std::string::npos ||
			shader_name == "file.oso" ||
			shader_name == "psdFileTex.oso" ||
			shader_name == "surfaceShader.oso" ||
			shader_name == "NetworkMaterials_Surface.oso" ||
			shader_name == "envChrome.oso" ||
			shader_name == "samplerInfo.oso" ||
			shader_name == "shader3DelightAtmosphere.oso" ||
			shader_name == "displacementShader.oso" ||
			shader_name == "delightEnvironment.oso" ||
			shader_name == "texture3DelightSky.oso";

		if( useless )
		{
			it = i_osos.erase(it);
		}
		else
			++it;
	}
}
