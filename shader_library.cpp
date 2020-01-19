#include "shader_library.h"
#include "delight.h"

#include "VOP_ExternalOSL.h"

#include <FS/FS_Info.h>
#include <FS/UT_Directory.h>
#include <UT/UT_UI.h>
#include <VOP/VOP_Operator.h>

#include <vector>
#include <functional>

#include <assert.h>

#ifdef _WIN32
#	include <windows.h>
#	include <process.h>
#else
#	include <dlfcn.h>
#endif
#include <sys/stat.h>
#include <string.h>

/* Our only instance */
static shader_library g_shader_library;

/* forward declaration. code at the end. */
static std::string library_path( void );
static bool file_exists( const char *name );
static std::string dir_name( const std::string &i_path );
static const char *get_env( const char* i_var );
bool scan_dir(
	const std::string& i_path,
	std::unordered_map<std::string, std::string>& io_list);

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
		"dlTriplanar",
		"dlAttributeRead",
		"dlFlakes",
		"dlThin",
		"dlRandomInputColor",
		"dlFloatMath",
		"dlFloatBlend",
		"dlColorBlend",
		"dlColorBlendMulti",
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

/* -----------------------------------------------------------------------------

	The following system functions have been stolen from the venerable DlSystem
	so don't look past *this point* :>

   -------------------------------------------------------------------------- */

#include <locale>

static std::string library_path( void )
{
	static int foo = 0;

#ifdef _WIN32
	HMODULE hMod;
	if( !GetModuleHandleEx(
			GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
			| GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			(LPCTSTR)&foo,
			&hMod ) )
	{
		return std::string();
	}

	char name[1024];
	if( 0 != GetModuleFileName( hMod, name, sizeof(name) ) )
	{
		name[sizeof(name)-1] = 0;
		return std::string( name );
	}
#else
	Dl_info info;
	if( dladdr( &foo, &info ) )
	{
		if( info.dli_fname )
			return std::string( info.dli_fname );
	}
#endif

	return {};
}

static bool file_exists( const char *i_path )
{
#ifdef _WIN32
	return (GetFileAttributes(i_path) != 0xFFFFFFFF) ? TRUE:FALSE;
#else
	struct stat buf;
	return lstat(i_path, &buf)==0;
#endif
}

/**
	The equivalent of unix dirname, but portable.

	NOTES
	- This should be fixed to work better with multiple consecutive slashes.
	- Two leading slashes should also be kept as such because XBD 4.11 Pathname
	Resolution says:
		A pathname that begins with two successive slashes may be interpreted
		in an implementation-defined manner, although more than two leading
		slashes shall be treated as a single slash.
	Although I do not know of any UNIX system which makes use of this.
*/
static bool has_drive_letter( const std::string &i_path )
{
	return
		i_path.size() >= 2 &&
		i_path[1] == ':' &&
		std::isalpha( i_path[0], std::locale::classic() );
}

std::string dir_name( const std::string &i_path )
{
	/* See if there's a drive letter. That must be mostly ignored. */
	std::string::size_type drive_skip = has_drive_letter( i_path ) ? 2 : 0;

	auto delimiter = i_path.find_last_of( "/\\" );
	if( delimiter == std::string::npos )
	{
		/* No delimiter at all. Return either '.' or eg. 'c:' */
		if( drive_skip == 0 )
			return ".";
		else
			return i_path.substr( 0, 2 );
	}

	if( delimiter == drive_skip )
	{
		/* Root directory. Either '/' or 'c:/'. */
		return i_path.substr( 0, delimiter + 1 );
	}

	/* Check for trailing slash. */
	if( delimiter + 1 == i_path.size() )
	{
		/* Just recurse. */
		return dir_name( i_path.substr( 0, delimiter ) );
	}

	/* Common case: return path up to (but excluding) delimiter. */
	return i_path.substr( 0, delimiter );
}

const char *get_env( const char* i_var )
{
	char *value = getenv( i_var );

#ifdef _WIN32
	/*
		If the variable is not defined in the libc environment, look for it in the
		win32 environment and copy it to the libc one.
	*/
#define ENVVAR_VALUE_MAX_LENGTH 32767
	if( !value )
	{
		char *win32Value = (char*) malloc(ENVVAR_VALUE_MAX_LENGTH);
		if( GetEnvironmentVariable(
			i_var,
			win32Value,
			ENVVAR_VALUE_MAX_LENGTH ) )
		{
			char* var_val =
				(char*)malloc( strlen(i_var) + strlen(win32Value) + 2 );
			sprintf( var_val, "%s=%s", i_var, win32Value );
			_putenv( var_val );
			free( var_val );
			value = getenv( i_var );
		}

		free(win32Value);
	}
#endif

	return value;
}

bool scan_dir(
	const std::string& i_path,
	std::unordered_map<std::string, std::string>& io_list )
{
	FS_Info info(i_path.c_str());
	if(!info.getIsDirectory())
	{
		io_list[i_path] = i_path;
		return true;
	}

	UT_Directory dir(i_path.c_str(), nullptr, 0);
	UT_StringArray files;
	dir.getFiles("*", files);

	const std::string separator = 
#ifdef _WIN32
		"\\";
#else
		"/";
#endif

	std::string dir_path = i_path;
	for(int f = 0; f < files.size(); f++)
	{
		std::string file = files[f].toStdString();
		std::string full_path = dir_path + separator + file;
		io_list[file] = full_path;
	}

	return true;
}


