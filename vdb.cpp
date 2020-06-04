#include "vdb.h"

#include "context.h"
#include "VOP_ExternalOSL.h"
#include "VDBQuery.h" /* From 3delight */

#include <GT/GT_Handles.h>
#include <OBJ/OBJ_Node.h>
#include <SOP/SOP_Node.h>

#include <nsi_dynamic.hpp>
#include <assert.h>
#include <nsi.hpp>
#include <iostream>

vdb::vdb(
	const context& i_ctx, OBJ_Node *i_obj,
	double i_time,
	const GT_PrimitiveHandle &i_gt_primitive,
	unsigned i_primitive_index,
	const std::string &i_vdb_file )
:
	primitive( i_ctx, i_obj, i_time, i_gt_primitive, i_primitive_index ),
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
	const char *const *grid_names = nullptr;
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

	/*
		Retrieve the required grid names from the material (see
		GetVolumeParams() in VOP_ExternalOSL.cpp), along with the velocity
		scale.
	*/

	UT_String density_grid;
	UT_String color_grid;
	UT_String temperature_grid;
	UT_String emissionintensity_grid;
	UT_String velocity_grid;
	double velocity_scale = 1.0;

	double time = m_context.m_current_time;
	OP_Node* op = m_object->getMaterialNode(time);

	/*
		This might seem useless but it's not, as resolve_material_path might
		return a child of "op" instead of "op" itself, if it's a Material
		Builder.
	*/
	VOP_Node *material = op ?
		resolve_material_path( op->getFullPath().c_str() ) :
		nullptr;

	if(material)
	{
		using namespace VolumeGridParameters;

		if(material->hasParm(density_name))
		{
			material->evalString(density_grid, density_name, 0, time);
		}
		if(material->hasParm(color_name))
		{
			material->evalString(color_grid, color_name, 0, time);
		}
		if(material->hasParm(temperature_name))
		{
			material->evalString(temperature_grid, temperature_name, 0, time);
		}
		if(material->hasParm(emission_name))
		{
			material->evalString(emissionintensity_grid, emission_name, 0, time);
		}
		if(material->hasParm(velocity_name))
		{
			material->evalString(velocity_grid, velocity_name, 0, time);
		}

		if(material->hasParm(velocity_scale_name))
		{
			velocity_scale = material->evalFloat(velocity_scale_name, 0, time);
		}
	}

	// Export required grid names if they're available
	for( int i=0; i<num_grids; i++ )
	{
		std::string grid( grid_names[i] ? grid_names[i] : "" );

		if( grid == density_grid.toStdString() )
		{
			arguments.Add( new NSI::StringArg("densitygrid", grid) );
		}

		if( grid == color_grid.toStdString() )
		{
			arguments.Add( new NSI::StringArg( "colorgrid", grid) );
		}

		if( grid == temperature_grid.toStdString() )
		{
			arguments.Add( new NSI::StringArg( "temperaturegrid", grid) );
		}

		if( grid == emissionintensity_grid.toStdString() )
		{
			arguments.Add( new NSI::StringArg( "emissionintensitygrid", grid) );
		}

		if( grid == velocity_grid.toStdString() )
		{
			arguments.Add( new NSI::StringArg( "velocitygrid", grid) );
		}
	}

	arguments.Add( new NSI::DoubleArg( "velocityscale", velocity_scale ) );

	m_nsi.SetAttribute( m_handle + "|volume", arguments );

	primitive::set_attributes();
}

bool vdb::is_volume()const
{
	return true;
}

/**
	There is no motion blur parameters to set here on the volumes itself as the
	volume contains a velocity field that is supported by 3Delight. BUT,
	the actual volume container might be moving, so we need to set the
	right matrix.
*/
void vdb::set_attributes_at_time(
	double i_time,
	const GT_PrimitiveHandle i_gt_primitive) const
{
	/*
		FIXME: hopefull this will always be idenityt as the instancer takes
		care of the transform. If not we are in trouble.
	*/

#if 0
	const GT_TransformHandle &handle = i_gt_primitive->getPrimitiveTransform();
	UT_Matrix4D local;
	handle->getMatrix( local );

	/*
		FIXME : this is redundant with the attribute already set by
		primitive::connect. However, since our way of supporting VDBs involves
		them being read as instances, that part is skipped (for now). We don't
		want the transform to be applied twice!
	*/
	/* The stars are aligned for Houdini and NSI */
	m_nsi.SetAttributeAtTime(
		m_handle,
		i_time,
		NSI::DoubleMatrixArg( "transformationmatrix", local.data() ) );
#endif
}

/**
	\brief Returns the path of a VDB file if this node is a "VDB loader".

	\returns path to VDB file if the file SOP indeed loads a VDB file or
	empty string if not a valid VDB loader.

	Just try to find any FILE SOP that has a VDB.

	We don't support the general case VDB as we go through the file
	SOP. For now we just skip this until we can find a more elegant
	way to handle both file-loaded VDBs and general Houdini volumes.
*/
std::string vdb::node_is_vdb_loader( OBJ_Node *i_node, double i_time )
{
	std::vector< SOP_Node *> files;
	std::vector< OP_Node * > traversal; traversal.push_back( i_node );

	while( traversal.size() )
	{
		OP_Node *network = traversal.back();
		traversal.pop_back();
		int nkids = network->getNchildren();
		for( int i=0; i< nkids; i++ )
		{
			OP_Node *node = network->getChild(i);
			SOP_Node *sop = node->castToSOPNode();
			if( sop )
			{
				const UT_StringRef &SOP_name = node->getOperator()->getName();
				if( SOP_name == "file" )
					files.push_back( sop );
			}

			if( !node->isNetwork() )
				continue;

			OP_Network *kidnet = (OP_Network *)node;
			if( kidnet->getNchildren() )
			{
				traversal.push_back( kidnet );
			}
		}
	}

	if( files.size() != 1 )
	{
#if 0
		fprintf(
			stderr,
			"3Delight for Houdini: we support only one VDB file per file obj" );
#endif
		return {};
	}

	SOP_Node *file_sop = files.back();
	int index = file_sop->getParmIndex( "file" );
	if( index < 0 )
	{
		return {};
	}

	UT_String file;
	file_sop->evalString( file, "file", 0, i_time );

	if( !file.fileExtension() || ::strcmp(file.fileExtension(),".vdb") )
	{
		return {};
	}

	return std::string( file.c_str() );
}

