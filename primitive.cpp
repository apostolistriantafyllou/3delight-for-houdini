#include "primitive.h"

#include "time_sampler.h"

#include <nsi.hpp>

#include <OBJ/OBJ_Node.h>

primitive::primitive(
	const context& i_context,
	OBJ_Node* i_object,
	double i_time,
	const GT_PrimitiveHandle& i_gt_primitive,
	unsigned i_primitive_index)
	:	exporter(i_context, i_object)
{
	add_time_sample(i_time, i_gt_primitive);

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
	for(const TimedPrimitive& prim : m_gt_primitives)
	{
		set_attributes_at_time(prim.first, prim.second);
	}
}

bool primitive::add_time_sample(
	double i_time,
	const GT_PrimitiveHandle& i_primitive)
{
	if(!m_gt_primitives.empty() &&
		m_gt_primitives.front().second->getPrimitiveType() !=
			i_primitive->getPrimitiveType())
	{
		// All GT primitives must be of the same type. This is an error.
		return false;
	}

	bool frame_aligned = requires_frame_aligned_sample();

	if(i_time != m_context.m_current_time && frame_aligned)
	{
		// We simply don't need this time sample. Not an error.
		return true;
	}

	if(!m_gt_primitives.empty() && i_time < m_gt_primitives.back().first)
	{
		/*
			This sample arrived out of chronological order. This is supposed to
			happen only when creating an additional time sample for primitives
			that require one that is aligned on a frame. It can be safely
			ignored by the others.
		*/
		assert(i_time == m_context.m_current_time);
		if(!frame_aligned)
		{
			return true;
		}
	}

	m_gt_primitives.push_back(TimedPrimitive(i_time, i_primitive));

	return true;
}

bool primitive::is_volume()const
{
	return false;
}

bool primitive::requires_frame_aligned_sample()const
{
	return false;
}
