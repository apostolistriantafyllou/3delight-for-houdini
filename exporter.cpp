#include "exporter.h"

#include "context.h"

#include <nsi.hpp>

#include <OBJ/OBJ_Node.h>
#include <VOP/VOP_Node.h>

#include  <GT/GT_PrimPolygonMesh.h>

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

	NOTE: we don't try to resolve the "tuple size" here. For example,
	a float with a tuple size of 3 might be better of as a "point" in NSI.
	We let the caller take care of this.
*/
static NSIType_t gt_to_nsi_type( GT_Type i_type, GT_Storage i_storage )
{
	switch( i_type)
	{
	case GT_TYPE_POINT: return  NSITypePoint;
	case GT_TYPE_COLOR: return  NSITypeColor;
	case GT_TYPE_VECTOR: return  NSITypeVector;
	case GT_TYPE_TEXTURE: return  NSITypeFloat; // it's a float float[3]
	case GT_TYPE_NORMAL: return  NSITypeNormal;

	case GT_TYPE_NONE:
	{
		// HDK doc for GT_TYPE_NONE says "Implicit type based on data"
		switch( i_storage )
		{
		case GT_STORE_INT32: return NSITypeInteger;
		case GT_STORE_REAL32: return NSITypeFloat;
		case GT_STORE_REAL64: return NSITypeDouble;
		case GT_STORE_STRING: return NSITypeString;
		default:
			return NSITypeInvalid;
		}
	}

	default: return NSITypeInvalid;
	}
}

const char* exporter::transparent_surface_handle()
{
	return "3delight_transparent_surface";
}

bool exporter::find_attribute(
	const GT_Primitive& i_primitive,
	const std::string& i_name,
	GT_DataArrayHandle& o_data,
	NSIType_t& o_nsi_type,
	int& o_nsi_flags,
	bool& o_point_attribute)const
{
	// Scan the 4 attributes lists for one with the right name
	for(unsigned a = GT_OWNER_VERTEX; a <= GT_OWNER_DETAIL; a++)
	{
		const GT_AttributeListHandle& attributes =
			i_primitive.getAttributeList(GT_Owner(a));
		if( !attributes )
		{
			continue;
		}

		o_data = attributes->get(i_name.c_str());
		if( !o_data )
		{
			continue;
		}

		o_nsi_type = gt_to_nsi_type(o_data->getTypeInfo(), o_data->getStorage());

		if( o_nsi_type == NSITypeInvalid || o_nsi_type == NSITypeString )
		{
			std::cerr
				<< "unsupported attribute type " << o_data->getTypeInfo()
				<< " of name " << i_name << " on " << m_handle;
			continue;
		}

		/*
			This flag is needed because of particles. If the number of
			particles varies from one time step to the next, 3Delight might
			not be able to guess that those attributes are assigned
			per-vertex simply by looking at their count.
		*/
		o_nsi_flags = a == GT_OWNER_VERTEX ? NSIParamPerVertex : 0;

		o_point_attribute = a == GT_OWNER_POINT;

		return true;
	}

	return false;
}

