#include "geometry.h"

#include "context.h"
#include "curvemesh.h"
#include "instance.h"
#include "object_attributes.h"
#include "polygonmesh.h"
#include "pointmesh.h"
#include "time_sampler.h"
#include "vdb.h"

#include <GT/GT_GEODetail.h>
#include <GT/GT_PrimInstance.h>
#include <GT/GT_Refine.h>
#include <GT/GT_RefineParms.h>
#include <OBJ/OBJ_Node.h>
#include <OP/OP_Operator.h>
#include <VOP/VOP_Node.h>
#include <SOP/SOP_Node.h>

#include <iostream>
#include <algorithm>


namespace
{

/**
	Refines i_primitive and appends the resulting primitives to io_result
*/
std::vector<primitive *> Refine(
	const GT_PrimitiveHandle& i_primitive,
	OBJ_Node& i_object,
	const context& i_context,
	double i_time,
	std::vector<primitive*>& io_result,
	unsigned& io_primitive_index,
	bool& io_requires_frame_aligned_sample,
	int i_level = 0 );


/**
	\brief A GT refiner for an OBJ_Node.

	When an OBJ_Node is refined, the top node is a null to which
	we will connect all newly created primitives. The GT prmitives
	are not registered as
*/
struct OBJ_Node_Refiner : public GT_Refine
{
	OBJ_Node *m_node;

	/**
		All the refined objects
	*/
	std::vector<primitive*> &m_result;

	/**
		This has to be set at each addPrimitive run. It is necessary
		for the easy implementation of instances. We need this variable
		because addPrimitive() does't have a return value and we need
		that value for instances (as we refine the instance models).
	*/
	std::vector<primitive*> m_return;

	bool& m_requires_frame_aligned_sample;
	const context &m_context;
	double m_time;
	int m_level;
	unsigned& m_primitive_index;
	bool m_stop;

	OBJ_Node_Refiner(
		OBJ_Node *i_node,
		const context &i_context,
		double i_time,
		std::vector<primitive*> &io_result,
		unsigned& io_primitive_index,
		bool& io_requires_frame_aligned_sample,
		int level)
	:
		m_node(i_node),
		m_result(io_result),
		m_requires_frame_aligned_sample(io_requires_frame_aligned_sample),
		m_context(i_context),
		m_time(i_time),
		m_level(level),
		m_primitive_index(io_primitive_index),
		m_stop(false)
	{
	}


	/**

		Diable threadig at this level: no gain and all pain!

		Allowing threading means that we receive geo out-of-order.  This
		is impossible to manage in the situation where we have motion
		blur for example. Not only that, but we would have to disable it
		when we have instances, as the order of primitives comes randomly.
		Note sure how this can work really.
	*/
	virtual bool allowThreading( void ) const override
	{
		return false;
	}

