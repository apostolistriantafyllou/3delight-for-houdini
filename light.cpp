#include "light.h"
#include "vop.h"
#include "shader_library.h"

#include "context.h"

#include <VOP/VOP_Node.h>
#include <OBJ/OBJ_Node.h>
#include <UT/UT_String.h>
#include <GT/GT_Handles.h>

#include <assert.h>
#include <nsi.hpp>
#include <iostream>

light::light( const context& i_ctx, OBJ_Node *i_object )
:
	exporter( i_ctx, i_object )
{
	m_handle = i_object->getFullPath();
	m_handle += "|light";

	m_is_env_light = i_object->getParmPtr("env_map") != nullptr;
}

void light::create( void ) const
{
	/*
		We will be creating the light's geometry under this transform.
		Note that we create the light even if not enabled. This will
		be useful for IPR.
	*/
	m_nsi.Create( m_handle, "transform" );

	/*
		Create the attribute node so that we can connect the surface
		shader used for the light source.
	*/
	std::string attributes = attributes_handle();
	m_nsi.Create( attributes, "attributes" );

	std::string shader( m_handle ); shader += "|shader";
	m_nsi.Create( shader, "shader" );

	const shader_library& library = shader_library::get_instance();
	std::string path_to_hlight = library.get_shader_path(shader_name());

	m_nsi.SetAttribute(
		shader, NSI::StringArg("shaderfilename", path_to_hlight) );

	m_nsi.Connect( shader, "", attributes, "surfaceshader" );
	m_nsi.Connect( attributes, "", m_handle, "geometryattributes" );
}

