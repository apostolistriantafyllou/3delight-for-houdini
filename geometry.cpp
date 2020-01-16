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

/// Refines i_primitive and puts the resulting primitives in io_result
bool Refine(
	const GT_PrimitiveHandle& i_primitive,
	OBJ_Node& i_object,
	const context& i_context,
	double i_time,
	std::vector<primitive*>& io_result,
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
	std::vector<primitive*> &m_result;
	bool& m_requires_frame_aligned_sample;
	const context &m_context;
	double m_time;
	int m_level;
	unsigned m_primitive_index;
	bool m_add_time_samples;
	bool m_stop;

	OBJ_Node_Refiner(
		OBJ_Node *i_node,
		const context &i_context,
		double i_time,
		std::vector<primitive*> &io_result,
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
		m_primitive_index(0),
		m_add_time_samples(i_add_time_samples),
		m_stop(false)
	{
	}


	/**
		One interesting thing here is how we deal with instances. We first
		addPrimitive() recursively to resolve the instanced geometry since we
		need its handle to pass to the actual instancer. All the rest is 1 to 1
		mapping with either one of our primitive exporters.

		When m_add_time_samples is true, we follow the same refinement logic,
		but we add the resulting GT primitives to previously creates primitive
		exporters instead of creating new ones.
	*/
	void addPrimitive( const GT_PrimitiveHandle &i_primitive )
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
				}
				// Fall through so the instancer is updated, too
				case GT_PRIM_POLYGON_MESH:
				case GT_PRIM_SUBDIVISION_MESH:
				case GT_PRIM_POINT_MESH:
				case GT_PRIM_SUBDIVISION_CURVES:
				case GT_PRIM_CURVE_MESH:
				case GT_PRIM_VDB_VOLUME:
					assert(m_primitive_index < m_result.size());
					if(m_primitive_index >= m_result.size() ||
						!m_result[m_primitive_index]->
							add_time_sample(m_time, i_primitive))
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
						m_requires_frame_aligned_sample,
						m_add_time_samples,
						m_level+1);
			}

			m_primitive_index++;
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
			break;
		}
		case GT_PRIM_SUBDIVISION_MESH:
			m_result.push_back(
				new polygonmesh(m_context, m_node, m_time, i_primitive, index, true) );
			break;

		case GT_PRIM_POINT_MESH:
			m_result.push_back(
				new pointmesh(m_context, m_node, m_time, i_primitive, index) );
			break;

		case GT_PRIM_SUBDIVISION_CURVES:
			m_result.push_back(
				new curvemesh(m_context, m_node, m_time, i_primitive, index) );
			break;

		case GT_PRIM_CURVE_MESH:
			m_result.push_back(
				new curvemesh(m_context, m_node, m_time, i_primitive, index) );
			break;

		case GT_PRIM_INSTANCE:
		{
			/* First add the primitive so that we can get its handle. */
			const GT_PrimInstance *I =
				static_cast<const GT_PrimInstance *>( i_primitive.get() );
			addPrimitive( I->geometry() );

			if( m_result.size() == index )
			{
				std::cerr
					<< "3Delight for Houdini: unable to create instanced geometry for "
					<< m_node->getFullPath() << std::endl;
				return;
			}

			index = m_result.size();

			primitive *instanced = m_result.back();
			instanced->set_as_instanced();
			m_result.push_back(
				new instance(
					m_context, m_node, m_time, i_primitive, index, instanced->handle()) );
			break;
		}

		case GT_PRIM_VOXEL_VOLUME:
			fprintf(
				stderr, "3Delight for Houdini: unsupported VDB/Volume "
					"workflow for %s\n", m_node->getName().c_str() );
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
			}
			else
			{
				fprintf( stderr, "3Delight for Houdini: unsupported VDB/Volume "
					"workflow for %s\n", m_node->getName().c_str() );
			}
			break;
		}

		default:
#ifdef VERBOSE
			std::cout << "Refining " << m_node->getFullPath() <<
				" to level " << m_level  << std::endl;
#endif
			if(!Refine(
					i_primitive,
					*m_node,
					m_context,
					m_time,
					m_result,
					m_requires_frame_aligned_sample,
					m_add_time_samples,
					m_level+1))
			{
#ifdef VERBOSE
				std::cerr << "3Delight for Houdini: unsupported object "
					<< m_node->getFullPath()
					<< " of class " << i_primitive->className()
					<< std::endl;
#endif
			}
		}

		if(!m_result.empty())
		{
			m_requires_frame_aligned_sample =
				m_result.back()->requires_frame_aligned_sample();
		}
	}
};

bool Refine(
	const GT_PrimitiveHandle& i_primitive,
	OBJ_Node& i_object,
	const context& i_context,
	double i_time,
	std::vector<primitive*>& io_result,
	bool& io_requires_frame_aligned_sample,
	bool i_add_time_samples,
	int i_level)
{
	OBJ_Node_Refiner refiner(
		&i_object,
		i_context,
		i_time,
		io_result,
		io_requires_frame_aligned_sample,
		i_add_time_samples,
		i_level);

	GT_RefineParms params;
	params.setAllowSubdivision( true );
	params.setAddVertexNormals( true );
	params.setCuspAngle( GEO_DEFAULT_ADJUSTED_CUSP_ANGLE );

	return i_primitive->refine( refiner, &params );
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
		Refine(
			gt,
			*m_object,
			m_context,
			time,
			m_primitives,
			requires_frame_aligned_sample,
			update);

		if(m_primitives.empty())
		{
			break;
		}

		exported_frame_aligned_sample = time == m_context.m_current_time;
		update = true;
	}

#ifdef VERBOSE
	std::cout << m_object->getFullPath() << " gave birth to " <<
		m_primitives.size() << " primitives." << std::endl;
#endif
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

	// Do local material assignment

	int index = m_object->getParmIndex( "shop_materialpath" );
	if( index < 0 )
	{
		return;
	}

	UT_String material_path;
	m_object->evalString( material_path, "shop_materialpath", 0, 0.f );

	if( material_path.length() == 0 )
	{
		return;
	}

	std::string attributes = m_handle + "|attributes";
	m_nsi.Create( attributes, "attributes" );
	m_nsi.Connect( attributes, "", m_handle, "geometryattributes" );

	m_nsi.Connect(
		material_path.buffer(), "",
		attributes, volume ? "volumeshader" : "surfaceshader" );
}
