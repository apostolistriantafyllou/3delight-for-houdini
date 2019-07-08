#include "scene.h"

#include "camera.h"
#include "context.h"
#include "exporter.h"
#include "light.h"
#include "polygonmesh.h"
#include "null.h"
#include "pointmesh.h"
#include "utilities.h"

#include <OP/OP_Director.h>
#include <SOP/SOP_Node.h>
#include <OBJ/OBJ_Camera.h>
#include <OBJ/OBJ_Node.h>

#include <GT/GT_Refine.h>
#include <GT/GT_GEODetail.h>

/**
	A GT refiner for an OBJ_Node
*/
struct OBJ_Node_Refiner : public GT_Refine
{
	OBJ_Node *m_node;
	std::vector<exporter *> &m_result;
	const context &m_context;

	OBJ_Node_Refiner(
		OBJ_Node *i_node,
		const context &i_context,
		std::vector< exporter *> &i_result )
	:
		m_result(i_result),
		m_node(i_node),
		m_context(i_context)
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
		default:
			std::cout << "Unsupported object " << m_node->getFullPath() <<
				" of type " << i_primitive->className() << std::endl;
			break;
		}
	}
};

/**
	\brief Decide what to do with the given OP_Node.

	NULLs are output, always. They are the internal nodes of our scene
	tree. Other than that, we output any obj that is a OBJ_GEOMETRY
	and that is renderable in the current frame range.
*/
void scene::process_obj(
	const context &i_context,
	OP_Node *i_node,
	std::vector<exporter *> &o_to_export )
{
	OBJ_Node *obj = i_node->castToOBJNode();

	if( !obj )
		return;

	/*
		Each object is its own null transform. This could produce useless
		transforms if we decide not to output the node after all but I am
		aiming for code simplicity here.
	*/
	GT_PrimitiveHandle empty;
	o_to_export.push_back( new null(i_context, obj, empty) );

	if( obj->getObjectType() & OBJ_NULL )
	{
		return;
	}

	if( obj->castToOBJLight() )
	{
		o_to_export.push_back( new light(i_context, obj, empty) );

		/*
			We don't return here because an OBJ_Light is also and OBJ_Camera
			and we want to export both. This will alow us, for example, to
			render the scene from the point of view of a light source if
			that's what the user wishes.
		*/
	}

	if( obj->castToOBJCamera() )
	{
		o_to_export.push_back( new camera(i_context, obj, empty) );
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
	\brief Scans for geometry, cameras, lights, etc...
*/
void scene::scan_geo_network(
	const context &i_context,
	OP_Network *i_network,
	std::vector< exporter * > &o_to_export )
{
	/* A traversal stack to abvoid recursion */
	std::vector< OP_Network * > traversal;
	traversal.push_back( i_network );
	i_network = nullptr; // avoid typos below.

	/*
		After this while loop, to_export will be filled with the OBJs
		to export.
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
			process_obj( i_context, node, o_to_export );

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
*/
void scene::export_scene( const context &i_context )
{
	OP_Node *our_dear_leader = OPgetDirector();
	std::vector<exporter *> to_export;

	scan_geo_network(
		i_context,
		(OP_Network*)our_dear_leader->findNode( "/obj"),
		to_export );

	NSI::Context &nsi = i_context.m_nsi;

	/*
		Create phase. This will create all the NSI nodes from the Houdini
		objects that we support.

		\see process_obj() to see which objects end up here.
	*/
	for( auto &exporter : to_export )
	{
		exporter->create();
	}

	/*
		Connect to parents. This will effectively create the entire scene
		hierarchy in one go.
	*/
	for( auto exporter : to_export )
	{
#if 0
		if( exporter->instanced() )
		{
			/*
				This object will be referenced by an instancer, we don't
				need to connect it anywhere.
			*/
			continue;
		}
#endif

		/*
			Parent in scene hierarchy. The parent is already created by
			create loop above.
		*/
		const OBJ_Node *parent = exporter->parent();

		if( parent )
		{
			nsi.Connect(
				exporter->handle().c_str(), "",
				parent->getFullPath().buffer(), "objects" );
		}
		else
		{
			nsi.Connect(
				exporter->handle().c_str(),
				"", NSI_SCENE_ROOT, "objects" );
		}
	}

	/* Finally, SetAttributes[AtTime], in parallel (FIXME) */
	for( auto &exporter : to_export )
	{
		exporter->set_attributes();
		exporter->set_attributes_at_time( i_context.m_start_time );
	}
}