/**
	gotchas:
	- When we find a texture coordinate attribute, we output "st".
	  regardless of the name we find (usually "uv"). This is to be
	  consistent with other packages.
	- We have a similar patch for "rest" and "rnml" that we tranform
	  to "Pref" and "Nref". We also need to put them as point/normals
	  for correct transform to object space (as they are defined as
	  float[3]).
*/
void exporter::export_attributes(
	std::vector<std::string> &io_which_ones,
	const GT_Primitive &i_primitive,
	double i_time,
	GT_DataArrayHandle i_vertices_list) const
{
	/* Get the vertices list if it's provided */
	GT_DataArrayHandle buffer_in_case_we_need_it;
	const int *vertices = nullptr;
	if( i_vertices_list )
	{
		vertices = i_vertices_list->getI32Array( buffer_in_case_we_need_it );
	}

	// Retrieve a context that might redirect the attributes to a shared file
	NSI::Context& nsi = static_attributes_context();

	if(nsi.Handle() == NSI_BAD_CONTEXT)
	{
		// Those attributes have already been exported in a previous frame
		return;
	}

	for(int w = io_which_ones.size()-1; w >= 0; w--)
	{
		std::string name = io_which_ones[w];

		GT_DataArrayHandle data;
		NSIType_t nsi_type = NSITypeInvalid;
		int nsi_flags = 0;
		bool point_attribute = false;

		if(!find_attribute(
				i_primitive,
				name,
				data,
				nsi_type,
				nsi_flags,
				point_attribute))
		{
			continue;
		}

		io_which_ones.erase(io_which_ones.begin()+w);

		if( name == "uv" )
			name = "st";

		if( name == "rest" )
		{
			name = "Pref";
			nsi_type = NSITypePoint;
		}

		if( name == "rnml" )
		{
			name = "Nref";
			nsi_type = NSITypeNormal;
		}

		if( name == "pscale" )
		{
			name = "width";
		}

		if( point_attribute && i_vertices_list )
		{
			nsi.SetAttribute( m_handle,
				*NSI::Argument( name + ".indices" )
					.SetType( NSITypeInteger )
					->SetCount( i_vertices_list->entries() )
					->SetValuePointer( vertices ) );
		}

		GT_DataArrayHandle buffer_in_case_we_need_it_2;
		if( data->getTypeInfo() == GT_TYPE_TEXTURE )
		{
			nsi.SetAttributeAtTime( m_handle, i_time,
				*NSI::Argument(name)
					.SetArrayType( NSITypeFloat, 3)
					->SetCount( data->entries() )
					->SetValuePointer(
						data->getF32Array(buffer_in_case_we_need_it_2))
					->SetFlags(nsi_flags));
			continue;
		}

		const void* nsi_data = nullptr;
		switch(nsi_type)
		{
			case NSITypeInteger:
				nsi_data = data->getI32Array(buffer_in_case_we_need_it_2);
				break;
			case NSITypeDouble:
			case NSITypeDoubleMatrix:
				nsi_data = data->getF64Array(buffer_in_case_we_need_it_2);
				break;
			default:
				nsi_data = data->getF32Array(buffer_in_case_we_need_it_2);
		}

		if( nsi_type == NSITypeFloat && data->getTupleSize()>1 )
		{
			nsi.SetAttributeAtTime( m_handle, i_time,
				*NSI::Argument(name)
					.SetArrayType( nsi_type, data->getTupleSize() )
					->SetCount( data->entries() )
					->SetValuePointer( nsi_data )
					->SetFlags(nsi_flags));
		}
		else
		{
			if( name == "width" )
			{
				/* To match Houdini/Mantra */
				float *fdata = (float*)nsi_data;
				for( int i=0; i<data->entries(); i++ )
				{
					fdata[i] *= 2.0f;
				}
			}

			nsi.SetAttributeAtTime( m_handle, i_time,
				*NSI::Argument(name)
					.SetType( nsi_type )
					->SetCount( data->entries() )
					->SetValuePointer( nsi_data )
					->SetFlags(nsi_flags));
		}
	}

	return; // so that we don't fall into the void.
}

