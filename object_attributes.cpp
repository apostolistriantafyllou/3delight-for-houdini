#include "object_attributes.h"

#include "context.h"

#include <OBJ/OBJ_Node.h>
#include <UT/UT_String.h>

void object_attributes::export_object_attribute_nodes(const context& i_ctx)
{
	for( int i = 0; i != e_invalidAttribute; i++ )
	{
		geo_attribute type = (geo_attribute)i;
		const char* handle = geo_attribute_node_handle( type );
		i_ctx.m_nsi.Create( handle, "attributes" );

		i_ctx.m_nsi.SetAttribute(
			handle,
			NSI::IntegerArg(
				geo_attribute_nsi_name( type ),
				geo_attribute_nsi_default_value( type ) ? 0 : 1 ) );
	}
}

void object_attributes::connect_to_object_attributes_nodes(
	const context& i_ctx,
	OBJ_Node* i_object,
	const std::string& i_handle	)
{
	for( int i = 0; i != e_invalidAttribute; i++ )
	{
		geo_attribute type = (geo_attribute)i;
		adjust_geo_attribute_connection( i_ctx, i_object, i_handle, type,
			/* first time = */ true );
	}
}

const char* object_attributes::geo_attribute_node_handle( geo_attribute i_type )
{
	switch( i_type )
	{
		case e_matte: return "3delight_matte";
		case e_prelit: return "3delight_prelit";
		case e_visCamera: return "3delight_invisibleToCameraRays";
		case e_visShadow: return "3delight_invisibleToShadowRays";
		case e_visDiffuse: return "3delight_invisibleToDiffuseRays";
		case e_visReflection: return "3delight_invisibleToReflectionRays";
		case e_visRefraction: return "3delight_invisibleToRefractionRays";
		case e_invalidAttribute:
		default: assert( false );
	}

	return "";
}

const char* object_attributes::geo_attribute_nsi_name( geo_attribute i_type )
{
	switch( i_type )
	{
		case e_matte: return "matte";
		case e_prelit: return "prelit";
		case e_visCamera: return "visibility.camera";
		case e_visShadow: return "visibility.shadow";
		case e_visDiffuse: return "visibility.diffuse";
		case e_visReflection: return "visibility.reflection";
		case e_visRefraction: return "visibility.refraction";
		case e_invalidAttribute:
		default: assert( false );
	}

	return "";
}

const char* object_attributes::geo_attribute_houdini_name( geo_attribute i_type )
{
	switch( i_type )
	{
		case e_matte:
		case e_prelit: return "_3dl_compositing";
		case e_visCamera: return "_3dl_visibility_camera";
		case e_visShadow: return "_3dl_visibility_shadow";
		case e_visDiffuse: return "_3dl_visibility_diffuse";
		case e_visReflection: return "_3dl_visibility_reflection";
		case e_visRefraction: return "_3dl_visibility_refraction";
		case e_invalidAttribute:
		default: assert( false );
	}

	return "";
}

bool object_attributes::geo_attribute_nsi_default_value( geo_attribute i_type )
{
	switch( i_type )
	{
		case e_matte:
		case e_prelit:
			return false;
		default:
			return true;
	}
}

bool object_attributes::geo_attribute_has_houdini_default_value(
	geo_attribute i_type,
	OBJ_Node* i_object,
	double i_time )
{
	assert( i_object );
	const char* parm_name = geo_attribute_houdini_name(i_type);

	/*
		As a safety measure, check whether the OBJ node has the actual
		3Delight attribute defined. This can happen if for some reason
		scripts are not run or if the user simply decides to remove
		attributes.
	*/
	if( !i_object->hasParm(parm_name) )
	{
		return true;
	}

	switch( i_type )
	{
		/*
			Matte and prelit require special processing because they're separate
			boolean attribute in NSI but are implemented by the same enum
			attribute in 3DFH, to make them mutually exclusive.
		*/
		case e_matte:
		{
			UT_String value;
			i_object->evalString(value, parm_name, 0, i_time);
			// Matte is off by default
			return value != "matte";
		}
		case e_prelit:
		{
			UT_String value;
			i_object->evalString(value, parm_name, 0, i_time);
			// Prelit is off by default
			return value != "prelit";
		}
		default:
			// Visibility attributes are all on by default
			return i_object->evalInt(parm_name, 0, i_time);
	}
}

void object_attributes::adjust_geo_attribute_connection(
	const context& i_ctx,
	OBJ_Node* i_object,
	const std::string& i_handle,
	geo_attribute i_type,
	bool i_first_time )
{
	if( !geo_attribute_has_houdini_default_value(
		i_type, i_object,i_ctx.m_current_time ) )
	{
		i_ctx.m_nsi.Connect(
			geo_attribute_node_handle( i_type ), "",
			i_handle.c_str(), "geometryattributes");
	}
	else if( !i_first_time )
	{
		// The disconnect operation is needed during live render.
		i_ctx.m_nsi.Disconnect(
			geo_attribute_node_handle( i_type ), "",
			i_handle.c_str(), "geometryattributes");
	}
}
