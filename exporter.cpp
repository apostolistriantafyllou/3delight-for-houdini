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
	const GT_Primitive &i_primitive,
	double i_time,
	std::vector<std::string> &io_which_ones ) const
{
	GT_AttributeListHandle attributes[4] =
	{
		i_primitive.getVertexAttributes(),
		i_primitive.getPointAttributes(),
		i_primitive.getUniformAttributes(),
		i_primitive.getDetailAttributes(),
	};

	/* Get the vertex_list if there is  poly/subdiv mesh */
	GT_DataArrayHandle buffer_in_case_we_need_it;
	const int *vertices = nullptr;
	GT_DataArrayHandle vertex_list;
	int type = i_primitive.getPrimitiveType();
	if( type == GT_PRIM_SUBDIVISION_MESH || type == GT_PRIM_POLYGON_MESH )
	{
		const GT_PrimPolygonMesh &poly =
			static_cast<const GT_PrimPolygonMesh&>(i_primitive);

		vertex_list = poly.getVertexList();
		vertices = vertex_list->getI32Array( buffer_in_case_we_need_it );
	}

	for(int w = io_which_ones.size()-1; w >= 0; w--)
	{
		std::string name = io_which_ones[w];

		for( int i=0; i<4; i++ )
		{
			if( !attributes[i] )
			{
				continue;
			}

			const GT_DataArrayHandle &data = attributes[i]->get( name.c_str() );
			if( !data )
			{
				continue;
			}

			NSIType_t nsi_type =
				gt_to_nsi_type( data->getTypeInfo(), data->getStorage() );

			if( nsi_type == NSITypeInvalid || nsi_type == NSITypeString )
			{
				std::cerr
					<< "unsupported attribute type " << data->getTypeInfo()
					<< " of name " << name << " on " << m_handle;
				continue;
			}

			/*
				This flag is needed because of particles. If the number of
				particles varies from one time step to the next, 3Delight might
				not be able to guess that those attributes are assigned
				per-vertex simply by looking at their count.
			*/
			int flags = i==0 ? NSIParamPerVertex : 0;

			io_which_ones.erase(io_which_ones.begin()+w);

			if( name == "uv" )
				name = "st";

			bool point_attributes = i==1;
			if( point_attributes && vertex_list )
			{
				m_nsi.SetAttribute( m_handle,
					*NSI::Argument( name + ".indices" )
						.SetType( NSITypeInteger )
						->SetCount( vertex_list->entries() )
						->SetValuePointer( vertices ) );
			}

			GT_DataArrayHandle buffer_in_case_we_need_it_2;
			if( data->getTypeInfo() == GT_TYPE_TEXTURE )
			{
				m_nsi.SetAttributeAtTime( m_handle, i_time,
					*NSI::Argument(name)
						.SetArrayType( NSITypeFloat, 3)
						->SetCount( data->entries() )
						->SetValuePointer(
							data->getF32Array(buffer_in_case_we_need_it_2))
						->SetFlags(flags));
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
				m_nsi.SetAttributeAtTime( m_handle, i_time,
					*NSI::Argument(name)
						.SetArrayType( nsi_type, data->getTupleSize() )
						->SetCount( data->entries() )
						->SetValuePointer( nsi_data )
						->SetFlags(flags));
			}
			else
			{
				m_nsi.SetAttributeAtTime( m_handle, i_time,
					*NSI::Argument(name)
						.SetType( nsi_type )
						->SetCount( data->entries() )
						->SetValuePointer( nsi_data )
						->SetFlags(flags));
			}

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

void exporter::export_bind_attributes(
	const GT_Primitive &i_primitive ) const
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
				a == "id" || a == "pscale";
			} ),
		binds.end() );

	export_attributes( i_primitive, m_context.m_current_time, binds );
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

	OP_Node* op_node = OPgetDirector()->findNode( material_path );
	VOP_Node *vop_node = op_node->castToVOPNode();

	if( !vop_node )
		return nullptr;

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

#if 0
			/* bind is crazy, I don't want tp support that */
			if( op->getName().toStdString() == "bind" &&
				input->evalInt(
					"useasparmdefiner", 0, m_context.m_current_time) == 0 )
			{
				UT_String primvar;
				input->evalString(
					primvar, "parmname", 0, m_context.m_current_time );
				o_to_export.push_back( primvar.toStdString() );
			}
			else
#endif
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
