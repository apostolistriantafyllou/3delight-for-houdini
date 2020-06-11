#include "dl_system.h"

#include "delight.h"
#include <locale>

#ifdef _WIN32
#	include <windows.h>
#	include <process.h>
#else
#	include <dlfcn.h>
#endif
#include <sys/stat.h>
#include <string.h>

#include <FS/FS_Info.h>
#include <FS/UT_Directory.h>
#include <UT/UT_NTStreamUtil.h>

namespace dl_system
{

/**
	\brief This will return the filesystem path of the current library (which is
	the 3Delight for Houdini plug-in).
*/
std::string library_path( void )
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

bool file_exists( const char *i_path )
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
bool has_drive_letter( const std::string &i_path )
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

/**
*/
bool create_directory_for_file( const std::string &i_file )
{
	return UTcreateDirectoryForFile( i_file.data() );
}

std::string delight_doc_url()
{
	std::string url_name = "https://www.3delight.com/documentation/display/3DfH/";
	return url_name;
}

} /* namespace dl_system */