	/**
		One interesting thing here is how we deal with instances. We first
		refine() recursively to resolve the instanced geometry since we
		need its handle to pass to the actual instancer. All the rest is 1 to 1
		mapping with either one of our primitive exporters.
	*/
	void addPrimitive( const GT_PrimitiveHandle &i_primitive ) override
	{
		if(m_stop)
		{
			return;
		}

		// Create new primitive exporters for refined GT primitives

		unsigned index = m_result.size();

		switch( i_primitive->getPrimitiveType() )
		{
		case GT_PRIM_POLYGON_MESH:
		{
			m_result.push_back(
				new polygonmesh(m_context, m_node, m_time, i_primitive, index, false) );
			m_return.push_back( m_result.back() );
			break;
		}
		case GT_PRIM_SUBDIVISION_MESH:
			m_result.push_back(
				new polygonmesh(m_context, m_node, m_time, i_primitive, index, true) );
			m_return.push_back( m_result.back() );
			break;

		case GT_PRIM_POINT_MESH:
		{
			const UT_StringRef &op_name = m_node->getOperator()->getName();
			if( op_name == "instance" &&
				m_node->getParmIndex("instancepath")>=0 )
			{
				/*
					OBJ-level instancer. Note that "instancepath" will always
					return "" if there is a s@instance on the geo.
				*/
				UT_String path;
				m_node->evalString( path, "instancepath", 0, m_time );
				std::string absolute = exporter::absolute_path( m_node, path );

				std::vector<std::string> models;
				models.push_back( absolute );

				m_result.push_back(
					new instance(
						m_context, m_node, m_time, i_primitive, index, models));
			}
			else
			{
				m_result.push_back(
					new pointmesh(m_context, m_node, m_time, i_primitive, index) );
			}
			m_return.push_back( m_result.back() );
			break;
		}

		case GT_PRIM_SUBDIVISION_CURVES:
			m_result.push_back(
				new curvemesh(m_context, m_node, m_time, i_primitive, index) );
			m_return.push_back( m_result.back() );
			break;

		case GT_PRIM_CURVE_MESH:
			m_result.push_back(
				new curvemesh(m_context, m_node, m_time, i_primitive, index) );
			m_return.push_back( m_result.back() );
			break;

		case GT_PRIM_INSTANCE:
		{
			/* First add the primitive so that we can get its handle. */
			const GT_PrimInstance *I =
				static_cast<const GT_PrimInstance *>( i_primitive.get() );

			std::string vdb_path =
				vdb::node_is_vdb_loader( m_node, m_context.m_current_time );

			std::vector<primitive *> ret;
			if( !vdb_path.empty() )
			{
				/*
					We got ourselves an instancer of VDBs. This happens anytime
					you have a packed disk file read with some transform.  If
					we enter the refine below we will still get a correct
					render but with loaded VDBs in memory; and we don't want
					that.
				*/
				m_result.push_back( new vdb(
					m_context, m_node, m_time, i_primitive, index, vdb_path) );
				ret.push_back( m_result.back () );
				m_return.push_back( m_result.back() );
			}
			else
			{
				ret = Refine(
					I->geometry(),
					*m_node,
					m_context,
					m_time,
					m_result,
					m_primitive_index,
					m_requires_frame_aligned_sample,
					m_level+1);
			}

			if(	ret.empty() )
			{
#ifdef VERBOSE
				std::cerr
					<< "3Delight for Houdini: unable to create instanced geometry for "
					<< m_node->getFullPath() << std::endl;
#endif
				return;
			}

			std::vector<std::string> source_models;
			for( auto P : ret )
			{
				source_models.push_back( P->handle() );
				P->set_as_instanced();
			}

			index = m_result.size();
			m_result.push_back(
				new instance(
					m_context, m_node,m_time,i_primitive,index, source_models));
			m_return.push_back( m_result.back() );
			break;
		}

		case GT_PRIM_VOXEL_VOLUME:
#ifdef VERBOSE
			fprintf(
				stderr, "3Delight for Houdini: unsupported VDB/Volume "
					"workflow for %s\n", m_node->getName().c_str() );
#endif
			break;

		case GT_PRIM_VDB_VOLUME:
		{
			/*
				Houdini could call us once per grid! And this is not what we
				want.
			*/
			auto it = std::find_if(
				m_result.begin(), m_result.end(),
				[] (const primitive *e) { return e->is_volume(); } );

			if( it != m_result.end() )
			{
				/* volume already output */
				return;
			}

			std::string vdb_path =
				vdb::node_is_vdb_loader( m_node, m_context.m_current_time );

			if( !vdb_path.empty() )
			{
				m_result.push_back(
					new vdb( m_context, m_node, m_time, i_primitive, index, vdb_path) );
				m_return.push_back( m_result.back() );
			}
			else
			{
#ifdef VERBOSE
				fprintf( stderr, "3Delight for Houdini: unsupported VDB/Volume "
					"workflow for %s\n", m_node->getName().c_str() );
#endif
			}
			break;
		}

		default:
		{
#ifdef VERBOSE
			fprintf( stderr, "%p: Opening %s and result is %d, return is %d (level %d)\n",
				this,
				i_primitive->className(), m_result.size(), m_return.size(), m_level );
#endif

			std::vector<primitive *> ret = Refine(
				i_primitive,
				*m_node,
				m_context,
				m_time,
				m_result,
				m_primitive_index,
				m_requires_frame_aligned_sample,
				m_level+1);

			if( ret.empty() )
			{
#ifdef VERBOSE
				std::cerr << "3Delight for Houdini: unsupported object "
					<< m_node->getFullPath()
					<< " of class " << i_primitive->className()
					<< std::endl;
#endif
			}

			/*
				We need to add the return shapes from the recursive context
				into the current one. The m_result doens't need to be updated
				as it is passed by reference from one context to the other and
				it has been updated in the recursive context.
			*/
			m_return.insert( m_return.end(), ret.begin(), ret.end() );
		}
		}

		if(!m_result.empty())
		{
			m_requires_frame_aligned_sample =
				m_requires_frame_aligned_sample ||
				m_result.back()->requires_frame_aligned_sample();
		}
	}
};

std::vector<primitive *> Refine(
	const GT_PrimitiveHandle& i_primitive,
	OBJ_Node& i_object,
	const context& i_context,
	double i_time,
	std::vector<primitive*>& io_result,
	unsigned& io_primitive_index,
	bool& io_requires_frame_aligned_sample,
	int i_level)
{
	OBJ_Node_Refiner refiner(
		&i_object,
		i_context,
		i_time,
		io_result,
		io_primitive_index,
		io_requires_frame_aligned_sample,
		i_level);

	GT_RefineParms params;
	params.setAllowSubdivision( true );
	params.setAddVertexNormals( true );
	params.setCuspAngle( GEO_DEFAULT_ADJUSTED_CUSP_ANGLE );

	i_primitive->refine( refiner, &params );

	return refiner.m_return;
}

}