void exporter::export_override_attributes() const
{
	if ( !m_object->hasParm("_3dl_spatial_override") )
	{
		// We presume that our other attributes are not present
		return;
	}

	std::string	override_nsi_handle = m_handle;
	override_nsi_handle += "|attributeOverrides";

	m_nsi.Create( override_nsi_handle, "attributes" );

	NSI::ArgumentList arguments;

	float time = m_context.m_current_time;

	// Visible to Camera override
	if ( m_object->hasParm("_3dl_override_visibility_camera_enable") )
	{
		bool over_vis_camera_enable =
			m_object->evalInt("_3dl_override_visibility_camera_enable", 0, time);

		if( over_vis_camera_enable )
		{
			bool vis_camera_over =
				m_object->evalInt("_3dl_override_visibility_camera", 0, time);

			arguments.Add(
				new NSI::IntegerArg("visibility.camera", vis_camera_over));
			arguments.Add(
				new NSI::IntegerArg("visibility.camera.priority", 10));
		}
		else
		{
			m_nsi.DeleteAttribute( override_nsi_handle, "visibility.camera" );
			m_nsi.DeleteAttribute(
				override_nsi_handle, "visibility.camera.priority" );
		}
	}

	// Visible in Diffuse override
	if ( m_object->hasParm("_3dl_override_visibility_diffuse_enable") )
	{
		bool over_vis_diffuse_enable =
			m_object->evalInt("_3dl_override_visibility_diffuse_enable", 0, time);

		if( over_vis_diffuse_enable )
		{
			bool vis_diffuse_over = false;
				m_object->evalInt("_3dl_override_visibility_diffuse", 0, time);

			arguments.Add(
				new NSI::IntegerArg("visibility.diffuse", vis_diffuse_over));
			arguments.Add(
				new NSI::IntegerArg("visibility.diffuse.priority", 10));
		}
		else
		{
			m_nsi.DeleteAttribute(
				override_nsi_handle, "visibility.diffuse" );
			m_nsi.DeleteAttribute(
				override_nsi_handle, "visibility.diffuse.priority" );
		}
	}

	// Visible in Reflections override
	if ( m_object->hasParm("_3dl_override_visibility_reflection_enable") )
	{
		bool over_vis_reflection_enable =
			m_object->evalInt(
				"_3dl_override_visibility_reflection_enable", 0, time);

		if( over_vis_reflection_enable )
		{
			bool over_vis_reflection =
				m_object->evalInt("_3dl_override_visibility_reflection", 0, time);

			arguments.Add(
				new NSI::IntegerArg("visibility.reflection", over_vis_reflection));
			arguments.Add(
				new NSI::IntegerArg("visibility.reflection.priority", 10));
		}
		else
		{
			m_nsi.DeleteAttribute(
				override_nsi_handle, "visibility.reflection" );
			m_nsi.DeleteAttribute(
				override_nsi_handle, "visibility.reflection.priority" );
		}
	}

	// Visible in Refractions override
	if( m_object->hasParm("_3dl_override_visibility_refraction_enable") )
	{
		bool over_vis_refraction_enable =
			m_object->evalInt("_3dl_override_visibility_refraction_enable", 0, time);

		if( over_vis_refraction_enable )
		{
			bool over_vis_refraction =
				m_object->evalInt("_3dl_override_visibility_refraction", 0, time);

			arguments.Add(
				new NSI::IntegerArg("visibility.refraction", over_vis_refraction));
			arguments.Add(
				new NSI::IntegerArg("visibility.refraction.priority", 10));
		}
		else
		{
			m_nsi.DeleteAttribute(
				override_nsi_handle, "visibility.refraction" );
			m_nsi.DeleteAttribute(
				override_nsi_handle, "visibility.refraction.priority" );
		}
	}

	// Compositing mode override
	if ( m_object->hasParm("_3dl_override_compositing_enable") )
	{
		bool over_compositing_enable =
			m_object->evalInt("_3dl_override_compositing_enable", 0, time);

		if( over_compositing_enable )
		{
			UT_String override_compositing;
			m_object->evalString(
				override_compositing, "_3dl_override_compositing", 0, time);

			bool matte = override_compositing == "matte";
			bool prelit = override_compositing == "prelit";

			arguments.Add( new NSI::IntegerArg("matte", matte));
			arguments.Add( new NSI::IntegerArg("matte.priority", 10));
			arguments.Add( new NSI::IntegerArg("prelit", prelit));
			arguments.Add( new NSI::IntegerArg("prelit.priority", 10));
		}
		else
		{
			m_nsi.DeleteAttribute( override_nsi_handle, "matte" );
			m_nsi.DeleteAttribute( override_nsi_handle, "matte.priority" );
			m_nsi.DeleteAttribute( override_nsi_handle, "prelit" );
			m_nsi.DeleteAttribute( override_nsi_handle, "prelit.priority" );
		}
	}

	// Set all override attributes defined above
	if( arguments.size() > 0 )
	{
		m_nsi.SetAttribute( override_nsi_handle, arguments );
	}

	/*
		Adjust connections to make the override effective or not according to
		its	main override toggle.

		Note that the surface shader overrides doesn't need an [x] Enable
		toggle.
	*/
	bool spatial_override = m_object->evalInt("_3dl_spatial_override", 0, time);
	if( spatial_override )
	{
		m_nsi.Connect( m_handle, "",
			override_nsi_handle, "bounds" );
		m_nsi.Connect( override_nsi_handle, "",
			NSI_SCENE_ROOT, "geometryattributes" );

		m_nsi.Connect(
			transparent_surface_handle(), "",
			m_handle, "geometryattributes" );

		bool override_surface_shader = m_object->evalInt(
			"_3dl_override_surface_shader", 0, time);

		if( override_surface_shader )
		{
			NSI::ArgumentList arguments;
			arguments.Add(new NSI::IntegerArg("priority", 10));

			std::string material;
			get_assigned_material( material );

			m_nsi.Connect(
				material, "", override_nsi_handle, "surfaceshader", arguments );
		}
	}
	else
	{
		/* NOTE: This is needed for IPR only */
		m_nsi.Disconnect( m_handle, "",
			override_nsi_handle, "bounds" );
		m_nsi.Disconnect( override_nsi_handle, "",
			NSI_SCENE_ROOT, "geometryattributes" );
	}
}

