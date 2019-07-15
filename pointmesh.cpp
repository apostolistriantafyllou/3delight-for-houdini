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

void pointmesh::set_attributes_at_time( double i_time ) const
{
	const GT_PrimPointMesh *pointmesh =
		static_cast<const GT_PrimPointMesh *>(m_gt_primitive.get());

	const GT_AttributeListHandle &attributes = pointmesh->getPointAttributes() ;
	const UT_StringArray &names = attributes->getNames();

	for( int i = 0; i<names.entries(); i++ )
	{
		const GT_DataArrayHandle &data = attributes->get( names[i] );
		GT_Type type = data->getTypeInfo();
		NSIType_t nsi_type = NSITypeInvalid;

		switch( type )
		{
			case GT_TYPE_POINT: nsi_type = NSITypePoint; break;
			case GT_TYPE_COLOR: nsi_type = NSITypeColor; break;
			case GT_TYPE_VECTOR: nsi_type = NSITypeVector; break;
			case GT_TYPE_NORMAL: nsi_type = NSITypeNormal; break;
			case GT_TYPE_NONE: nsi_type = NSITypeFloat; break;
			default:
				std::cout << "unsupported attribute type " << type << " of name "
					<< names[i] << " on " << m_handle;
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
