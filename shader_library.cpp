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


/**
	Find the path of the dso (ROP) and guess the shader path from there
	for all Houdini shaders.
*/
shader_library::shader_library()
{
	m_rop_path = library_path();

	assert( m_rop_path.size() != 0 );
}

const shader_library &shader_library::get_instance( void )
{
	return g_shader_library;
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
		m_rop_path + "/osl/" + name + ".oso";

	if( file_exists( installation_path.c_str()) )
		return installation_path;

	return {};
}

/* -----------------------------------------------------------------------------

	The following system functions have been stolen from the venerable DlSystem
	so don't look past *this point* :>

   -------------------------------------------------------------------------- */

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
