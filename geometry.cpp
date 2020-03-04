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
	bool i_add_time_samples,
	int i_level = 0);


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
	bool m_add_time_samples;
	bool m_stop;

	OBJ_Node_Refiner(
		OBJ_Node *i_node,
		const context &i_context,
		double i_time,
		std::vector<primitive*> &io_result,
		unsigned& io_primitive_index,
		bool& io_requires_frame_aligned_sample,
		bool i_add_time_samples,
		int level)
	:
		m_node(i_node),
		m_result(io_result),
		m_requires_frame_aligned_sample(io_requires_frame_aligned_sample),
		m_context(i_context),
		m_time(i_time),
		m_level(level),
		m_primitive_index(io_primitive_index),
		m_add_time_samples(i_add_time_samples),
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

		When m_add_time_samples is true, we follow the same refinement logic,
		but we add the resulting GT primitives to previously creates primitive
		exporters instead of creating new ones.
	*/
	void addPrimitive( const GT_PrimitiveHandle &i_primitive ) override
	{
		if(m_stop)
		{
			return;
		}

		if(m_add_time_samples)
		{
			// Update existing primitive exporters

			switch( i_primitive->getPrimitiveType() )
			{
				case GT_PRIM_INSTANCE:
				{
					const GT_PrimInstance* I =
						static_cast<const GT_PrimInstance*>(i_primitive.get());
					addPrimitive(I->geometry());
					/*
					Refine(
						I->geometry(), *m_node, m_context, m_time, m_result,
						m_requires_frame_aligned_sample, m_add_time_samples,
						m_level+1);
					*/
				}
				// Fall through so the instancer is updated, too
				case GT_PRIM_POLYGON_MESH:
				case GT_PRIM_SUBDIVISION_MESH:
				case GT_PRIM_POINT_MESH:
				case GT_PRIM_SUBDIVISION_CURVES:
				case GT_PRIM_CURVE_MESH:
				case GT_PRIM_VDB_VOLUME:
					assert(m_primitive_index < m_result.size());


					if(m_primitive_index < m_result.size() &&
						m_result[m_primitive_index]->
							add_time_sample(m_time, i_primitive))
					{
						m_primitive_index++;
					}
					else
					{
						m_stop = true;
						std::cerr
							<< "3Delight for Houdini: unable to create motion-"
								"blurred geometry for "
							<< m_node->getFullPath() << std::endl;
					}
					break;
				case GT_PRIM_VOXEL_VOLUME:
					// Unsupported
					break;
				default:
					Refine(
						i_primitive,
						*m_node,
						m_context,
						m_time,
						m_result,
						m_primitive_index,
						m_requires_frame_aligned_sample,
						m_add_time_samples,
						m_level+1);
			}

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

			std::vector<primitive *> ret = Refine(
				I->geometry(),
				*m_node,
				m_context,
				m_time,
				m_result,
				m_primitive_index,
				m_requires_frame_aligned_sample,
				m_add_time_samples,
				m_level+1);

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
				m_add_time_samples,
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
	bool i_add_time_samples,
	int i_level)
{
	OBJ_Node_Refiner refiner(
		&i_object,
		i_context,
		i_time,
		io_result,
		io_primitive_index,
		io_requires_frame_aligned_sample,
		i_add_time_samples,
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

	/*
		Refine the geometry once per time sample, and maybe also at the current
		time (if it contains primitives that require frame-aligned time samples
		and such a sample is not part of the ones returned by the time_sampler).
	*/
	bool update = false;
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

		GT_PrimitiveHandle gt( GT_GEODetail::makeDetail(detail_handle) );
		unsigned primitive_index = 0;
		Refine(
			gt,
			*m_object,
			m_context,
			time,
			m_primitives,
			primitive_index,
			requires_frame_aligned_sample,
			update);

		if(m_primitives.empty())
		{
			break;
		}

		exported_frame_aligned_sample =
			exported_frame_aligned_sample ||
			time == m_context.m_current_time;
		update = true;
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

void geometry::set_attributes()const
{
	for(primitive* p : m_primitives)
	{
		p->set_attributes();
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
		OBJ-level material assignment.

		\see polygonmesh for SOP-level assignmens on polygonal faces
	*/
	std::string material_path;
	if( get_assigned_material(material_path) == nullptr )
		return;

	std::string attributes = m_handle + "|attributes";
	m_nsi.Create( attributes, "attributes" );
	m_nsi.Connect( attributes, "", m_handle, "geometryattributes" );

	m_nsi.Connect(
		material_path, "",
		attributes, volume ? "volumeshader" : "surfaceshader" );
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
