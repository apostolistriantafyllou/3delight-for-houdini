#include "light.h"

#include "context.h"

#include <OP/OP_Director.h>
#include <OBJ/OBJ_Node.h>
#include <UT/UT_String.h>
#include <GT/GT_Handles.h>

#include <assert.h>
#include <nsi.hpp>
#include <iostream>

light::light(
	const context& i_ctx,
	OBJ_Node *i_object,
	const GT_PrimitiveHandle &i_gt_primitive )
:
	exporter( i_ctx, i_object, i_gt_primitive )
{
	m_handle = i_object->getFullPath();
	m_handle += "|light";
}

void light::create( void ) const
{
	/*
		We will be crating the light's geometry under this transform.
		Note that we create the light even if not enabled. This will
		be useful for IPR.
	*/
	m_nsi.Create( m_handle.c_str(), "transform" );

}

void light::create_default_geometry( void ) const
{
	if( !m_object->evalInt("light_enable", 0, 0) )
		return;

	int type = m_object->evalInt( "light_type", 0, 0 );
	int is_spot = m_object->evalInt( "coneenable", 0, 0 );

	// for area lights.
	float x_size = m_object->evalFloat( "areasize", 0, 0 ) * 0.5f;
	float y_size = m_object->evalFloat( "areasize", 1, 0 ) * 0.5f;

	std::cout << "light type for " << m_object->getFullPath() <<
		" is " << type << std::endl;

	std::string geo_name = m_handle + "|geometry";

	if( type == e_point || type == e_disk || type == e_sphere )
	{
		m_nsi.Create( geo_name.c_str(), "particles" );

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
			args.push( new NSI::FloatArg( "width", 2.0f ) );
		}

		if( type == e_disk )
		{
			args.push( NSI::Argument::New( "N" )
				->SetType( NSITypeNormal )
				->SetValuePointer( &N[0] ) );
		}

		m_nsi.SetAttribute( geo_name.c_str(), args );
	}
	else if( type == e_grid || is_spot )
	{
		float scale = is_spot ? 0.001f : 1.0f;

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

		m_nsi.Create( geo_name.c_str(), "mesh" );
		m_nsi.SetAttribute( geo_name.c_str(), mesh_attributes );
	}
	else if( type == e_tube )
	{
		m_nsi.Create( geo_name.c_str(), "mesh" );

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
		m_nsi.SetAttribute( geo_name.c_str(), args );
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
	m_nsi.Connect(
		geo_name.c_str(), "",
		m_handle.c_str(), "objects" );
}

void light::set_attributes( void ) const
{
	if( !m_object->evalInt("light_enable", 0, 0) )
		return;

	create_default_geometry();
}

void light::set_attributes_at_time( double i_time ) const
{
	return;
}
