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