void exporter::export_bind_attributes(
	const GT_Primitive &i_primitive,
	GT_DataArrayHandle i_vertices_list ) const
{
	std::vector< std::string > binds;
	get_bind_attributes( binds );

	std::sort( binds.begin(), binds.end() );
	binds.erase( std::unique(binds.begin(), binds.end()), binds.end() );

	/*
		Remove attributes that are exported anyway.
	*/
	binds.erase(
		std::remove_if(
			binds.begin(),
			binds.end(),
			[](const std::string &a)
			{
				return a == "P" || a == "N" || a == "uv" || a == "width" ||
				a == "id" || a == "pscale" || a == "rest" || a == "rnml";
			} ),
		binds.end() );

	export_attributes(
		binds, i_primitive, m_context.m_current_time, i_vertices_list );
}

VOP_Node *exporter::get_assigned_material( std::string &o_path ) const
{
	int index = m_object->getParmIndex( "shop_materialpath" );
	if( index < 0 )
		return nullptr;

	std::string attributes( m_handle + "|attributes" );
	UT_String material_path;
	m_object->evalString( material_path, "shop_materialpath", 0, 0.f );

	if( material_path.length()==0 )
	{
		return nullptr;
	}

	return resolve_material_path( material_path.c_str(), o_path );
}

VOP_Node *exporter::resolve_material_path(
	const char *i_path,  std::string &o_path ) const
{
	OP_Node* op_node = OPgetDirector()->findNode( i_path );
	VOP_Node *vop_node = op_node->castToVOPNode();

	if( !vop_node )
	{
		/* Try a relative search */
		vop_node = m_object->findVOPNode( i_path );
		if( !vop_node )
		{
			return nullptr;
		}
	}

	o_path = vop_node->getFullPath().toStdString();
	return vop_node;
}

void exporter::get_bind_attributes(
	std::vector< std::string > &o_to_export ) const
{
	std::string dummy;
	VOP_Node *vop = get_assigned_material( dummy );

	if( !vop )
		return;

	std::vector<VOP_Node*> aov_export_nodes;

	std::vector<OP_Node*> traversal; traversal.push_back( vop );
	while( traversal.size() )
	{
		OP_Node* node = traversal.back();
		traversal.pop_back();

		int ninputs = node->nInputs();
		for( int i = 0; i < ninputs; i++ )
		{
			VOP_Node *input = CAST_VOPNODE( node->getInput(i) );
			if( !input )
				continue;

			OP_Operator* op = input->getOperator();
			if( !op )
				continue;

			if( op->getName().toStdString() == "3Delight::dlPrimitiveAttribute" ||
				op->getName().toStdString() == "3Delight::dlAttributeRead")
			{
				UT_String primvar;
				input->evalString(
					primvar, "attribute_name", 0, m_context.m_current_time );
				o_to_export.push_back( primvar.toStdString() );
			}
			else
			{
				traversal.push_back( input );
			}
		}
	}
}

NSI::Context&
exporter::static_attributes_context(
	time_sampler::blur_source i_type)const
{
	bool animated =
		time_sampler::is_time_dependent(
			*m_object,
			m_context.m_current_time,
			i_type);
	return animated ? m_context.m_nsi : m_context.m_static_nsi;
}
