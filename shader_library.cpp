#include "shader_library.h"

#include <assert.h>

#ifdef _WIN32
#	include <windows.h>
#	include <process.h>
#else
#	include <dlfcn.h>
#endif
#include <sys/stat.h>

/* Our only instance */
static shader_library g_shader_library;

/* forward declaration. code at the end. */
static std::string library_path( void );
static bool file_exists( const char *name );
static std::string dir_name( const std::string &i_path );


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
		The name of the shader to look for without extension

	Just a quick implementation to get us to the next step:
	- we check plug-ins installation path + OSO
	- and some developper path (to be removed later on).
*/
std::string shader_library::get_shader_path( const char *name ) const
{
	std::string installation_path =
		m_plugin_path + "/../osl/" + name + ".oso";

	if( file_exists( installation_path.c_str()) )
		return installation_path;

	return {};
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
