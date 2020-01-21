#include "instance.h"

#include "context.h"

#include <nsi.hpp>

#include <GT/GT_PrimInstance.h>
#include <OBJ/OBJ_Node.h>

instance::instance(
	const context& i_ctx,
	OBJ_Node *i_object,
	double i_time,
	const GT_PrimitiveHandle &i_gt_primitive,
	unsigned i_primitive_index,
	const std::string &i_geometry_handle )
:
	primitive( i_ctx, i_object, i_time, i_gt_primitive, i_primitive_index ),
	m_geometry_handle(i_geometry_handle)
{
}

void instance::create( void ) const
{
	m_nsi.Create( m_handle.c_str(), "instances" );
}

void instance::connect( void ) const
{
	primitive::connect();

	m_nsi.Connect( m_geometry_handle, "", m_handle, "sourcemodels" );
}

void instance::set_attributes( void ) const
{
	const GT_PrimInstance *instance =
		static_cast<const GT_PrimInstance *>(default_gt_primitive().get());
	std::vector< std::string > to_export{ "Cd" };
	exporter::export_attributes(
		to_export, *instance, m_context.m_current_time, GT_DataArrayHandle() );

	primitive::set_attributes();
}

void instance::set_attributes_at_time(
	double i_time,
	const GT_PrimitiveHandle i_gt_primitive) const
{
	const GT_PrimInstance *instance =
		static_cast<const GT_PrimInstance *>(i_gt_primitive.get());

	const GT_TransformArrayHandle transforms = instance->transforms();

	double *matrices = new double[ transforms->entries() * 16 ];
	double *it = matrices;

	static_assert( sizeof(UT_Matrix4D) == 16*sizeof(double), "check 4D matrix" );

	for( int i=0; i<transforms->entries(); i++ )
	{
		const GT_TransformHandle &handle = transforms->get(i);
		UT_Matrix4D matrix;
		handle->getMatrix( matrix );
		memcpy( it, matrix.data(), sizeof(UT_Matrix4D) );
		it += 16;
	}

	NSI::ArgumentList args;

	args.Add( NSI::Argument::New( "transformationmatrices" )
			->SetType( NSITypeDoubleMatrix )
			->SetCount( transforms->entries() )
			->SetValuePointer(matrices) );

	m_nsi.SetAttributeAtTime( m_handle.c_str(), i_time, args );

	delete [] matrices;
}
