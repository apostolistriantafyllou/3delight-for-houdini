#include "light.h"

#include "vop.h"
#include "shader_library.h"

#include "context.h"
#include "time_sampler.h"

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
	m_handle += "|light";

	m_is_env_light = m_object->getParmPtr("env_map") != nullptr;
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

	std::string shader = shader_handle();
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
	std::string geo_name = geometry_handle();

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

	fpreal time = m_context.m_current_time;
	int type = m_object->evalInt( "light_type", 0, time );
	int is_spot = m_object->evalInt( "coneenable", 0, time );

	float x_size = 1.0f;
	float y_size = 1.0f;
	if(!is_spot && (type == e_grid || type == e_tube || type == e_geometry))
	{
		x_size = m_object->evalFloat( "areasize", 0, time );
		y_size = m_object->evalFloat( "areasize", 1, time );
	}


	if( type == e_grid || is_spot )
	{
		if(is_spot)
		{
			y_size = x_size = 0.001f;
		}
		else
		{
			x_size *= 0.5f;
			y_size *= 0.5f;
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
		/*
			FIXME : scale disk with (x_size, y_size) and sphere with
			(x_size, y_size, x_size).
		*/

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

		// The base cylinder is 1 unit long (in X) and has a diameter of 0.15.
		x_size *= 0.5f;
		y_size *= 0.075f;

		std::vector<float> P;
		std::vector<int> indices, nvertices;
		const int kNumSteps = 18;
		for( int i = 0; i < kNumSteps; ++i )
		{
			float angle = (float(i) / float(kNumSteps)) * float(2.0 * M_PI);
			float z = y_size * sin( angle );
			float y = y_size * cos( angle );
			P.push_back( x_size ); P.push_back( y ); P.push_back( z );
			P.push_back( -x_size ); P.push_back( y ); P.push_back( z );

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
	else if( type == e_distant || type == e_sun )
	{
		float angle =
			type == e_distant
			?	0.0f
			:	m_object->evalFloat("vm_envangle", 0, time) * 2.0f;

		/*
			Yes ladies and gentlemen, a distant/sun light is just an environment
			with a small angle :)
		*/
		m_nsi.Create( geo_name, "environment" );
		m_nsi.SetAttribute( geo_name, NSI::DoubleArg("angle", angle) );
	}
	else if( type == e_geometry )
	{
		UT_String path;
		m_object->evalString(path, "areageometry", 0, time);

		/*
			First create a transform node, which could be deleted in
			delete_geometry(), if needed, thus severing the connection to the
			user-defined node. Using a direct connection would require
			remembering the handle of the connected object for IPR.
		*/
		m_nsi.Create(geo_name, "transform");

		double scale[16] =
		{
			x_size, 0.0, 0.0, 0.0,
			0.0, y_size, 0.0, 0.0,
			0.0, 0.0, x_size, 0.0,
			0.0, 0.0, 0.0, 1.0
		};

		m_nsi.SetAttribute(
			geo_name,
			NSI::DoubleMatrixArg("transformationmatrix", scale));

		/*
			Simply connect to the named geo, welcome to NSI!
			FIXME: support "intothisobject"
		*/
		OBJ_Node* obj_node = OPgetDirector()->findOBJNode(path);
		if(obj_node)
		{
			m_nsi.Connect(
				obj_node->getFullPath().toStdString(), "", geo_name, "objects");
		}
	}

	/* Connect to parent transform. */
	m_nsi.Connect( geo_name, "", m_handle, "objects" );
}

void light::delete_geometry( void ) const
{
	m_nsi.Delete(geometry_handle());
}

void light::set_attributes( void ) const
{
	create_geometry();

	set_visibility_to_camera();

	for(time_sampler t(m_context, *m_object, time_sampler::e_deformation); t; t++)
	{
		set_attributes_at_time(*t);
	}
}

/**
	We set the attributes of the surface shader here. We trick ourselves
	into believing that hlight is actually a vop node so that we just have
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
	if( m_object->evalInt("light_enable", 0, m_context.m_current_time) )
	{
		std::string parent_handle = m_object->getFullPath().c_str();
		m_nsi.Connect(m_handle, "", parent_handle, "objects");
	}

	export_override_attributes();
}

/**
	\brief Callback that should be connected to an OP_Node that has an
	associated light exporter.
*/
void light::changed_cb(
	OP_Node* i_caller,
	void* i_callee,
	OP_EventType i_type,
	void* i_data)
{
	context* ctx = (context*)i_callee;
	if(i_type != OP_PARM_CHANGED)
	{
		return;
	}

	intptr_t parm_index = reinterpret_cast<intptr_t>(i_data);

	light node(*ctx, i_caller->castToOBJNode());
	if(!node.set_single_shader_attribute(parm_index))
	{
		PRM_Parm& parm = node.m_object->getParm(parm_index);
		std::string name = parm.getToken();

		if(name == "light_enable")
		{
			node.disconnect();
			// Will connect only if required
			node.connect();
		}
		else if(name == "light_contribprimary")
		{
			node.set_visibility_to_camera();
		}
		else if(name == "light_type" || name == "coneenable" ||
			name == "areasize" || name == "areageometry" ||
			name == "vm_envangle")
		{
			// Rebuild the whole geometry
			node.delete_geometry();
			node.create_geometry();
			node.connect();
		}
		else
		{
			return;
		}
	}

	ctx->m_nsi.RenderControl(NSI::CStringPArg("action", "synchronize"));
}

void light::disconnect()const
{
	std::string parent_handle = m_object->getFullPath().c_str();
	m_nsi.Disconnect(m_handle, "", parent_handle, "objects");
}

/*
	This is *very* similar to vop::set_single_attribute. Unfortunately, the NSI
	nodes are not named the same way in both classes (because the light's shader
	is an auxiliary node, not the "main" one in the light) so we can't just call
	vop(m_context, m_vop).set_single_shader_attribute(i_parm_index).
*/
bool light::set_single_shader_attribute(int i_parm_index)const
{
	NSI::ArgumentList list;

	/*
		Again, we use the light OBJ_Node as a VOP_Node
		(see set_attributes_at_time).
	*/
	vop::list_shader_parameters(
		m_vop,
		shader_name(),
		m_context.m_current_time,
		i_parm_index,
		list);

	if(list.empty())
	{
		return false;
	}

	m_nsi.SetAttribute( shader_handle(), list );

	return true;
}

void light::set_visibility_to_camera()const
{
	int cam_vis =
		m_object->evalInt("light_contribprimary", 0, m_context.m_current_time);
	m_nsi.SetAttribute(
		attributes_handle(), NSI::IntegerArg("visibility.camera", cam_vis));
}