void light::create_geometry( void ) const
{
	std::string geo_name = m_handle + "|geometry";

	if( m_is_env_light )
	{
		/*
			Envlight is a bit different as it doesn't have a "light_type"
			parameter.
		*/
		m_nsi.Create( geo_name, "environment" );
		m_nsi.SetAttribute( geo_name, NSI::DoubleArg("angle", 360) );
		m_nsi.Connect( geo_name, "", m_handle, "objects" );
		return;
	}

	int type = m_object->evalInt( "light_type", 0, 0 );
	int is_spot = m_object->evalInt( "coneenable", 0, 0 );

	// for area lights.

	if( type == e_grid || is_spot )
	{
		float x_size = 0.001f;
		float y_size = x_size;

		if( !is_spot )
		{
			x_size = m_object->evalFloat( "areasize", 0, 0 ) * 0.5f;
			y_size = m_object->evalFloat( "areasize", 1, 0 ) * 0.5f;
		}

		float P[] = {
			-x_size, -y_size, 0.0f,  -x_size, y_size, 0.0f,
			x_size, y_size, 0.0f,   x_size, -y_size, 0.0f };
		int nvertices[] = { 4 };

		NSI::ArgumentList mesh_attributes;
		mesh_attributes.Add(
			NSI::Argument::New( "P" )
			->SetType( NSITypePoint )
			->SetCount( 4 )
			->SetValuePointer( const_cast<float*>(P)) );

		mesh_attributes.Add(
			NSI::Argument::New( "nvertices" )
			->SetType( NSITypeInteger )
			->SetCount( 1 )
			->SetValuePointer( const_cast<int*>(nvertices)) );

		m_nsi.Create( geo_name, "mesh" );
		m_nsi.SetAttribute( geo_name, mesh_attributes );
	}
	else if( type == e_point || type == e_disk || type == e_sphere )
	{
		NSI::ArgumentList args;
		float P[3] = { 0.0f, 0.0f, 0.0f };
		float N[3] = { 0.0f, 0.0f, -1.0f }; // for disks
		args.push( NSI::Argument::New( "P" )
			->SetType( NSITypePoint )
			->SetValuePointer( &P[0] ) );

		if( type == e_point )
		{
			/* Create a very small sphere particle */
			args.push( new NSI::FloatArg( "width", 1e-3f ) );
		}
		else
		{
			args.push( new NSI::FloatArg( "width", 1.0f ) );
		}

		if( type == e_disk )
		{
			args.push( NSI::Argument::New( "N" )
				->SetType( NSITypeNormal )
				->SetValuePointer( &N[0] ) );
		}

		m_nsi.Create( geo_name, "particles" );
		m_nsi.SetAttribute( geo_name, args );
	}
	else if( type == e_tube )
	{
		m_nsi.Create( geo_name, "mesh" );

		/* The cylinder is 1 unit long (in X) and has a radius of 0.5. */
		std::vector<float> P;
		std::vector<int> indices, nvertices;
		const int kNumSteps = 18;
		for( int i = 0; i < kNumSteps; ++i )
		{
			float angle = (float(i) / float(kNumSteps)) * float(2.0 * M_PI);
			float z = 1.0f * cos( angle );
			float y = 1.0f * sin( angle );
			P.push_back( y ); P.push_back( 1.0f ); P.push_back( z );
			P.push_back( y ); P.push_back( -1.0f ); P.push_back( z );

			nvertices.push_back( 4 );
			indices.push_back( i * 2 );
			indices.push_back( i * 2 + 1 );
			indices.push_back( (i * 2 + 3) % (2 * kNumSteps) );
			indices.push_back( (i * 2 + 2) % (2 * kNumSteps) );
		}

		NSI::ArgumentList args;
		args.push( NSI::Argument::New( "nvertices" )
			->SetType( NSITypeInteger )
			->SetCount( nvertices.size() )
			->SetValuePointer( &nvertices[0] ) );
		args.push( NSI::Argument::New( "P" )
			->SetType( NSITypePoint )
			->SetCount( 2 * kNumSteps )
			->SetValuePointer( &P[0] ) );
		args.push( NSI::Argument::New( "P.indices" )
			->SetType( NSITypeInteger )
			->SetCount( 4 * kNumSteps )
			->SetValuePointer( &indices[0] ) );
		m_nsi.SetAttribute( geo_name, args );
	}
	else if( type == e_distant )
	{
		/*
			Yes ladies and gentlemen, a distant light is just an environment
			with an angle of 0 :)
		*/
		m_nsi.Create( geo_name, "environment" );
		m_nsi.SetAttribute( geo_name, NSI::DoubleArg("angle", 0) );
	}
	else if( type == e_geometry )
	{
		UT_String path;
		m_object->evalString(path, "areageometry", 0, 0.0f);

		OBJ_Node* obj_node = OPgetDirector()->findOBJNode(path);
		if(!obj_node)
		{
			return;
		}

		/*
			Simply connect to the named geo, welcome to NSI!
			FIXME: support "intothisobject"
		*/
		geo_name = obj_node->getFullPath();
	}

	/* Connect to parent transform. */
	m_nsi.Connect( geo_name, "", m_handle, "objects" );
}

void light::set_attributes( void ) const
{
	create_geometry();

	set_visibility_to_camera();
}

/**
	We set the attributes of the surface shader here. We trick ourselves
	to believe that hlight is actually a vop node so that we just have
	to write an "hlight.osl" and that's it.
*/
void light::set_attributes_at_time( double i_time ) const
{
	NSI::ArgumentList list;
	vop::list_shader_parameters(
		m_vop,
		shader_name(),
		i_time, -1, list );

	std::string shader(m_handle); shader += "|shader";
	m_nsi.SetAttributeAtTime( shader, i_time, list );
}

void light::connect( void ) const
{
	/*
		The light is always created, but connected only if visible, so its easy
		to deal with visibility changes in IPR.
	*/
	if( m_object->evalInt("light_enable", 0, 0) )
	{
		exporter::connect();
	}
}

void light::set_visibility_to_camera()const
{
	int cam_vis = m_object->evalInt("light_contribprimary", 0, 0);
	m_nsi.SetAttribute(
		attributes_handle(), NSI::IntegerArg("visibility.camera", cam_vis));
}
