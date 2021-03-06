#include "instance.h"

#include "context.h"
#include "vdb.h"
#include "vop.h"
#include "dl_system.h"

#include <nsi.hpp>

#include <GT/GT_PrimInstance.h>
#include <OBJ/OBJ_Node.h>
#include <OP/OP_Operator.h>
#include <VOP/VOP_Node.h>

#include <unordered_map>

const char *k_file_prefix = "__file:";

/**
	\brief Constructor.

	\param i_ctx
	\param i_object
	\param i_time
	\param i_gt_primitive
	\param i_primitive_index
	\param i_source_models
		NSI handles of the instancer's source models.
*/
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

instance::instance(
	const context& i_ctx,
	OBJ_Node *i_object,
	double i_time,
	const GT_PrimitiveHandle &i_gt_primitive,
	unsigned i_primitive_index )
:
	primitive( i_ctx, i_object, i_time, i_gt_primitive, i_primitive_index )
{
	/*
		OBJ-level instancer. Note that "instancepath" will always
		return "" if there is a s@instance on the geo.
	*/
	UT_String path;
	m_object->evalString(path, "instancepath", 0, m_context.m_current_time );
	OP_Node *instanced = OPgetDirector()->findNode(path);
	if( !instanced )
	{
		instanced = m_object->findNode(path);
	}

	if( instanced )
	{
		m_source_models.push_back( exporter::handle(*instanced, m_context));
	}
}

void instance::create( void ) const
{
	m_nsi.Create( m_handle.c_str(), "instances" );
}

void instance::connect( void ) const
{
	primitive::connect();

	/*
		We need a parent transform (which we call a merge point) for each
		<instance,material> pair.

		In NSI-speak, each <instance, material> pair will become a
		"sourcemodel" that we can select using sourcemodels plug of the
		instance node.
	*/
	std::map<merge_point, int> modelindices;
	std::vector<const merge_point*> merge_points;
	get_merge_points( modelindices, merge_points );

	if( merge_points.empty() )
	{
		std::string merge_h = merge_handle( merge_point() );
		m_nsi.Create( merge_h, "transform" );
		for( const auto &sm : m_source_models )
			m_nsi.Connect(
				sm, "",
				merge_h, "objects",
				NSI::IntegerArg("strength", 1) );
		m_nsi.Connect( merge_h, "", m_handle, "sourcemodels" );
		return;
	}

	auto it = modelindices.begin();
	int modelindex = 0;

	/*
		Create a merge point for each material, connect source models to it
		and create/assign the correct material for each such merge point.
	*/
	while( it != modelindices.end() )
	{
		const auto &merge = it->first;

		std::string merge_h = merge_handle(merge);
		m_nsi.Create( merge_h, "transform" );

		VOP_Node *materials[3] = {nullptr};
		if( merge.second )
		{
			resolve_material_path(
				m_object, merge.second->getFullPath().c_str(), materials );
		}

		const std::string &object = merge.first;
		if( !object.empty() )
		{
			if( object.find(k_file_prefix)==0)
			{
				std::string path = object.substr(::strlen(k_file_prefix));

				/*
					Detect NSI files. We use an intentionally permissive test so
					that slight variations on ".nsi" are accepted.

					Also, avoid loading non-existent NSI files, as this was
					reported to crash on Windows (although this seems suspicious
					since 3Delight should already handle that properly).
				*/
				if( path.find(".nsi") != std::string::npos &&
					dl_system::file_exists(path.c_str()) )
				{
					m_nsi.Create(object, "procedural");
					m_nsi.SetAttribute(
						object,
						(
							NSI::CStringPArg("type", "apistream"),
							NSI::StringArg("filename", path)
						) );
					m_nsi.Connect(object, "", merge_h, "objects");
				}
				// Detect VDB files
				else if(path.size() > 4 && path.substr(path.size()-4) == ".vdb")
				{
					m_nsi.Create(object, "volume");
					NSI::ArgumentList args;
					args.Add(new NSI::StringArg("vdbfilename", path));
					vdb_file::get_attributes_from_material(
						args, path, materials[2], m_context.m_current_time);
					m_nsi.SetAttribute(object, args);
					m_nsi.Connect(object, "", merge_h, "objects");
				}
			}
			else
			{
				OP_Node *node = OPgetDirector()->findOBJNode(object.c_str());
				assert(node);
				if (node)
				{
					m_nsi.Connect(
						exporter::handle(*node, m_context), "",
						merge_h, "objects");
				}
			}
		}
		else
		{
			for( auto sm : m_source_models )
				m_nsi.Connect( sm, "", merge_h, "objects" );
		}

		m_nsi.Connect( merge_h, "", m_handle, "sourcemodels",
			NSI::IntegerArg( "index", modelindex ) );

		std::string attribute_handle = merge_h + "|attribute";
		m_nsi.Create( attribute_handle, "attributes" );
		m_nsi.Connect( attribute_handle, "", merge_h, "geometryattributes");

		const char *names[3] =
			{ "surfaceshader", "displacementshader", "volumeshader" };

		for( int i=0; i<3; i++ )
		{
			VOP_Node *material = materials[i];
			if( !material )
				continue;

			// FIXME: use vop::is_texture
			m_nsi.Connect(
				vop::handle(*material, m_context), "",
				attribute_handle, names[i],
				(
					NSI::IntegerArg("priority", 2),
					NSI::IntegerArg("strength", 1)
				) );
		}

		/* Opportune moment to store the index. */
		it->second = modelindex++;
		++it;
	}

	/*
		For each instance, create the model index depending on which material
		it is attached to. This will allow 3DelightNSI to instantiate the
		correct merge point with the correctly assigned material.
	*/
	int n = num_instances();
	std::vector<int> nsi_modelindices; nsi_modelindices.reserve( n );

	for( int i=0; i<n; i++ )
	{
		if( i<merge_points.size() )
		{
			nsi_modelindices.push_back( modelindices[*merge_points[i]] );
		}
		else
		{
			assert( merge_points.size() == 1 );
			assert( modelindices.size() == 1 );
			nsi_modelindices.push_back(0);
		}
	}

	m_nsi.SetAttribute( m_handle,
		*NSI::Argument("modelindices").SetType(NSITypeInteger)
			->SetCount(nsi_modelindices.size())
			->SetValuePointer(&nsi_modelindices[0]) );
}

