#include "pointmesh.h"

#include "context.h"
#include "time_sampler.h"

#include <GT/GT_PrimPointMesh.h>
#include <OBJ/OBJ_Node.h>
#include <nsi.hpp>

const std::string k_position_attribute = "P";
const std::string k_velocity_attribute = "v";

pointmesh::pointmesh(
	const context& i_ctx,
	OBJ_Node *i_object,
	const GT_PrimitiveHandle &i_gt_primitive,
	unsigned i_primitive_index )
:
	primitive( i_ctx, i_object, i_gt_primitive, i_primitive_index )
{
}

void pointmesh::create( void ) const
{
	m_nsi.Create( m_handle.c_str(), "particles" );
}

void pointmesh::set_attributes( void ) const
{
	exporter::export_bind_attributes( *default_gt_primitive().get() );

	time_sampler t(m_context, *m_object, time_sampler::e_deformation);

	/*
		If the point mesh is not motion-blurred (ie : it has only one time
		sample or if it has no velocity attribute) we simply output the
		attributes' values at each time sample. When there is no motion blur,
		it's simpler this way. However, it's always preferable to implement
		motion blur using the velocity attribute, in Houdini, because attributes
		tend to be corrupted when particles are removed between two samples :
		the "id" attribute seems updated correctly, but the other attributes
		(such as "P") have funky values.
	*/
	GT_DataArrayHandle velocity_data;
	NSIType_t velocity_nsi_type;
	int velocity_nsi_flags;
	bool velocity_point_attribute;
	if(t.nb_samples() <= 1 ||
		!find_attribute(
			*default_gt_primitive().get(),
			k_velocity_attribute,
			velocity_data,
			velocity_nsi_type,
			velocity_nsi_flags,
			velocity_point_attribute) ||
		velocity_nsi_type != NSITypeVector)
	{
		// Export attributes at each time sample
		primitive::set_attributes();
		return;
	}

	unsigned nb_points = velocity_data->entries();

	// Retrieve position data handle
	GT_DataArrayHandle position_data;
	NSIType_t position_nsi_type;
	int position_nsi_flags;
	bool position_point_attribute;
	if(!find_attribute(
			*default_gt_primitive().get(),
			k_position_attribute,
			position_data,
			position_nsi_type,
			position_nsi_flags,
			position_point_attribute) ||
		position_nsi_type != NSITypePoint ||
		position_data->entries() != nb_points)
	{
		// This is very weird, most likely impossible
		assert(false);
		return;
	}

	/*
		Generate 2 samples.  The first is the actual first sample, and the
		second is computed from the first sample and the velocity attribute.
		It's an asymmetric solution, in the sense that the first sample contains
		accurate data while the second is only an approximation, but it's still
		better than using two "accurate" samples from Houdini. The ideal
		solution would probably have been extrapolating 2 approximation on
		either side of accurate data sampled from the middle of the shutter
		interval, but it would be a bit complicated with our actual GT
		refinement setup.
	*/

	float time = default_time();

	// Retrieve position data in a writable buffer
	float* nsi_position_data = new float[nb_points*3];
	position_data->fillArray(nsi_position_data, 0, nb_points, 3);

	// Output first time sample's position
	m_nsi.SetAttributeAtTime(
		m_handle,
		time,
		*NSI::Argument("P")
			.SetType(position_nsi_type)
			->SetCount(nb_points)
			->SetValuePointer(nsi_position_data));

	// Retrieve velocity
	GT_DataArrayHandle velocity_buffer;
	const float* nsi_velocity_data = velocity_data->getF32Array(velocity_buffer);
	for(unsigned p = 0; p < 3*nb_points; p++)
	{
		nsi_position_data[p] += nsi_velocity_data[p];
	}

	// Output second time sample's position
	m_nsi.SetAttributeAtTime(
		m_handle,
		time + 1.0,
		*NSI::Argument("P")
			.SetType(position_nsi_type)
			->SetCount(nb_points)
			->SetValuePointer(nsi_position_data));

	delete[] nsi_position_data;

	export_basic_attributes(time, default_gt_primitive(), true);
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
	to_export.push_back("width");

	export_attributes( *i_gt_primitive.get(), i_time, to_export );

	if( std::find(to_export.begin(), to_export.end(), "width")
		!= to_export.end() )
	{
		// "width" not in attribute list. default to something.
		m_nsi.SetAttributeAtTime(
			m_handle, i_time, NSI::FloatArg("width", 0.1f) );
	}
}