geometry::geometry(const context& i_context, OBJ_Node* i_object)
	:	exporter(i_context, i_object)
{
	SOP_Node *sop = m_object->getRenderSopPtr();

	assert( sop );
	/*
		Refine the geometry once per time sample, and maybe also at the current
		time (if it contains primitives that require frame-aligned time samples
		and such a sample is not part of the ones returned by the time_sampler).
	*/
	bool requires_frame_aligned_sample = false;
	bool exported_frame_aligned_sample = false;
	for(time_sampler t(m_context, *m_object, time_sampler::e_deformation);
		t || (requires_frame_aligned_sample && !exported_frame_aligned_sample);
		t ? t++,0 : 0)
	{
		double time;
		if(t)
		{
			time = *t;
		}
		else
		{
			/*
				This is an extra time sample meant only for primitives that
				require one that is aligned on a frame.
			*/
			assert(requires_frame_aligned_sample);
			time = m_context.m_current_time;
		}

		OP_Context context(time);
		GU_DetailHandle detail_handle( sop->getCookedGeoHandle(context) );

		if( !detail_handle.isValid() )
		{
			std::cerr
				<< "3Delight for Houdini: " << m_object->getFullPath()
				<< " has no valid detail" << std::endl;
			return;
		}

		/*
			This seems to be important in order to avoid overwriting the handles
			of previous time samples with the current one.
		*/
		detail_handle.addPreserveRequest();

		std::vector<primitive *> result;

		GT_PrimitiveHandle gt( GT_GEODetail::makeDetail(detail_handle) );
		unsigned primitive_index = 0;
		Refine(
			gt,
			*m_object,
			m_context,
			time,
			result,
			primitive_index,
			requires_frame_aligned_sample );


		if( result.empty() )
			break;

		if( m_primitives.empty() )
		{
			m_primitives = result;
		}
		else if( result.size() == m_primitives.size() )
		{
			int i = 0;
			for( auto E : m_primitives )
			{
				if( !E->merge_time_samples(*result[i]) )
				{
					#if 0
					fprintf( stderr, "Error (%s %s) (%d %d)\n",
						E->handle().c_str(), result[i]->handle().c_str(),
						E->default_gt_primitive()->getPrimitiveType(),
						result[i]->default_gt_primitive()->getPrimitiveType() );
					#endif
				}
				i++;
			}

			for( auto R : result )
				delete R;
		}

		exported_frame_aligned_sample =
			exported_frame_aligned_sample ||
			time == m_context.m_current_time;
	}

#ifdef VERBOSE
	std::cout << m_object->getFullPath() << " gave birth to " <<
		m_primitives.size() << " primitives." << std::endl;
#endif
}

geometry::~geometry()
{
	for( primitive* p : m_primitives )
	{
		delete p;
	}
}

void geometry::create()const
{
	for(primitive* p : m_primitives)
	{
		p->create();
	}
}

/**
	Besides calling the set_attributes() of underlying primitives to set the
	standard geometric attributes (such as P, N, etc.), we also export
	export attributes that are needed by the assigned materials. This is done
	in primitive::export_bind_attributes(...).
*/

void geometry::set_attributes()const
{
	std::string dummy;
	OP_Node *vop = get_assigned_material( dummy );

	for(primitive* p : m_primitives)
	{
		p->set_attributes();
		p->export_bind_attributes( vop );
	}
}

