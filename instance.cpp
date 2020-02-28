#include "instance.h"

#include "context.h"

#include <nsi.hpp>

#include <GT/GT_PrimInstance.h>
#include <OBJ/OBJ_Node.h>

#include <unordered_map>

instance::instance(
	const context& i_ctx,
	OBJ_Node *i_object,
	double i_time,
	const GT_PrimitiveHandle &i_gt_primitive,
	unsigned i_primitive_index,
	const std::vector<std::string> &i_source_models )
:
	primitive( i_ctx, i_object, i_time, i_gt_primitive, i_primitive_index ),
	m_source_models(i_source_models)
{
}

void instance::create( void ) const
{
	m_nsi.Create( m_handle.c_str(), "instances" );
}

void instance::connect( void ) const
{
	primitive::connect();

	/*
		We need a parent transform in case there are many primitives
		to instantiate.

		We also use the merge point to enable a different shader per instance.
		In NSI-speak, each <material, sourcemodel> pair will become a "model"
		that we can select using sourcemodels plug of the instance node.
	*/
	std::vector< std::string > materials;
	std::vector< int > sourcemodel_index_per;
	get_sop_materials( materials  );

	std::unordered_map<std::string, int> unique_materials;
	for( const auto &m : materials )
	{
		if( unique_materials.find(m) == unique_materials.end() )
			unique_materials[m] = 0;
	}

	/* We need at least one merge point, even without materials. */
	if( unique_materials.empty() )
		unique_materials[std::string("")] = 0;

	auto it = unique_materials.begin();
	int modelindex = 0;

	/*
		Create a merge point for each material, connect source models to it
		and create/assign the correct material for each such merge point.
	*/
	while( it != unique_materials.end() )
	{
		const std::string &material_handle = it->first;
		std::string merge_h = merge_handle( material_handle );

		m_nsi.Create( merge_h, "transform" );
		for( auto sm : m_source_models )
		{
			m_nsi.Connect( sm, "", merge_h, "objects" );
		}

		if( materials.empty() )
		{
			/*
				There are no SOP materials assignments for this instancer.
				Our job is done!
			*/
			m_nsi.Connect( merge_h, "", m_handle, "sourcemodels" );
			assert( unique_materials.size() == 1 );
			return;
		}

		m_nsi.Connect( merge_h, "", m_handle, "sourcemodels",
			NSI::IntegerArg( "index", modelindex ) );

		std::string attribute_handle = merge_h + "|attribute";
		m_nsi.Create( attribute_handle, "attributes" );
		m_nsi.Connect( attribute_handle, "", merge_h,"geometryattributes");
		m_nsi.Connect( material_handle, "", attribute_handle, "surfaceshader",
			NSI::IntegerArg("priority", 2) );

		/* Opportune moment to store the index. */
		it->second = modelindex++;
		++it;
	}

	/*
		For each instance, create the model index depending on which material
		it is attached to. This will allow 3DelightNSI to instantiate the
		correct merge point with the correctly assigned material.
	*/
	std::vector<int> modelindices; modelindices.reserve( materials.size() );
	for( const auto &m : materials )
	{
		modelindices.push_back( unique_materials[m] );
	}

	m_nsi.SetAttribute( m_handle,
		*NSI::Argument("modelindices").SetType(NSITypeInteger)
			->SetCount(modelindices.size())
			->SetValuePointer(&modelindices[0]) );
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
	// Retrieve a context that might redirect the attributes to a shared file
	NSI::Context& nsi = static_attributes_context();
	if(nsi.Handle() == NSI_BAD_CONTEXT)
	{
		// Those attributes have already been exported in a previous frame
		return;
	}

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

	nsi.SetAttributeAtTime( m_handle.c_str(), i_time, args );

	delete [] matrices;
}

std::string instance::merge_handle( const std::string &i_material ) const
{
	return m_handle + i_material + "|merge";
}
