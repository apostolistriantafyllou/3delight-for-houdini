#include "exporter.h"

#include "context.h"

#include <nsi.hpp>

#include <OBJ/OBJ_Node.h>
#include <VOP/VOP_Node.h>

exporter::exporter(
	const context& i_context, OBJ_Node *i_object )
:
	m_context(i_context),
	m_nsi(i_context.m_nsi),
	m_object(i_object)
{
	assert(m_object);
	m_handle = m_object->getFullPath();
}

exporter::exporter( const context &i_context, VOP_Node *i_node )
:
	m_context(i_context),
	m_nsi(i_context.m_nsi),
	m_vop(i_node)
{
	assert(m_vop);
	m_handle = m_vop->getFullPath();
}

const std::string &exporter::handle( void ) const
{
	return m_handle;
}

/**
	Utility to get an NSI type from a GT_Type.

	\returns NSITypeInvalid if we don't know what to do with
	the type
*/
NSIType_t gt_to_nsi_type( GT_Type i_type )
{
	switch( i_type)
	{
	case GT_TYPE_POINT: return  NSITypePoint;
	case GT_TYPE_COLOR: return  NSITypeColor;
	case GT_TYPE_VECTOR: return  NSITypeVector;
	case GT_TYPE_TEXTURE: return  NSITypeFloat; // it's a float float[3]
	case GT_TYPE_NORMAL: return  NSITypeNormal;
	case GT_TYPE_NONE: return  NSITypeFloat;
	default:
		break;
	}
	return NSITypeInvalid;
}

const char* exporter::TransparentSurfaceHandle()
{
	return "3delight_transparent_surface";
}

/**
	gotchas:
	- When we find a texture coordinate attribute, we output "st".
	  regardless of the name we find (usually "uv"). This is to be
	  consistent with other packages.
*/
void exporter::export_attributes(
	const GT_AttributeListHandle *i_attributes,
	int i_n, double i_time,
	std::vector<const char *> &io_which_ones,
	const int* i_which_flags ) const
{
	for(int w = io_which_ones.size()-1; w >= 0; w--)
	{
		const char *name = io_which_ones[w];

		for( int i=0; i<i_n; i++ )
		{
			if( !i_attributes[i] )
			{
				continue;
			}

			const GT_DataArrayHandle &data = i_attributes[i]->get( name );
			if( data.get() == nullptr )
			{
				continue;
			}

			NSIType_t nsi_type = gt_to_nsi_type( data->getTypeInfo() );
			if( nsi_type == NSITypeInvalid )
			{
				std::cout << "unsupported attribute type " << data->getTypeInfo()
					<< " of name " << name << " on " << m_handle;
				continue;
			}

			int flags = i_which_flags ? i_which_flags[w] : 0;

			io_which_ones.erase(io_which_ones.begin()+w);

			GT_DataArrayHandle buffer_in_case_we_need_it;

			if( data->getTypeInfo() == GT_TYPE_TEXTURE )
			{
				m_nsi.SetAttributeAtTime( m_handle, i_time,
					*NSI::Argument("st")
						.SetArrayType( NSITypeFloat, 3)
						->SetCount( data->entries() )
						->SetValuePointer(
							data->getF32Array(buffer_in_case_we_need_it))
						->SetFlags(flags));
				continue;
			}

			m_nsi.SetAttributeAtTime( m_handle, i_time,
				*NSI::Argument(name)
					.SetType( nsi_type )
					->SetCount( data->entries() )
					->SetValuePointer( data->getF32Array(buffer_in_case_we_need_it))
					->SetFlags(flags));
		}
	}

	return; // so that we don't fall into the void.
}

