#include "scene.h"

/* exporters { */
#include "camera.h"
#include "exporter.h"
#include "instance.h"
#include "light.h"
#include "polygonmesh.h"
#include "null.h"
#include "pointmesh.h"
#include "vop.h"
/* } */

#include "context.h"
#include "utilities.h"

#include <GT/GT_GEODetail.h>
#include <GT/GT_Refine.h>
#include <GT/GT_PrimInstance.h>
#include <OBJ/OBJ_Camera.h>
#include <OBJ/OBJ_Node.h>
#include <OP/OP_Director.h>
#include <SOP/SOP_Node.h>

/**
	\brief A GT refiner for an OBJ_Node.

	When an OBJ_Node is refined, the top node is a null to which
	we will connect all newly created primitives. The GT prmitives
	are not registered as
*/
struct OBJ_Node_Refiner : public GT_Refine
{
	OBJ_Node *m_node;
	std::vector<exporter *> &m_result;
	const context &m_context;
	int m_level;

	OBJ_Node_Refiner(
		OBJ_Node *i_node,
		const context &i_context,
		std::vector< exporter *> &i_result,
		int level = 0 )
	:
		m_result(i_result),
		m_node(i_node),
		m_context(i_context),
		m_level(level)
	{
	}

	void addPrimitive( const GT_PrimitiveHandle &i_primitive )
	{
		switch( i_primitive->getPrimitiveType() )
		{
		case GT_PRIM_POLYGON_MESH:
			m_result.push_back(
				new polygonmesh(m_context, m_node,i_primitive, false) );
			break;

		case GT_PRIM_SUBDIVISION_MESH:
			m_result.push_back(
				new polygonmesh(m_context, m_node,i_primitive, true) );
			break;

		case GT_PRIM_POINT_MESH:
			m_result.push_back(
				new pointmesh(m_context, m_node,i_primitive) );
			break;

		case GT_PRIM_INSTANCE:
		{
			size_t s = m_result.size();

			/* First add the primitive so that we can get its handle. */
			const GT_PrimInstance *I =
				static_cast<const GT_PrimInstance *>( i_primitive.get() );
			addPrimitive( I->geometry() );

			if( s == m_result.size() )
			{
				std::cout << "Unable to create instancer" << std::endl;
				return;
			}

			exporter *instanced = m_result.back();
			instanced->set_as_instanced();
			m_result.push_back(
				new instance(
					m_context, m_node, i_primitive, instanced->handle()) );
			break;
		}

		default:
			if( m_level < 5 )
			{
				std::cout << "Refining " << m_node->getFullPath() <<
					" to level " << m_level  << std::endl;

				OBJ_Node_Refiner refiner(
					m_node, m_context, m_result, m_level+1 );
				i_primitive->refine( refiner, nullptr );
			}
			else
			{
				std::cout << "Unsupported object " << m_node->getFullPath() <<
					" of type " << i_primitive->className() << std::endl;
			}
			return;
		}
	}
};

/**
	\brief Decide what to do with the given OP_Node.

	NULLs are output, always. They are the internal nodes of our scene
	tree. Other than that, we output any obj that is a OBJ_GEOMETRY
	and that is renderable in the current frame range.
*/
void scene::process_node(
	const context &i_context,
	OP_Node *i_node,
	std::vector<exporter *> &o_to_export )
{
	if( i_node->castToVOPNode() )
	{
		/*
		   Our material network connections is done :) The connection
		   between nodes is done in \ref vop::copnnect
		   Read vop.cpp if you are bewildered by this simplicity.
		*/
		o_to_export.push_back(
			new vop(i_context, i_node->castToVOPNode()) );
		return;
	}

	OBJ_Node *obj = i_node->castToOBJNode();
	if( !obj )
		return;

	/*
		Each object is its own null transform. This could produce useless
		transforms if we decide not to output the node after all but I am
		aiming for code simplicity here.
	*/
	o_to_export.push_back( new null(i_context, obj) );

	if( obj->getObjectType() & OBJ_NULL )
	{
		return;
	}

	if( obj->castToOBJLight() )
	{
		o_to_export.push_back( new light(i_context, obj) );

		/*
			We don't return here because an OBJ_Light is also and OBJ_Camera
			and we want to export both. This will allow to render the scene
			from the point of view of a light source.
		*/
	}

	if( obj->castToOBJCamera() )
	{
		o_to_export.push_back( new camera(i_context, obj) );
		return;
	}

	SOP_Node *sop = obj->getRenderSopPtr();

	if( !sop )
	{
		std::cout << "3Delight for Houdini: no render SOP for " <<
			obj->getFullPath() << std::endl;
		return;
	}

	/* FIXME: motion blur */
	OP_Context context( i_context.m_start_time );
	const GU_ConstDetailHandle detail_handle( sop->getCookedGeoHandle(context) );

	if( !detail_handle.isValid() )
	{
		std::cout << "3Delight for Houdini: " << obj->getFullPath() <<
			" has no valud detail" << std::endl;
		return;
	}

	std::vector< exporter *> gt_primitives;

	GT_PrimitiveHandle gt( GT_GEODetail::makeDetail(detail_handle) );
	OBJ_Node_Refiner refiner( obj, i_context, gt_primitives );
	gt->refine( refiner, nullptr );

	std::cout << obj->getFullPath() << " gave birth to " <<
		gt_primitives.size() << " primitives." << std::endl;

	o_to_export.insert(
		o_to_export.end(), gt_primitives.begin(), gt_primitives.end() );

}

/**
	\brief Scans for geometry, cameras, lights, vops etc... and produces
	a list of exporters.

	\ref exporter
*/
void scene::scan(
	const context &i_context,
	std::vector<exporter *> &o_to_export )
{
	/* A traversal stack to avoid recursion */
	std::vector< OP_Node * > traversal;

	OP_Node *our_dear_leader = OPgetDirector();
	traversal.push_back( our_dear_leader->findNode( "/obj") );
	traversal.push_back( our_dear_leader->findNode( "/mat") );

	/*
		After this while loop, to_export will be filled with the OBJs
		and the VOPs to convert to NSI.
	*/
	while( traversal.size() )
	{
		OP_Node *network = traversal.back();
		traversal.pop_back();

		assert( network->isNetwork() );

		int nkids = network->getNchildren();
		for( int i=0; i< nkids; i++ )
		{
			OP_Node *node = network->getChild(i);
			process_node( i_context, node, o_to_export );

			if( !node->isNetwork() )
				continue;

			OP_Network *kidnet = (OP_Network *)node;
			if( kidnet->getNchildren() )
			{
				traversal.push_back( kidnet );
			}
		}
	}
}

/**
	\brief Contains the high-level logic of scene conversion to
	a NSI representation.

	\see process_node
*/
void scene::convert_to_nsi( const context &i_context )
{
	/* Start be getting the list of exporter for the scene */
	std::vector<exporter *> to_export;
	scan( i_context, to_export );

	/*
		Create phase. This will create all the NSI nodes from the Houdini
		objects that we support.
	*/
	for( auto &exporter : to_export )
	{
		exporter->create();
	}

	/*
		Now connect nodes together. This has to be done after the create
		so that all the nodes are present.
	*/
	for( auto &exporter : to_export )
	{
		exporter->connect();
	}

	/* Finally, SetAttributes[AtTime], in parallel (FIXME) */
	for( auto &exporter : to_export )
	{
		exporter->set_attributes();
		exporter->set_attributes_at_time( i_context.m_start_time );
	}
}