void geometry::connect()const
{
	// Connect all primitives to their ancestor
	bool volume = false;
	for(primitive* p : m_primitives)
	{
		/*
			There is only one material for the whole object. Let's hope that all
			its components use it the same way! We give priority to the volume
			because of our limited current support, which involves VDB
			primitives (volumes) referred to by instancers (non-volume, but
			also non-surface).
		*/
		volume = volume || p->is_volume();

		p->connect();
	}

	// Connect geometry attributes for non-default values
	object_attributes::connect_to_object_attributes_nodes(
		m_context,
		m_object,
		m_handle);

	/*
		Connect to the matte attributes node if the node is part of the matte
		bundle.
		FIXME : the call to connect_to_object_attributes_nodes above also
		handles mattes, but based on an object's attribute instead of a bundle.
	*/
	if(m_context.object_is_matte(*m_object))
	{
		const char *matte_handle =
			object_attributes::geo_attribute_node_handle(
				object_attributes::e_matte);
		m_nsi.Connect(matte_handle, "", m_handle, "geometryattributes");
	}

	/*
		OBJ-level material assignment.

		\see polygonmesh for primitive attribute assignment on polygonal faces
	*/
	std::string material_path;
	if( get_assigned_material(material_path) == nullptr )
		return;

	m_nsi.Create( attributes_handle(), "attributes" );
	m_nsi.Connect( attributes_handle(), "", m_handle, "geometryattributes" );

	m_nsi.Connect(
		material_path, "",
		attributes_handle(), volume ? "volumeshader" : "surfaceshader" );

	export_override_attributes();
}