/**
	For the instance, we simply output all the attributes that are attached.
	These attributes will be propagated by 3Delight to the instanced geometry.
	This means that vertex/point/uniform/detail attributes on the instancer will
	become individual *detail* attribute on the instances.
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

			/* Yes, O(N) complexity but doesn't matter here */
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
	NSI::Context& nsi = attributes_context();
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
			*M++ = 1.0; *M++= 0.0; *M++ = 0.0;  *M++ = 0.0;
			*M++ = 0.0; *M++= 1.0; *M++ = 0.0;  *M++ = 0.0;
			*M++ = 0.0; *M++= 0.0; *M++ = 1.0;  *M++ = 0.0;
			*M++ = 0.0; *M++= 0.0; *M++ = 0.0;  *M++ = 1.0;
			num_matrices = 1;
		}
		else
		{
			GT_Owner type;
			GT_DataArrayHandle P = i_gt_primitive->findAttribute( "P", type, 0);
			if( P )
			{
				matrices = new double[ P->entries() * 16 ];
				get_transforms( i_gt_primitive, (UT_Matrix4D *) matrices );
				num_matrices = P->entries();
			}
		}

		assert( num_matrices > 0 );
	}
	else
	{
		/**
			At SOP-level, we get a GT_PrimInstance with the matrices provided
			by Houdini. Wouldn't this be nice if it worked the same on the
			OBJ-level ?
		*/
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

	/*
		Add velocity to the matrices if needed. We support per point velocity
		or detail velocity. This works for SOP-level and OBJ-level instancers.
	*/
	GT_DataArrayHandle tmp;
	const float *velocity = nullptr;
	double time = i_time;
	GT_Owner owner;
	GT_DataArrayHandle velocity_data =
		has_velocity_blur() ?
			i_gt_primitive->findAttribute("v", owner, 0) : GT_DataArrayHandle();

	int v_inc = 0;
	if( velocity_data )
	{
		v_inc = velocity_data->entries() == num_matrices ? 3 : 0;
		const float *v = velocity = velocity_data->getF32Array(tmp);
		double velocity_weight = m_context.ShutterOpen() - i_time;
		for (int i = 0; i < num_matrices; i++, v+=v_inc)
		{
			double *trs = matrices + 16*i + 12; /* X/Y/Z transform */
			trs[0] += v[0] * velocity_weight;
			trs[1] += v[1] * velocity_weight;
			trs[2] += v[2] * velocity_weight;
		}
		time = m_context.ShutterOpen();
	}

	NSI::ArgumentList args;
	args.Add( NSI::Argument::New( "transformationmatrices" )
		->SetType( NSITypeDoubleMatrix )
		->SetCount( num_matrices )
		->SetValuePointer(matrices) );
	nsi.SetAttributeAtTime( m_handle.c_str(), time, args );

	if( velocity_data )
	{
		/* Extrapolate shutter end matrix */
		const float *v = velocity;
		double velocity_weight =
			m_context.ShutterClose() - m_context.ShutterOpen();
		for (int i = 0; i < num_matrices; i++, v+=v_inc)
		{
			double *trs = matrices + 16*i + 12;
			trs[0] += v[0] * velocity_weight;
			trs[1] += v[1] * velocity_weight;
			trs[2] += v[2] * velocity_weight;
		}
		NSI::ArgumentList args;
		args.Add(NSI::Argument::New("transformationmatrices")
			 ->SetType(NSITypeDoubleMatrix)
			 ->SetCount(num_matrices)
			 ->SetValuePointer(matrices));
		nsi.SetAttributeAtTime(
			m_handle.c_str(), m_context.ShutterClose(), args);
	}
	delete [] matrices;
}

