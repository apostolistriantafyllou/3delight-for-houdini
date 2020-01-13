#include "primitive.h"

#include "time_sampler.h"

#include <nsi.hpp>

#include <OBJ/OBJ_Node.h>

primitive::primitive(
	const context& i_context,
	OBJ_Node* i_object,
	const GT_PrimitiveHandle& i_gt_primitive,
	unsigned i_primitive_index)
	:	exporter(i_context, i_object),
		m_gt_primitives(1, i_gt_primitive)
{
	/*
		Geometry uses its full path + a prefix as a handle. So that
		it leaves the full path handle to the parent transform.
	*/
	m_handle += "|object|" + std::to_string(i_primitive_index);
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
		default_gt_primitive()->getPrimitiveTransform();
	UT_Matrix4D matrix;
	transform->getMatrix( matrix );

	m_nsi.Create( parent, "transform" );
	m_nsi.SetAttribute( parent,
			NSI::DoubleMatrixArg( "transformationmatrix", matrix.data() ) );
	m_nsi.Connect( parent, "", m_object->getFullPath().c_str(), "objects" );

	m_nsi.Connect( m_handle, "", parent, "objects" );

	export_override_attributes();
}

void primitive::set_attributes()const
{
	time_sampler t(m_context, *m_object, time_sampler::e_deformation);
	if(t.nb_samples() > m_gt_primitives.size())
	{
		std::cerr
			<< "3Delight for Houdini: missing motion blur samples for "
			<< m_object->getFullPath() << std::endl;
	}

	for(const GT_PrimitiveHandle& prim : m_gt_primitives)
	{
		set_attributes_at_time(*t, prim);
		t++;
	}
}

bool primitive::add_time_sample(const GT_PrimitiveHandle& i_primitive)
{
	if(!m_gt_primitives.empty() &&
		m_gt_primitives[0]->getPrimitiveType() != i_primitive->getPrimitiveType())
	{
		return false;
	}

	m_gt_primitives.push_back(i_primitive);
	return true;
}

bool primitive::is_volume()const
{
	return false;
}
