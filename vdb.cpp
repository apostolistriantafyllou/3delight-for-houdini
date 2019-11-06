#include "vdb.h"

#include "context.h"
#include "VDBQuery.h" /* From 3delight */

#include <GT/GT_Handles.h>

#include <nsi_dynamic.hpp>
#include <assert.h>
#include <nsi.hpp>
#include <iostream>

vdb::vdb(
	const context& i_ctx, OBJ_Node *i_obj,
	const GT_PrimitiveHandle &i_gt_primitive,
	const std::string &i_vdb_file )
:
	exporter( i_ctx, i_obj, i_gt_primitive),
	m_vdb_file(i_vdb_file)
{
}

void vdb::create( void ) const
{
	m_nsi.Create( m_handle, "transform" );
	std::string volume = m_handle + "|volume";
	m_nsi.Create( volume, "volume" );

	m_nsi.Connect( volume, "", m_handle, "objects" );
}

void vdb::set_attributes( void ) const
{
	NSI::DynamicAPI api;
#ifdef __APPLE__
	/*
		Houdini will nuke [DY]LD_LIBRARY_PATH on macOS. This is a bit insane
		but reality is more insane so that's fine. We will try to get the
		library path manually, something we usually try to avoid for robustness
		reasons.
	*/
	const char *macos_delight = ::getenv("DELIGHT" );
	if( macos_delight )
	{
		std::string path_to_lib(macos_delight);
		path_to_lib += "/lib/lib3delight.dylib";
		api = NSI::DynamicAPI( path_to_lib.c_str() );
	}
#endif

	/*
	*/
	decltype( &DlVDBGetGridNames) vdb_grids = nullptr;
	api.LoadFunction(vdb_grids, "DlVDBGetGridNames");

	if( vdb_grids == nullptr )
	{
		::fprintf( stderr,
			"3Delight for Houdini: unable to load VDB utility "
			"function from 3Delight dynamic library" );
		return;
	}

	int num_grids = 0;
	const char *const *grid_names;
	if( !vdb_grids( m_vdb_file.c_str(), &num_grids, &grid_names) ||
		num_grids==0 ||
		!grid_names )
	{
		::fprintf( stderr,
			"3Delight for Houdini: no usable grid in VDB %s",
			m_vdb_file.c_str() );
		return;
	}

	NSI::ArgumentList arguments;
	arguments.Add( new NSI::CStringPArg( "vdbfilename", m_vdb_file.c_str()) );

	for( int i=0; i<num_grids; i++ )
	{
		std::string grid( grid_names[i] ? grid_names[i] : "" );

		if( grid == "density" )
		{
			arguments.Add( new NSI::StringArg("densitygrid", grid) );
		}
		else if( grid == "temperature" )
		{
			arguments.Add( new NSI::StringArg( "temperaturegrid", grid) );
		}
		else if( grid == "heat" )
		{
			arguments.Add( new NSI::StringArg( "emissionintensitygrid", grid) );
		}
		else if( grid == "velocity" || grid == "v" || grid == "vel" )
		{
			arguments.Add( new NSI::StringArg( "velocitygrid", grid) );
		}
	}

#if 0
	arguments.Add( new NSI::DoubleArg( "velocityscale", doubleValue ) );
#endif

	m_nsi.SetAttribute( m_handle + "|volume", arguments );
}

/**
	There is no motion blur parameters to set here on the volumes itself as the
	volume contains a velocity field that is supported by 3Delight. BUT,
	the actual volume container might be moving, so we need to set the
	right matrix.
*/
void vdb::set_attributes_at_time( double i_time ) const
{
	const GT_TransformHandle &handle = m_gt_primitive->getPrimitiveTransform();
	UT_Matrix4D local;
	handle->getMatrix( local );

	/* The stars are aligned for Houdini and NSI */
	m_nsi.SetAttributeAtTime(
		m_handle,
		i_time,
		NSI::DoubleMatrixArg( "transformationmatrix", local.data() ) );
}
