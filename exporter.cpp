#include "exporter.h"

#include "context.h"

#include <GT/GT_Handles.h>
#include <OBJ/OBJ_Node.h>

#include <iostream>

GT_PrimitiveHandle exporter::sm_invalid_gt_primitive;

static int s_handle = 0;
exporter::exporter(
	const context& i_context, OBJ_Node *i_object,
	const GT_PrimitiveHandle &i_gt_primitive )
:
	m_context(i_context),
	m_nsi(i_context.m_nsi),
	m_object(i_object),
	m_gt_primitive(i_gt_primitive)
{
	/*
		Geometry uses its full path + a prefix as a handle. So that
		it leaves the full path handle to the parent transform.
	*/
	m_handle = i_object->getFullPath();
	m_handle += "|object|";
	m_handle += std::to_string( s_handle ++ );
}

exporter::exporter( const context &i_context, VOP_Node *i_node )
:
	m_context(i_context),
	m_nsi(i_context.m_nsi),
	m_vop(i_node)
{
}

const std::string &exporter::handle( void ) const
{
	return m_handle;
}

/**
	\brief Connect to the parent object of this exporter and also
	perform local material assignment if this op node has a
	"shop_materialpath" parameter.
*/
void exporter::connect( void ) const
{
	if( !m_object )
		return;

	std::string parent;

	if( m_instanced )
	{
		/**
			This geometry will be connected to the "sourcemodels" of an NSI
			instances node and doesn't need to be connected to any other
			parent.

			\ref instancer::connect
		*/
		goto assign_material; // one goto for assembly gods.
	}

	/*
	   We support the transformation of the GT_Primitive by inserting
	   a local NSI transform node between the object and its parent.
	*/
	if( m_gt_primitive != sm_invalid_gt_primitive )
	{
		parent = m_handle + "|transform";
		const GT_TransformHandle &transform =
			m_gt_primitive->getPrimitiveTransform();
		UT_Matrix4D matrix;
		transform->getMatrix( matrix );

		m_nsi.Create( parent, "transform" );
		m_nsi.SetAttribute( parent,
				NSI::DoubleMatrixArg( "transformationmatrix", matrix.data() ) );
		m_nsi.Connect( parent, "", m_object->getFullPath().c_str(), "objects" );
	}
	else
	{
		parent = m_object->getFullPath().c_str();
	}

	m_nsi.Connect( m_handle, "", parent, "objects" );


assign_material:
	/* Do local material assignment */
	int index = m_object->getParmIndex( "shop_materialpath" );
	if( index < 0 )
		return;

	std::string attributes( m_handle + "|attributes" );
	UT_String material_path;
	m_object->evalString( material_path, "shop_materialpath", 0, 0.f );

	if( material_path.length()==0 )
	{
		return;
	}

	m_nsi.Create( attributes, "attributes" );
	m_nsi.Connect( attributes, "", m_handle, "geometryattributes" );
	m_nsi.Connect( material_path.buffer(), "", attributes, "surfaceshader" );
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

/**
	gotchas:
	- When we find a texture coordinate attribute, we output "st".
	  regardless of the name we find (usually "uv"). This is to be
	  consistant with other packages.  After the first one we add a
	  number.
*/
void exporter::export_attributes(
	const GT_AttributeListHandle *i_attributes,
	int i_n, double i_time,
	std::vector<const char *> &io_which_ones ) const
{
	for( int i=0; i<i_n; i++ )
	{
		auto it = io_which_ones.begin();
		while( it != io_which_ones.end() )
		{
			const char *name = *it;

			const GT_DataArrayHandle &data = i_attributes[i]->get( name );
			if( data.get() == nullptr )
			{
				/* Name not part of this attributes list */
				it ++;
				continue;
			}

			NSIType_t nsi_type = gt_to_nsi_type( data->getTypeInfo() );
			if( nsi_type == NSITypeInvalid )
			{
				std::cout << "unsupported attribute type " << data->getTypeInfo()
					<< " of name " << name << " on " << m_handle;
				it ++ ;
				continue;
			}

			it = io_which_ones.erase( it );
			GT_DataArrayHandle buffer_in_case_we_need_it;

			if( data->getTypeInfo() == GT_TYPE_TEXTURE )
			{
				m_nsi.SetAttributeAtTime( m_handle, i_time,
					*NSI::Argument("st")
						.SetArrayType( NSITypeFloat, 3)
						->SetCount( data->entries() )
						->SetValuePointer(
							data->getF32Array(buffer_in_case_we_need_it)));
				continue;
			}

			m_nsi.SetAttributeAtTime( m_handle, i_time,
				*NSI::Argument(name)
					.SetType( nsi_type )
					->SetCount( data->entries() )
					->SetValuePointer( data->getF32Array(buffer_in_case_we_need_it)));
		}
	}

	return; // so that we don't fall into the void.
}