std::string instance::merge_handle(const merge_point& i_merge_point) const
{
	VOP_Node* node = i_merge_point.second;
	return
		m_handle +
		"|" + i_merge_point.first +
		"|" + (node ? vop::handle(*node, m_context) : std::string()) +
		"|merge";
}

/**
	\brief Returns as many merge points as necessary to render all the
	<instance[file],material> combinations for this instancer

	Note that there could be *alot* of strings in here. Millions of them
	if we are note careful, because o_merge_points could have as many
	entries as the total number of instances. But, we know that there
	aren't that many possible *different* instances so we just
	store a pointer in o_merge_points that points to the uniqed map.

	The size of the uniqued map is the total number of <instance,material>
	combination for this instancer.

	Note that only OBJ-level instancer seem to have an "instance" attribute
	but our design also permits s@instance on the SOP-level.
*/
void instance::get_merge_points(
	std::map<merge_point, int> &o_uniqued,
	std::vector<const merge_point*> &o_merge_points ) const
{
	GT_Owner type;
	auto instance =
		default_gt_primitive().get()->findAttribute( "instance", type, 0 );
	auto instancefile =
		default_gt_primitive().get()->findAttribute( "instancefile", type, 0 );

	/* give priority to s@instancefile */
	if( instancefile )
		instance = instancefile;

	auto material =
		default_gt_primitive().get()->findAttribute( "shop_materialpath", type, 0 );

	int max_count = std::max(
		instance ? instance->entries() : 0,
		material ? material->entries() : 0 );

	for( int i=0; i<max_count; i ++ )
	{
		std::string I = (instance && i<instance->entries()) ?
					(const char *)(instance->getS(i)) : "";
		std::string M =	(material && i<material->entries()) ?
					(const char*)(material->getS(i)) : "";

		if( instancefile )
		{
			/*  This refers to a file instead of a node. Prefix it so that we
			don't have any collisions */

			I = k_file_prefix + I;
		}

		/*
			Note that this one will be resolved at NSI attribute
			creation time. Fow now, we just need the head node.
		*/
		OP_Node* op = OPgetDirector()->findNode( M.c_str() );
		if( !op )
		{
			/*
				Not sure one can have relative paths in SOPs ... but WHO knows
				in Houdini right ?
			*/
			op = m_object->findNode( M.c_str() );
		}

		merge_point m(I, (VOP_Node *)op);

		if( o_uniqued.find(m) == o_uniqued.end() )
			o_uniqued[m] = 0;

		const auto &it = o_uniqued.find( m );
		o_merge_points.push_back( &it->first );
	}
}

/**
	Returns the total number of instances in this instancer.
*/
int instance::num_instances( void ) const
{
	const UT_StringRef &op_name = m_object->getOperator()->getName();
	if( op_name == "instance" )
	{
		/* OBJ */
		GT_Owner dummy;
		GT_DataArrayHandle P =
			default_gt_primitive().get()->findAttribute( "P", dummy, 0 );
		return P ? P->entries() : 0;
	}

	/* SOP */
	const GT_PrimInstance *instance =
		static_cast<const GT_PrimInstance *>(default_gt_primitive().get());

	const GT_TransformArrayHandle transforms = instance->transforms();
	return transforms->entries();
}

/** utility function to access detail/primitive attributes safely */
inline const float *_attr_access(
	const float *i_src, GT_DataArrayHandle i_attribute, int i_index, int i_stride )
{
	if( !i_attribute )
		return nullptr;

	if( i_index >= i_attribute->entries() )
	{
		/* detail, only first value valid */
		assert( i_attribute->entries() == 1 );
		return i_src;
	}
	return i_src + i_index*i_stride;
}


