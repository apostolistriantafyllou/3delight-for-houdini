#include "primitive.h"

#include <nsi.hpp>

#include <OBJ/OBJ_Node.h>

static int s_handle = 0;
primitive::primitive(
	const context& i_context,
	OBJ_Node* i_object,
	const GT_PrimitiveHandle& i_gt_primitive)
	:	exporter(i_context, i_object),
		m_gt_primitive(i_gt_primitive)
{
	/*
		Geometry uses its full path + a prefix as a handle. So that
		it leaves the full path handle to the parent transform.
	*/
	m_handle += "|object|" + std::to_string( s_handle ++ );
}

void
primitive::connect()const
{
	assert(m_object);

	if( m_instanced )
	{
		/**
			This geometry will be connected to the "sourcemodels" of an NSI
			instances node and doesn't need to be connected to any other
			parent.
		*/
		return;
	}

	/*
		We support the transformation of the GT_Primitive by inserting
		a local NSI transform node between the object and its parent.
	*/
	std::string parent = m_handle + "|transform";
	const GT_TransformHandle &transform =
		m_gt_primitive->getPrimitiveTransform();
	UT_Matrix4D matrix;
	transform->getMatrix( matrix );

	m_nsi.Create( parent, "transform" );
	m_nsi.SetAttribute( parent,
			NSI::DoubleMatrixArg( "transformationmatrix", matrix.data() ) );
	m_nsi.Connect( parent, "", m_object->getFullPath().c_str(), "objects" );

	m_nsi.Connect( m_handle, "", parent, "objects" );

	export_override_attributes();
}

bool primitive::is_volume()const
{
	return false;
}
