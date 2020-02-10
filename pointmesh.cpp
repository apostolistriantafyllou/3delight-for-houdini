#include "pointmesh.h"

#include "context.h"
#include "time_sampler.h"

#include <GT/GT_PrimPointMesh.h>
#include <OBJ/OBJ_Node.h>
#include <nsi.hpp>

pointmesh::pointmesh(
	const context& i_ctx,
	OBJ_Node *i_object,
	double i_time,
	const GT_PrimitiveHandle &i_gt_primitive,
	unsigned i_primitive_index )
:
	primitive(
		i_ctx,
		i_object,
		i_time,
		i_gt_primitive,
		i_primitive_index,
		has_velocity(i_gt_primitive))
{
}

void pointmesh::create( void ) const
{
	m_nsi.Create( m_handle.c_str(), "particles" );
}

void pointmesh::set_attributes( void ) const
{
	exporter::export_bind_attributes( *default_gt_primitive().get() );

	/*
		If the point mesh is not motion-blurred (ie : it needs only one time
		sample or if it has no velocity attribute) we simply output the
		attributes' values at each time sample. When there is no motion blur,
		it's simpler this way. However, it's always preferable to implement
		motion blur using the velocity attribute, in Houdini, because attributes
		tend to be corrupted when particles are removed between two samples :
		the "id" attribute seems updated correctly, but the other attributes
		(such as "P") have funky values.
	*/
	if(!requires_frame_aligned_sample() ||
		time_sampler(
			m_context,
			*m_object, time_sampler::e_deformation).nb_samples() <= 1 ||
		!export_extrapolated_P())
	{
		// Export attributes at each time sample
		primitive::set_attributes();
		return;
	}

	// Export particle width
	export_basic_attributes(
		m_context.m_current_time, default_gt_primitive(),
		true /* width ony */ );
}

void pointmesh::set_attributes_at_time(
	double i_time,
	const GT_PrimitiveHandle i_gt_primitive) const
{
	export_basic_attributes(i_time, i_gt_primitive, false);
}

void pointmesh::export_basic_attributes(
	double i_time,
	const GT_PrimitiveHandle i_gt_primitive,
	bool i_width_only)const
{
	std::vector< std::string > to_export;
	if(!i_width_only)
	{
		to_export.push_back("P");
		to_export.push_back("id");
	}

	/*
		We ask for both, but only one of them should be set by users.
	*/
	to_export.push_back("width");
	to_export.push_back("pscale");

	export_attributes( to_export, *i_gt_primitive.get(), i_time );

	if( std::find(to_export.begin(),to_export.end(),"width")!=to_export.end() &&
		std::find(to_export.begin(),to_export.end(),"pcale")!=to_export.end() )

	{
		// "width" not in attribute list. default to something.
		m_nsi.SetAttributeAtTime(
			m_handle, i_time, NSI::FloatArg("width", 0.1f) );
	}
}