/**
	\brief Returns the trasnforms needed for the OBJ-level instancer.

	If this is a SOP-level instancer, GT_PrimInstance will provide the necessary
	transforms so no need to call this.
*/
void instance::get_transforms(
	const GT_PrimitiveHandle i_gt_primitive, UT_Matrix4D *o_transforms ) const
{
	GT_Owner type;
	GT_DataArrayHandle P_data = i_gt_primitive->findAttribute( "P", type, 0 );

	if( !P_data )
	{
		assert( false );
		return;
	}

	GT_DataArrayHandle trans_data = i_gt_primitive->findAttribute( "trans", type, 0 );
	GT_DataArrayHandle N_data = i_gt_primitive->findAttribute( "N", type, 0 );
	GT_DataArrayHandle up_data = i_gt_primitive->findAttribute( "up", type, 0 );
	GT_DataArrayHandle pscale_data = i_gt_primitive->findAttribute( "pscale", type, 0 );
	GT_DataArrayHandle scale_data = i_gt_primitive->findAttribute( "scale", type, 0 );
	GT_DataArrayHandle rot_data = i_gt_primitive->findAttribute( "rot", type, 0 );
	GT_DataArrayHandle orient_data = i_gt_primitive->findAttribute( "orient", type, 0 );
	GT_DataArrayHandle pivot_data = i_gt_primitive->findAttribute( "pivot", type, 0 );
	GT_DataArrayHandle v_data = i_gt_primitive->findAttribute( "v", type, 0 );

	/*
		These temporary buffers won't be needed if attributes are indeed 32 bit
		floating points.
	*/
	GT_DataArrayHandle da1, da2, da3, da4, da5, da6, da7, da8, da9, da10;

	const float *P = P_data ? P_data->getF32Array(da1) : nullptr;
	const float *N = N_data ? N_data->getF32Array(da2) : nullptr;
	const float *up = up_data ? up_data->getF32Array(da3) : nullptr;
	const float *q = rot_data ? rot_data->getF32Array(da4) : nullptr;
	const float *tr = trans_data ? trans_data->getF32Array(da5) : nullptr;
	const float *s = pscale_data ? pscale_data->getF32Array(da6) : nullptr;
	const float *s3 = scale_data ? scale_data->getF32Array(da7) : nullptr;
	const float *orient = orient_data ? orient_data->getF32Array(da8) : nullptr;
	const float *pivot = pivot_data ? pivot_data->getF32Array(da9) : nullptr;
	const float *v = v_data ? v_data->getF32Array(da10) : nullptr;

#if 0
	printf( "P:%p N:%p up:%p q:%p tr:%p s:%p s3:%p orient:%p pivot:%p v:%p\n",
		P, N, up, q, tr, s, s3, o, pivot, v );
#endif

	for( int i=0; i<P_data->entries(); i++ )
	{
		UT_Matrix4F res;

		UT_Vector3F _v = {0.f, 0.f, 0.f};
		if( N )
			_v = UT_Vector3F(N[i*3], N[i*3+1], N[i*3+2] );
		else if( v )
			_v = UT_Vector3F(v[i*3], v[i*3+1], v[i*3+2] );

		res.instance(
			P + i*3,
			_v,
			s ? s[i] : 1.0f,
		  	(const UT_Vector3F *) _attr_access( s3, scale_data, i, 3),
		  	(const UT_Vector3F *) _attr_access( up, up_data, i, 3),
			(const UT_QuaternionT<float>*) _attr_access(q, rot_data, i, 4),
		  	(const UT_Vector3F *) _attr_access( tr, trans_data, i, 3),
			(const UT_QuaternionT<float>*) _attr_access(orient, orient_data,i, 4),
		  	(const UT_Vector3F *) _attr_access(pivot, pivot_data,i,3));

		UT_Matrix4D dmat( res );
		memcpy( o_transforms, &dmat[0], 16*sizeof(double) );
		o_transforms ++;
	}
}

/**
	\brief Get paths to instanced objects. SOP-level instancer has no external
	OBJs to reference so it returns nothing.

	\ref scene::scan_for_instanced
*/
void instance::get_instanced(
	std::unordered_set<std::string> &o_instanced ) const
{
	const UT_StringRef &op_name = m_object->getOperator()->getName();
	if( op_name != "instance" )
	{
		/* SOP-level */
		return;
	}

	UT_String path;
	m_object->evalString( path, "instancepath", 0, m_context.m_current_time );
	OP_Node* instanced = OPgetDirector()->findNode(path);
	if(!instanced)
		instanced = m_object->findNode(path);

	if( instanced )
	{
		o_instanced.insert( instanced->getFullPath().toStdString() );
		/* You can't have both instancedpath and s@instance */
		return;
	}

	GT_Owner type;
	auto instance =
		default_gt_primitive().get()->findAttribute( "instance", type, 0 );

	if( !instance )
		return;

	UT_StringArray instances;
	instance->getStrings(instances);

	for( int i=0; i<instances.entries(); i++ )
	{
		o_instanced.insert( instances[i].toStdString() );
	}
}
