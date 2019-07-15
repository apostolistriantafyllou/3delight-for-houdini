#include "pointmesh.h"

#include "context.h"

#include <GT/GT_PrimPointMesh.h>
#include <OBJ/OBJ_Node.h>
#include <nsi.hpp>

pointmesh::pointmesh(
	const context& i_ctx,
	OBJ_Node *i_object,
	const GT_PrimitiveHandle &i_gt_primitive )
:
	exporter( i_ctx, i_object, i_gt_primitive )
{
}

void pointmesh::create( void ) const
{
	m_nsi.Create( m_handle.c_str(), "particles" );
}

void pointmesh::set_attributes( void ) const
{
}

NSIType_t gt_to_nsi_type( GT_Type i_type )
{
	switch( i_type)
	{
	case GT_TYPE_POINT: return  NSITypePoint;
	case GT_TYPE_COLOR: return  NSITypeColor;
	case GT_TYPE_VECTOR: return  NSITypeVector;
	case GT_TYPE_NORMAL: return  NSITypeNormal;
	case GT_TYPE_NONE: return  NSITypeFloat;
	default:
		break;
	}
	return NSITypeInvalid;
}

void pointmesh::set_attributes_at_time( double i_time ) const
{
	const GT_PrimPointMesh *pointmesh =
		static_cast<const GT_PrimPointMesh *>(m_gt_primitive.get());

	/*
		NSI doesn't care if it's "vertex" or "uniform". It guess that
		from how many data its given. So just pass through the attributes
		as one list.
	*/
	const GT_AttributeListHandle all_attributes[2] =
	{
		pointmesh->getPointAttributes(),
		pointmesh->getDetailAttributes()
	};

	bool width_found = false;
	for( auto &attributes : all_attributes )
	{
		const UT_StringArray &names = attributes->getNames();
		for( int i = 0; i<names.entries(); i++ )
		{
			width_found = width_found || names[i] == "width";

			const GT_DataArrayHandle &data = attributes->get( names[i] );
			NSIType_t nsi_type = gt_to_nsi_type( data->getTypeInfo() );

			if( nsi_type == NSITypeInvalid )
			{
				std::cout << "unsupported attribute type " << data->getTypeInfo()
					<< " of name " << names[i] << " on " << m_handle;
				continue;
			}
			GT_DataArrayHandle buffer_in_case_we_need_it;
			m_nsi.SetAttributeAtTime( m_handle, i_time,
					*NSI::Argument( names[i].buffer() )
					.SetType( nsi_type )
					->SetCount( data->entries() )
					->SetValuePointer( data->getF32Array(buffer_in_case_we_need_it)));
		}
	}

	if( !width_found )
	{
		m_nsi.SetAttributeAtTime( m_handle, i_time,
			NSI::FloatArg("width", 0.1f) );
	}
}