void exporter::export_override_attributes() const
{
	if ( !m_object->hasParm("override_vol") )
	{
		// We presume that our other attributes are not present
		return;
	}

	std::string	override_nsi_handle = m_handle;
	override_nsi_handle += "|attributeOverrides";

	m_nsi.Create( override_nsi_handle.c_str(), "attributes" );

	NSI::ArgumentList arg_list;

	float time = m_context.m_current_time;

	// Visible to Camera override
	if ( m_object->hasParm("_3dl_over_vis_camera_enable") )
	{
		bool over_vis_camera_enable =
			m_object->evalInt("_3dl_over_vis_camera_enable", 0, time);

		if( over_vis_camera_enable )
		{
			bool vis_camera_over =
				m_object->evalInt("_3dl_over_vis_camera", 0, time);

			arg_list.Add(
				new NSI::IntegerArg("visibility.camera", vis_camera_over));
			arg_list.Add(
				new NSI::IntegerArg("visibility.camera.priority", 10));
		}
		else
		{
			m_nsi.DeleteAttribute(
				override_nsi_handle.c_str(), "visibility.camera" );
			m_nsi.DeleteAttribute(
				override_nsi_handle.c_str(), "visibility.camera.priority" );
		}
	}

	// Visible in Diffuse override
	if ( m_object->hasParm("_3dl_over_vis_diffuse_enable") )
	{
		bool over_vis_diffuse_enable =
			m_object->evalInt("_3dl_over_vis_diffuse_enable", 0, time);

		if( over_vis_diffuse_enable )
		{
			bool vis_diffuse_over = false;
				m_object->evalInt("_3dl_over_vis_diffuse", 0, time);

			arg_list.Add(
				new NSI::IntegerArg("visibility.diffuse", vis_diffuse_over));
			arg_list.Add(
				new NSI::IntegerArg("visibility.diffuse.priority", 10));
		}
		else
		{
			m_nsi.DeleteAttribute(
				override_nsi_handle.c_str(), "visibility.diffuse" );
			m_nsi.DeleteAttribute(
				override_nsi_handle.c_str(), "visibility.diffuse.priority" );
		}
	}

	// Visible in Reflections override
	if ( m_object->hasParm("_3dl_over_vis_reflection_enable") )
	{
		bool over_vis_reflection_enable =
			m_object->evalInt("_3dl_over_vis_reflection_enable", 0, time);

		if( over_vis_reflection_enable )
		{
			bool over_vis_reflection =
				m_object->evalInt("_3dl_over_vis_reflection", 0, time);

			arg_list.Add(
				new NSI::IntegerArg("visibility.reflection", over_vis_reflection));
			arg_list.Add(
				new NSI::IntegerArg("visibility.reflection.priority", 10));
		}
		else
		{
			m_nsi.DeleteAttribute(
				override_nsi_handle.c_str(), "visibility.reflection" );
			m_nsi.DeleteAttribute(
				override_nsi_handle.c_str(), "visibility.reflection.priority" );
		}
	}

	// Visible in Refractions override
	if ( m_object->hasParm("_3dl_over_vis_refraction_enable") )
	{
		bool over_vis_refraction_enable =
			m_object->evalInt("_3dl_over_vis_refraction_enable", 0, time);

		if( over_vis_refraction_enable )
		{
			bool over_vis_refraction =
				m_object->evalInt("_3dl_over_vis_refraction", 0, time);

			arg_list.Add(
				new NSI::IntegerArg("visibility.refraction", over_vis_refraction));
			arg_list.Add(
				new NSI::IntegerArg("visibility.refraction.priority", 10));
		}
		else
		{
			m_nsi.DeleteAttribute(
				override_nsi_handle.c_str(), "visibility.refraction" );
			m_nsi.DeleteAttribute(
				override_nsi_handle.c_str(), "visibility.refraction.priority" );
		}
	}

	// Compositing mode override
	if ( m_object->hasParm("_3dl_over_compositing_enable") )
	{
		bool over_compositing_enable =
			m_object->evalInt("_3dl_over_compositing_enable", 0, time);

		if( over_compositing_enable )
		{
			UT_String over_compositing;
			m_object->evalString(over_compositing, "_3dl_over_compositing", 0, time);

			bool matte = over_compositing == "matte";
			bool prelit = over_compositing == "prelit";

			arg_list.Add(new NSI::IntegerArg("matte", matte));
			arg_list.Add(new NSI::IntegerArg("matte.priority", 10));
			arg_list.Add(new NSI::IntegerArg("prelit", prelit));
			arg_list.Add(new NSI::IntegerArg("prelit.priority", 10));
		}
		else
		{
			m_nsi.DeleteAttribute(
				override_nsi_handle.c_str(), "matte" );
			m_nsi.DeleteAttribute(
				override_nsi_handle.c_str(), "matte.priority" );
			m_nsi.DeleteAttribute(
				override_nsi_handle.c_str(), "prelit" );
			m_nsi.DeleteAttribute(
				override_nsi_handle.c_str(), "prelit.priority" );
		}
	}

	// Set all override attributes defined above
	if( arg_list.size() > 0 )
	{
		m_nsi.SetAttribute( override_nsi_handle.c_str(), arg_list );
	}

	/**
		Adjust connections to make the override effective or not according to
		its	main override toggle.
	*/
	if ( m_object->hasParm("_3dl_override_vol") )
	{
		bool override_vol =	m_object->evalInt("_3dl_override_vol", 0, time);
		if( override_vol )
		{
			m_nsi.Connect( m_handle.c_str(), "",
				override_nsi_handle.c_str(), "bounds" );
			m_nsi.Connect( override_nsi_handle.c_str(), "",
				NSI_SCENE_ROOT, "geometryattributes" );

			m_nsi.Connect(
				TransparentSurfaceHandle(), "",
				m_handle.c_str(), "geometryattributes" );
#if 0
			// FIXME - how to get surfaceShaderHandle?
			bool override_ss = m_object->evalInt("_3dl_override_ss", 0, time);

			if ( override_ss )
			{
				NSI::ArgumentList argList;
				argList.Add(new NSI::IntegerArg("priority", 10));
				std::string surfaceShaderHandle;
				m_nsi.Connect(
					surfaceShaderHandle.c_str(), "",
					override_nsi_handle.c_str(), "surfaceshader",
					argList );
			}
#endif
		}
		else
		{
			m_nsi.Disconnect( m_handle.c_str(), "",
				override_nsi_handle.c_str(), "bounds" );
			m_nsi.Disconnect( override_nsi_handle.c_str(), "",
				NSI_SCENE_ROOT, "geometryattributes" );
		}
	}
}