void geometry::export_override_attributes() const
{
	if ( !m_object->hasParm("_3dl_spatial_override") )
	{
		// We presume that our other attributes are not present
		return;
	}

	std::string	override_nsi_handle = overrides_handle();

	m_nsi.Create( override_nsi_handle, "attributes" );

	NSI::ArgumentList arguments;

	float time = m_context.m_current_time;

	// Visible to Camera override
	if ( m_object->hasParm("_3dl_override_visibility_camera_enable") )
	{
		bool over_vis_camera_enable =
			m_object->evalInt("_3dl_override_visibility_camera_enable", 0, time);

		if( over_vis_camera_enable )
		{
			bool vis_camera_over =
				m_object->evalInt("_3dl_override_visibility_camera", 0, time);

			arguments.Add(
				new NSI::IntegerArg("visibility.camera", vis_camera_over));
			arguments.Add(
				new NSI::IntegerArg("visibility.camera.priority", 10));
		}
		else
		{
			m_nsi.DeleteAttribute( override_nsi_handle, "visibility.camera" );
			m_nsi.DeleteAttribute(
				override_nsi_handle, "visibility.camera.priority" );
		}
	}

	// Visible in Diffuse override
	if ( m_object->hasParm("_3dl_override_visibility_diffuse_enable") )
	{
		bool over_vis_diffuse_enable =
			m_object->evalInt("_3dl_override_visibility_diffuse_enable", 0, time);

		if( over_vis_diffuse_enable )
		{
			bool vis_diffuse_over = false;
				m_object->evalInt("_3dl_override_visibility_diffuse", 0, time);

			arguments.Add(
				new NSI::IntegerArg("visibility.diffuse", vis_diffuse_over));
			arguments.Add(
				new NSI::IntegerArg("visibility.diffuse.priority", 10));
		}
		else
		{
			m_nsi.DeleteAttribute(
				override_nsi_handle, "visibility.diffuse" );
			m_nsi.DeleteAttribute(
				override_nsi_handle, "visibility.diffuse.priority" );
		}
	}

	// Visible in Reflections override
	if ( m_object->hasParm("_3dl_override_visibility_reflection_enable") )
	{
		bool over_vis_reflection_enable =
			m_object->evalInt(
				"_3dl_override_visibility_reflection_enable", 0, time);

		if( over_vis_reflection_enable )
		{
			bool over_vis_reflection =
				m_object->evalInt("_3dl_override_visibility_reflection", 0, time);

			arguments.Add(
				new NSI::IntegerArg("visibility.reflection", over_vis_reflection));
			arguments.Add(
				new NSI::IntegerArg("visibility.reflection.priority", 10));
		}
		else
		{
			m_nsi.DeleteAttribute(
				override_nsi_handle, "visibility.reflection" );
			m_nsi.DeleteAttribute(
				override_nsi_handle, "visibility.reflection.priority" );
		}
	}

	// Visible in Refractions override
	if( m_object->hasParm("_3dl_override_visibility_refraction_enable") )
	{
		bool over_vis_refraction_enable =
			m_object->evalInt("_3dl_override_visibility_refraction_enable", 0, time);

		if( over_vis_refraction_enable )
		{
			bool over_vis_refraction =
				m_object->evalInt("_3dl_override_visibility_refraction", 0, time);

			arguments.Add(
				new NSI::IntegerArg("visibility.refraction", over_vis_refraction));
			arguments.Add(
				new NSI::IntegerArg("visibility.refraction.priority", 10));
		}
		else
		{
			m_nsi.DeleteAttribute(
				override_nsi_handle, "visibility.refraction" );
			m_nsi.DeleteAttribute(
				override_nsi_handle, "visibility.refraction.priority" );
		}
	}

	// Compositing mode override
	if ( m_object->hasParm("_3dl_override_compositing_enable") )
	{
		bool over_compositing_enable =
			m_object->evalInt("_3dl_override_compositing_enable", 0, time);

		if( over_compositing_enable )
		{
			UT_String override_compositing;
			m_object->evalString(
				override_compositing, "_3dl_override_compositing", 0, time);

			bool matte = override_compositing == "matte";
			bool prelit = override_compositing == "prelit";

			arguments.Add( new NSI::IntegerArg("matte", matte));
			arguments.Add( new NSI::IntegerArg("matte.priority", 10));
			arguments.Add( new NSI::IntegerArg("prelit", prelit));
			arguments.Add( new NSI::IntegerArg("prelit.priority", 10));
		}
		else
		{
			m_nsi.DeleteAttribute( override_nsi_handle, "matte" );
			m_nsi.DeleteAttribute( override_nsi_handle, "matte.priority" );
			m_nsi.DeleteAttribute( override_nsi_handle, "prelit" );
			m_nsi.DeleteAttribute( override_nsi_handle, "prelit.priority" );
		}
	}

	// Set all override attributes defined above
	if( arguments.size() > 0 )
	{
		m_nsi.SetAttribute( override_nsi_handle, arguments );
	}

	/*
		Adjust connections to make the override effective or not according to
		its	main override toggle.

		Note that the surface shader overrides doesn't need an [x] Enable
		toggle.
	*/
	bool spatial_override = m_object->evalInt("_3dl_spatial_override", 0, time);
	if( spatial_override )
	{
		m_nsi.Connect( m_handle, "",
			override_nsi_handle, "bounds" );
		m_nsi.Connect( override_nsi_handle, "",
			NSI_SCENE_ROOT, "geometryattributes" );

		m_nsi.Connect(
			transparent_surface_handle(), "",
			m_handle, "geometryattributes" );

		bool override_surface_shader = m_object->evalInt(
			"_3dl_override_surface_shader", 0, time);

		if( override_surface_shader )
		{
			NSI::ArgumentList arguments;
			arguments.Add(new NSI::IntegerArg("priority", 10));

			std::string material;
			get_assigned_material( material );

			m_nsi.Connect(
				material, "", override_nsi_handle, "surfaceshader", arguments );
		}
	}
	else
	{
		/* NOTE: This is needed for IPR only */
		m_nsi.Disconnect( m_handle, "",
			override_nsi_handle, "bounds" );
		m_nsi.Disconnect( override_nsi_handle, "",
			NSI_SCENE_ROOT, "geometryattributes" );
	}
}

/**
	\brief Return any instance exported in this geometry.
*/
void geometry::get_instances( std::vector<const instance *> &o_instances ) const
{
	for( auto P : m_primitives )
	{
		instance *I = dynamic_cast<instance *>( P );
		if( I )
			o_instances.push_back( I );
	}
}

VOP_Node *geometry::get_assigned_material( std::string &o_path ) const
{
	int index = m_object->getParmIndex( "shop_materialpath" );
	if( index < 0 )
		return nullptr;

	UT_String material_path;
	m_object->evalString( material_path, "shop_materialpath", 0, 0.f );

	if( material_path.length()==0 )
	{
		return nullptr;
	}

	return resolve_material_path( material_path.c_str(), o_path );
}

/**
	Return all the materials needed by this geometry.
*/
void geometry::get_all_material_paths(
	std::unordered_set< std::string > &o_materials ) const
{
	for( auto P : m_primitives )
	{
		P->get_all_material_paths( o_materials );
	}

	std::string obj_mat;
	get_assigned_material( obj_mat );

	o_materials.insert( obj_mat );
}
