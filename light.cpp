#include "light.h"

#include "context.h"
#include "geometry.h"
#include "null.h"
#include "object_attributes.h"
#include "shader_library.h"
#include "time_sampler.h"
#include "vop.h"
#include "delight.h"
#include "osl_utilities.h"

#include <VOP/VOP_Node.h>
#include <OBJ/OBJ_Node.h>
#include <UT/UT_String.h>
#include <GT/GT_Handles.h>

#include <assert.h>
#include <nsi.hpp>
#include <iostream>

namespace
{

	NSI::DynamicAPI&
		GetNSIAPI()
	{
		static NSI::DynamicAPI api;
		return api;
	}

	/**
		Returns the handle of the NSI "attributes" node that has a "prelit"
		attribute set to 1.
	*/
	const char* prelit_attribute_node_handle()
	{
		return
			object_attributes::geo_attribute_node_handle(
				object_attributes::e_prelit);
	}

}

light::light( const context& i_ctx, OBJ_Node *i_object )
:
	exporter( i_ctx, i_object )
{
	m_handle = handle(*m_object, i_ctx);

	m_is_env_light = m_object->getParmPtr("env_map") != nullptr;
	m_is_sky_map = m_object->getParmIndex("skymap_enable") != -1 &&
		m_object->evalInt("skymap_enable", 0, i_ctx.m_current_time) != 0;
	if( m_is_sky_map )
		m_is_env_light = true;

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

	/*
		Although we usually add strength to shader connections in order to
		protect them from recursive node deletions, we omit this parameter
		here because we own the shader, so it can't be shared.
	*/
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

	float x_size = m_object->evalFloat( "areasize", 0, time );
	float y_size = m_object->evalFloat( "areasize", 1, time );

	if( type == e_grid || is_spot )
	{
		if(is_spot)
		{
			/*
				Instead of setting y_size and x_size equal to 0.001 we use another
				paramter (_3dl_spotlight_size) as a multiplier (*0.001) so we can
				control it's shadow sharpness based on _3dl_spotlight_size value.
			*/
			float spotlight_size =
				m_object->evalFloat("_3dl_spotlight_size", 0, time);
			y_size = x_size = 0.001*spotlight_size;

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

		/*
			Scale sphere and disk using a matrix.
		*/
		if( type == e_disk )
		{
			double m[16] =
			{
				x_size, 0, 0, 0,
				0, y_size, 0, 0,
				0, 0, 1, 0,
				0, 0, 0, 1
			};
			m_nsi.SetAttribute(
				m_handle,
				NSI::DoubleMatrixArg( "transformationmatrix", m) );
		}
		else if( type == e_sphere )
		{
			double m[16] =
			{
				x_size, 0, 0, 0,
				0, y_size, 0, 0,
				0, 0, x_size, 0,
				0, 0, 0, 1.0
			};
			m_nsi.SetAttribute(
				m_handle,
				NSI::DoubleMatrixArg( "transformationmatrix",m) );
		}

		m_nsi.Create( geo_name, "particles" );
		m_nsi.SetAttribute( geo_name, args );
	}
	else if( type == e_tube || type == e_line)
	{
		m_nsi.Create( geo_name, "mesh" );

		// The base cylinder is 1 unit long (in X) and has a diameter of 0.15.
		x_size *= 0.5f;
		y_size *= 0.075f;

		/* Create a cylinder with a very small diameter so it looks like a line*/
		if (type == e_line)
			y_size = 0.0005;

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

		OBJ_Node* obj_node = OPgetDirector()->findOBJNode(path);
		if( !obj_node )
			obj_node = m_object->findOBJNode( path ); // relative?

		if(obj_node)
		{
			/*
				When "intothisobject" is enabled, we use the object's transform.
				Otherwise, we skip the transform and connect directly to its
				geometry, which is connected to an identity "hub" transform.
			*/
			std::string model =
				m_object->evalInt("intothisobject", 0, time)
				?	null::handle(*obj_node, m_context)
				:	geometry::hub_handle(*obj_node, m_context);

			m_nsi.Connect( model, "", geo_name, "objects");
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
		set_filter_attributes(*t);
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
	std::string dummy;

	vop::list_shader_parameters(
		m_context,
		m_vop,
		shader_name(),
		i_time, -1, list, dummy );

	m_nsi.SetAttributeAtTime( shader_handle(), i_time, list );
}

void light::connect( void ) const
{
	/*
		The light is always created, but connected only if visible, so its easy
		to deal with visibility changes in IPR.
	*/
	if( m_context.object_displayed(*m_object))
	{
		std::string parent_handle = null::handle(*m_object, m_context);
		m_nsi.Connect(m_handle, "", parent_handle, "objects");
	}

	/*
		We share object_attributes' "attributes" NSI node to set the prelit
		attribute on our geometry. We can't use the whole object_attributes
		system, however, because its meant for regular objects, which have a
		3-value "_3dl_compositing" parameter, while lights have a
		"_3dl_light_prelit" toggle (since they can't be mattes).
	*/
	const char* prelit = "_3dl_light_prelit";
	if( m_object->hasParm(prelit) &&
		m_object->evalInt(prelit, 0, m_context.m_current_time) )
	{
		m_nsi.Connect(
			prelit_attribute_node_handle(), "",
			m_handle, "geometryattributes");
	}
	connect_filter();
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
	OBJ_Node* obj = i_caller->castToOBJNode();
	assert(obj);

	if(i_type == OP_NODE_PREDELETE)
	{
		Delete(*obj, *ctx);
		return;
	}

	if(i_type != OP_PARM_CHANGED)
	{
		return;
	}


	intptr_t parm_index = reinterpret_cast<intptr_t>(i_data);

	if(null::is_transform_parameter_index(parm_index))
	{
		return;
	}

	light node(*ctx, obj);

	if(!node.set_single_shader_attribute(parm_index))
	{
		PRM_Parm& parm = node.m_object->getParm(parm_index);
		std::string name = parm.getToken();

		if(name == "light_enable" || name == "_3dl_light_prelit" ||
			name == "_3dl_filters_menu")
		{
			node.disconnect();
			// Will connect only if required
			node.connect();
			if (name == "_3dl_filters_menu")
			{
				node.set_filter_attributes(node.m_context.m_current_time);
			}
		}
		else if(name == "light_contribprimary")
		{
			node.set_visibility_to_camera();
		}
		else if(name == "light_type" || name == "coneenable" ||
			name == "areasize" || name == "areageometry" ||
			name == "vm_envangle" || name == "_3dl_spotlight_size")
		{
			// Rebuild the whole geometry
			node.delete_geometry();
			node.create_geometry();
			node.connect();
		}
		else if (name == "_3dl_gobo_scale")
		{
			node.set_filter_attributes(node.m_context.m_current_time);
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
	std::string parent_handle = null::handle(*m_object, m_context);
	m_nsi.Disconnect(m_handle, "", parent_handle, "objects");

	m_nsi.Disconnect(
		prelit_attribute_node_handle(), "",
		m_handle, "geometryattributes");
	m_nsi.Disconnect(NSI_ALL_NODES, "filter_output", shader_handle(), "filter");
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
	std::string dummy;

	/*
		Again, we use the light OBJ_Node as a VOP_Node
		(see set_attributes_at_time).
	*/
	vop::list_shader_parameters(
		m_context,
		m_vop,
		shader_name(),
		m_context.m_current_time,
		i_parm_index,
		list, dummy);

	if(list.empty())
	{
		//Check if we are modifying filter's parameters.
		PRM_Parm* menu = m_object->getParmPtr("_3dl_filters_menu");
		if (!menu)
			return false;

		UT_String filter_name;
		fpreal time = m_context.m_current_time;
		m_object->evalString(filter_name, "_3dl_filters_menu", 0, time);
		if (filter_name == "none" || m_object->getParmIndex("_3dl_filters_menu") == i_parm_index)
			return false;

		// Find the directory containing the osl shaders
		decltype(&DlGetInstallRoot) get_install_root = nullptr;
		GetNSIAPI().LoadFunction(get_install_root, "DlGetInstallRoot");
		std::string osl_dir;
		if (get_install_root)
		{
			osl_dir = std::string(get_install_root()) + "/osl/";
		}

		std::string filter_path = osl_dir + filter_name.toStdString() + ".oso";
		std::string filter_shader = m_handle + filter_name.toStdString() + "|shader";

		vop::list_shader_parameters(
			m_context,
			m_vop,
			filter_path.c_str(),
			m_context.m_current_time, i_parm_index, list, dummy);
		if (list.empty())
		{
			return false;
		}
		else
		{
			m_nsi.SetAttribute(filter_shader, list);
			return true;
		}
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

void light::connect_filter()const
{
	PRM_Parm* menu = m_object->getParmPtr("_3dl_filters_menu");
	if (!menu)
		return;

	UT_String filter_name;
	m_object->evalString(filter_name, "_3dl_filters_menu", 0, m_context.m_current_time);
	if (filter_name == "none")
		return;

	std::string filter_transform = m_handle + "|" + filter_name.toStdString();
	std::string filter_shader = m_handle + filter_name.toStdString() + "|shader";

	m_nsi.Create(filter_transform, "transform");
	m_nsi.Connect(filter_transform, "", m_handle, "objects");
	m_nsi.Create(filter_shader, "shader");
	m_nsi.Connect(filter_shader, "filter_output", shader_handle(), "filter");
}

void light::set_filter_attributes(double i_time) const
{
	PRM_Parm* menu = m_object->getParmPtr("_3dl_filters_menu");
	if (!menu)
		return;

	UT_String filter_name;
	fpreal time = m_context.m_current_time;
	m_object->evalString(filter_name, "_3dl_filters_menu", 0, time);
	if (filter_name == "none")
		return;

	// Find the directory containing the osl shaders
	decltype(&DlGetInstallRoot) get_install_root = nullptr;
	GetNSIAPI().LoadFunction(get_install_root, "DlGetInstallRoot");
	std::string osl_dir;
	if (get_install_root)
	{
		osl_dir = std::string(get_install_root()) + "/osl/";
	}

	//We don't load filters as VOP node so we have to find the path manually.
	std::string filter_path = osl_dir + filter_name.toStdString() + ".oso";
	std::string filter_shader = m_handle + filter_name.toStdString() + "|shader";

	NSI::ArgumentList list;
	std::string dummy;

	//Get all filter parameters from the corresponding osl file.
	vop::list_shader_parameters(
		m_context,
		m_vop,
		filter_path.c_str(),
		time, -1, list, dummy);

	m_nsi.SetAttributeAtTime(
		filter_shader, i_time, NSI::StringArg("shaderfilename", filter_path));

	//scale parameter is already existent on light node, so we can't have the same name
	//for our filter scale parameter. Instead we use another name and set it manually on NSI.
	if (filter_name == "dlGoboFilter")
	{
		float scale[2] =
		{
			(float)m_object->evalFloat("_3dl_gobo_scale", 0, time),
			(float)m_object->evalFloat("_3dl_gobo_scale", 1, time),
		};
		list.Add(NSI::Argument::New("scale")
			->SetArrayType(NSITypeFloat, 2)
			->CopyValue((float*)&scale[0], 2 * sizeof(float)));
	}
	m_nsi.SetAttributeAtTime(filter_shader, i_time, list);
}

std::string light::get_geometry_path( void ) const
{
	if( m_is_env_light )
		return {};

	fpreal time = m_context.m_current_time;
	int type = m_object->evalInt( "light_type", 0, time );
	if( type != e_geometry )
		return {};

	UT_String path;
	m_object->evalString(path, "areageometry", 0, time);

	OBJ_Node* obj_node = OPgetDirector()->findOBJNode(path);
	if( obj_node )
		return path.toStdString();

	obj_node = m_object->findOBJNode( path );
	if( !obj_node )
		return {};

	return obj_node->getFullPath().toStdString();
}

std::string light::handle(const OP_Node& i_light, const context& i_ctx)
{
	return exporter::handle(i_light, i_ctx) + "|light";
}

void light::Delete(OBJ_Node& i_node, const context& i_context)
{
	i_context.m_nsi.Delete(
		handle(i_node, i_context),
		NSI::IntegerArg("recursive", 1));
}

const char* light::shader_name()const
{
	if( m_is_sky_map )
		return "hlight_sky";

	return m_is_env_light ? "environmentlight" : "hlight";
}
