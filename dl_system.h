#pragma once

#include <string>
#include <unordered_map>

/**
	Useful [file]system functions.

	Why not use hboost::filesystem ? We don't want to be dependent on boost. I
	suspect SESI could update boost at any time and this would immediately
	break compatibility.

	Some of implementation is taken from our faithful and reliable DlSystem from
	3delight core. We also use FS library from the HDK. Hopefully that one is
	stable enough.
*/
namespace dl_system
{
	std::string library_path( void );
	bool file_exists( const char *name );
	std::string dir_name( const std::string &i_path );
	const char *get_env( const char* i_var );
	bool scan_dir(
		const std::string& i_path,
		std::unordered_map<std::string, std::string>& io_list );
	bool create_directory_for_file( const std::string &i_file );
	std::string delight_doc_url();
}
