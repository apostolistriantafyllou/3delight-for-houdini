#include "instance.h"

#include "context.h"

#include <nsi.hpp>

#include <GT/GT_PrimInstance.h>
#include <OBJ/OBJ_Node.h>
#include <OP/OP_Operator.h>

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
		We need a parent transform (which we call a merge point) in case there
		are many primitives to instantiate.

		We also use the merge point to enable a different shader per instance.
		In NSI-speak, each <material, sourcemodel> pair will become a
		"sourcemodel" that we can select using sourcemodels plug of the
		instance node.
	*/
	std::vector< std::string > materials;
	std::vector< int > sourcemodel_index_per;
	get_sop_materials( materials  );

	if( materials.size() == 1 )
	{
		/* Assign material to the entire instancer */
		std::string attribute_handle = m_handle + "|attribute";
		m_nsi.Create( attribute_handle, "attributes" );
		m_nsi.Connect( attribute_handle, "", m_handle, "geometryattributes");
		m_nsi.Connect( materials[0], "", attribute_handle, "surfaceshader",
			NSI::IntegerArg("priority", 2) );
		materials.clear();
	}

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

/**
	For the instance, we simply output all the attributes that are attached.
	These attributes will be propagated by 3Delight to the instanced geometry.
	This means that vertex/point/uniform/detail attributes on the intancer will
	become indiviual *detail* attribute on the instances.
*/
void instance::set_attributes( void ) const
{
	const GT_PrimInstance *instance =
		static_cast<const GT_PrimInstance *>(default_gt_primitive().get());

	std::vector< std::string > to_export;

	for( int i=GT_OWNER_VERTEX; i<=GT_OWNER_DETAIL; i++ )
	{
		const GT_AttributeListHandle& attributes =
			instance->getAttributeList( GT_Owner(i) );

		if( !attributes )
			continue;

		UT_StringArray names = attributes->getNames();

		for( int j=0; j<names.entries(); j++ )
		{
			std::string name = names[j].toStdString();

			/* Yes, O(N) complexity but doen't matter here */
			if( std::find(to_export.begin(), to_export.end(), name) ==
				to_export.end() )
			{
				to_export.push_back( name );
			}
		}
	}

	/* Remove useless attributes. */
	to_export.erase( std::remove_if(
		to_export.begin(), to_export.end(), [](const std::string &a)
		{ return a == "P"; }), to_export.end() );

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

	double *matrices = nullptr;
	int num_matrices = 0;

	const UT_StringRef &op_name = m_object->getOperator()->getName();
	if( op_name == "instance" )
	{
		int mode = 1;
		if( m_object->getParmIndex("ptinstance")>=0  )
			mode = m_object->evalInt("ptinstance", 0, m_context.m_current_time);

		if( mode == 0 )
		{
			/* Intancing off (a totally needless parameter btw) */
			double *M = matrices = new double[16];
			//double *M = matrices;
			*M++ = 1.0; *M++= 0.0; *M++ = 0.0;  *M++ = 0.0;
			*M++ = 0.0; *M++= 1.0; *M++ = 0.0;  *M++ = 0.0;
			*M++ = 0.0; *M++= 0.0; *M++ = 1.0;  *M++ = 0.0;
			*M++ = 0.0; *M++= 0.0; *M++ = 0.0;  *M++ = 1.0;
			num_matrices = 1;
		}
		else
		{
			GT_Owner type;
			GT_DataArrayHandle P =
				i_gt_primitive->findAttribute( "P", type, 0 /* segment */ );

			matrices = new double[P->entries() * 16 ];

			GT_DataArrayHandle buffer_in_case_we_need_it;
			const float *pos = P->getF32Array( buffer_in_case_we_need_it );

			double *M = matrices;
			for( int i=0; i<P->entries(); i++, pos+=3 )
			{
				*M++ = 1.0; *M++= 0.0; *M++ = 0.0;  *M++ = 0.0;
				*M++ = 0.0; *M++= 1.0; *M++ = 0.0;  *M++ = 0.0;
				*M++ = 0.0; *M++= 0.0; *M++ = 1.0;  *M++ = 0.0;
				*M++=pos[0]; *M++=pos[1]; *M++=pos[2]; *M++=1.0;
			}
			num_matrices = P->entries();
		}

		assert( num_matrices > 0 );
	}
	else
	{
		const GT_PrimInstance *instance =
			static_cast<const GT_PrimInstance *>(i_gt_primitive.get());

		assert( instance );

		const GT_TransformArrayHandle transforms = instance->transforms();
		matrices = new double[ transforms->entries() * 16 ];
		double *it = matrices;

		static_assert(
			sizeof(UT_Matrix4D) == 16*sizeof(double), "check 4D matrix" );

		for( int i=0; i<transforms->entries(); i++ )
		{
			const GT_TransformHandle &handle = transforms->get(i);
			UT_Matrix4D matrix;
			handle->getMatrix( matrix );
			memcpy( it, matrix.data(), sizeof(UT_Matrix4D) );
			it += 16;
		}
		num_matrices = transforms->entries();
	}

	NSI::ArgumentList args;

	args.Add( NSI::Argument::New( "transformationmatrices" )
			->SetType( NSITypeDoubleMatrix )
			->SetCount( num_matrices )
			->SetValuePointer(matrices) );

	nsi.SetAttributeAtTime( m_handle.c_str(), i_time, args );

	delete [] matrices;
}

std::string instance::merge_handle( const std::string &i_material ) const
{
	return m_handle + i_material + "|merge";
}
