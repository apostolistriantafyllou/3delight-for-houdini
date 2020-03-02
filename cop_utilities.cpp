#include "cop_utilities.h"
#include "ROP_3Delight.h"
#include "context.h"
#include "dl_system.h"

#include <OP/OP_Node.h>
#include <OP/OP_Director.h>
#include <OP/OP_Context.h>
#include <ROP/ROP_Node.h>
#include <TIL/TIL_Raster.h>
#include <TIL/TIL_Sequence.h>
#include <COP2/COP2_Node.h>
#include <UT/UT_TempFileManager.h>
#include <IMG/IMG_File.h>

/**
	Given some OP, returns the path to a converted texture.

	\param i_context
		The current renderinging context.
	\param i_op
		Path the COPS2 operator.

	Returns the path to the EXR texture to use instead of the operator. There
	are two possible loations where we store the cooked images:

	* In NSI export mode, they are stored in the cops/ subdirectory of where
	  the .nsi files are saved.
	* In other modes, we create temporary files that are deleted after each
	  render.

	We use EXR textures as this seems safer than using 8 bits.

	TODO: do we need to do somthing about color spaces?
*/
std::string cop_utilities::convert_to_texture(
	const context &i_context,
	const std::string &i_op )
{

	OP_Node* op_node = OPgetDirector()->findNode( i_op.c_str() );
	COP2_Node *cop = op_node->castToCOP2Node();
	if( !cop )
	{
		return {};
	}

	short key;
	TIL_Raster *image = NULL;
	if( cop->open(key) )
	{
		cop->close( key );
		return {};
	}

	const TIL_Sequence *seq = cop->getSequenceInfo();
	if( seq )
	{
		const TIL_Plane *plane = seq->getPlane(0);
		int xres,yres;
		seq->getRes(xres,yres);

		if( plane )
		{
			image = new TIL_Raster(
				PACK_RGB, plane->getFormat(), xres, yres );

			if( seq->getImageIndex(i_context.m_current_time) == -1 )
			{
				// out of frame range - black frame
				float black[4] = { 0.f };
				image->clearNormal( black );
			}
			else
			{
				OP_Context op_context( i_context.m_current_time );
				op_context.setXres( xres );
				op_context.setYres( yres );

				if( !cop->cookToRaster(image, op_context, plane) )
				{
					delete image;
					image = nullptr;
				}
			}
		}
	}

	std::string file_name;

	if( image )
	{
		if( i_context.m_export_nsi )
		{
			char *s = ::strdup( i_op.data() );

			/*
				Replace all occurences of "/" by "-" so that we have a valid
				file name
			*/
			int pos = 0;
			do
			{
				if( s[pos] == '/' ) s[pos] = '-';
			}
			while( s[pos++] );

			file_name = s+1; /* skip first '-' */
			::free( s );

			std::string dir = create_cop_directory( i_context.m_rop_path );

			if( dir.empty() )
			{
				file_name = {}; /* error */
			}
			else
			{
				int frame = OPgetDirector()->getChannelManager()->getFrame(
					i_context.m_current_time );
				char frameid[5] = {0};
				snprintf( frameid, 5, "%04d", frame );

				file_name = dir + "/" + file_name + "-";
				file_name += frameid;
				file_name += ".exr";

			}
		}
		else
		{
			file_name = UT_TempFileManager::getTempFilename();
			file_name += ".exr";

			/*
				Both the original (cooked) file and its auto-converted TDL will
				be deleted at context destruction (end of render).
			*/
			i_context.m_temp_filenames.push_back( file_name );
			i_context.m_temp_filenames.push_back( file_name + ".auto.tdl" );
			UT_TempFileManager::addTempFile( file_name );
			UT_TempFileManager::addTempFile( file_name + ".auto.tdl" );
		}
	}

	IMG_File::saveRasterAsFile( file_name.c_str(), image );
	delete image;

	/* must be called even if open() failed (WTF?) */
	cop->close(key);

	return file_name;
}

/** */
std::string cop_utilities::create_cop_directory( const std::string &i_rop )
{
	ROP_3Delight *rop = (ROP_3Delight *)
		OPgetDirector()->findNode( i_rop.data() )->castToROPNode();

	std::string path =
		rop->GetNSIExportFilename( 0 /* time doesn't matter */ );

	if( path.empty() || path == "stdout" )
	{
		/* This could happend if we are exporting to the console */
		return {};
	}

	path = dl_system::dir_name( path );

	std::string dir = path + "/cops";

	/* Note that 'somefile' won't be created, its just a tag. */
	if( !dl_system::create_directory_for_file(dir + "somefile") )
		return {};

	return dir;
}
