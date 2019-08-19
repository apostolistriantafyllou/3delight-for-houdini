#include "scene.h"

/* exporters { */
#include "camera.h"
#include "curvemesh.h"
#include "exporter.h"
#include "instance.h"
#include "light.h"
#include "polygonmesh.h"
#include "null.h"
#include "pointmesh.h"
#include "vop.h"
/* } */

#include "context.h"

#include <GT/GT_GEODetail.h>
#include <GT/GT_Refine.h>
#include <GT/GT_RefineParms.h>
#include <GT/GT_PrimInstance.h>
#include <OBJ/OBJ_Camera.h>
#include <OBJ/OBJ_Node.h>
#include <OP/OP_BundlePattern.h>
#include <OP/OP_Director.h>
#include <SOP/SOP_Node.h>
#include <UT/UT_String.h>
#include <UT/UT_TagManager.h>

#include <set>

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

	/**
		The only interesting thing here is how we deal with instances. We first
		addPrimitive() recursively to resolve the instanced geometry since we
		need it's handle to pass to the actual instancer. All the rest is
		is 1 to 1 mapping with either one of our expoters.
	*/
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

		case GT_PRIM_SUBDIVISION_CURVES:
			m_result.push_back(
				new curvemesh(m_context, m_node,i_primitive) );
			break;

		case GT_PRIM_CURVE_MESH:
			m_result.push_back(
				new curvemesh(m_context, m_node,i_primitive) );
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
				std::cerr
					<< "3Delight for Houdini: unable to create instanced geometry for "
					<< m_node->getFullPath() << std::endl;
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
#ifdef VERBOSE
			std::cout << "Refining " << m_node->getFullPath() <<
				" to level " << m_level  << std::endl;
#endif
			GT_RefineParms params;
			params.setAllowSubdivision( true );

			OBJ_Node_Refiner refiner( m_node, m_context, m_result, m_level+1 );
			if(	i_primitive->refine( refiner, &params ) )
			{
				std::cerr << "3Delight for Houdini: unsupported object "
					<< m_node->getFullPath()
					<< " of class " << i_primitive->className()
					<< std::endl;
			}
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

#if 0
	if( !obj->isObjectRenderable(i_context.m_start_time) )
		return;
#endif

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
		if(i_context.m_lights_to_render_pattern->match(obj, nullptr, true))
		{
			o_to_export.push_back( new light(i_context, obj) );
		}

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

	if(!i_context.m_objects_to_render_pattern->match(obj, nullptr, true))
	{
		return;
	}

	SOP_Node *sop = obj->getRenderSopPtr();

	if( !sop )
	{
		std::cerr
			<< "3Delight for Houdini: no render SOP for "
			<< obj->getFullPath() << std::endl;
		return;
	}

	/* FIXME: motion blur */
	OP_Context context( i_context.m_start_time );
	const GU_ConstDetailHandle detail_handle( sop->getCookedGeoHandle(context) );

	if( !detail_handle.isValid() )
	{
		std::cerr
			<< "3Delight for Houdini: " << obj->getFullPath()
			<< " has no valid detail" << std::endl;
		return;
	}

	std::vector< exporter *> gt_primitives;

	GT_PrimitiveHandle gt( GT_GEODetail::makeDetail(detail_handle) );
	OBJ_Node_Refiner refiner( obj, i_context, gt_primitives );
	gt->refine( refiner, nullptr );

#ifdef VERBOSE
	std::cout << obj->getFullPath() << " gave birth to " <<
		gt_primitives.size() << " primitives." << std::endl;
#endif

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

	/*
		Scene export is done, with the exception of light linking.

		Remember "lightcategories" expressions that already have a matching NSI
		"set" node in the scene. All lights are on by default, so light linking
		is used to turn them off. This requires that the NSI sets we have
		exported contains the *complement* of their corresponding expression.
		Without this set, we would have to test every object's light categories
		expression against every light's categories list, which would be a
		waste because those expressions tend be re-used on multiple objects.
	*/
	std::set<std::string> exported_lights_categories;

	std::vector<OBJ_Node*> lights_to_render;
	find_lights( *i_context.m_lights_to_render_pattern, lights_to_render );

	for( auto &exporter : to_export )
	{
		export_light_categories(
			i_context.m_nsi,
			exporter,
			exported_lights_categories,
			lights_to_render );
	}
}

void scene::find_lights(
	const OP_BundlePattern &i_light_pattern,
	std::vector<OBJ_Node*>& o_lights )
{
	/* A traversal stack to avoid recursion */
	std::vector< OP_Node * > traversal;

	OP_Node *our_dear_leader = OPgetDirector();
	traversal.push_back( our_dear_leader->findNode( "/obj") );

	while( traversal.size() )
	{
		OP_Node *network = traversal.back();
		traversal.pop_back();

		assert( network->isNetwork() );

		int nkids = network->getNchildren();
		for( int i=0; i< nkids; i++ )
		{
			OP_Node *node = network->getChild(i);
			OBJ_Node *obj = node->castToOBJNode();
			if( obj && obj->castToOBJLight() )
			{
				if( i_light_pattern.match(obj, nullptr, true))
				{
					o_lights.push_back(obj);
				}
			}

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

static const std::string k_light_category_prefix = ".!@category:";

void scene::export_light_categories(
	NSI::Context &i_nsi,
	exporter *i_exporter,
	std::set<std::string> &io_exported_lights_categories,
	const std::vector<OBJ_Node*> &i_lights_to_render )
{
	assert( i_exporter );

	OBJ_Node *i_object = i_exporter->obj();

	UT_String categories;
	int lightcategories_index = i_object->getParmIndex("lightcategories");
	if(lightcategories_index < 0)
	{
		// Light linking is not available for this object
		return;
	}

	i_object->evalString(categories, lightcategories_index, 0, 0.0f);
	if(!categories.c_str())
	{
		// Light linking is not available for this object
		return;
	}

	UT_TagManager tag_manager;
	UT_String errors;
	UT_TagExpressionPtr categories_expr =
		tag_manager.createExpression(categories, errors);

	// Trivial and (hopefully) most common case
	if(categories_expr->isTautology())
	{
		/*
			The object sees all lights, which is the default, so no connections
			are necessary (ie : no lights to turn off).
		*/
		return;
	}

	std::string cat_handle = k_light_category_prefix + categories.toStdString();
	std::string cat_attr_handle = cat_handle + "|attributes";

	auto insertion = io_exported_lights_categories.insert(cat_handle);
	if(insertion.second)
	{
		/*
			This is the first time we use this NSI set, so we have to create it
			first!
		*/
		i_nsi.Create(cat_handle, "set");
		i_nsi.Create(cat_attr_handle, "attributes");
		i_nsi.Connect(cat_attr_handle, "", cat_handle, "geometryattributes");

		for(OBJ_Node* light : i_lights_to_render)
		{
			/*
				Parse the list of categories for the light. This shouldn't be
				expensive. If it is, we might want to store it in the light
				exporter and work on a list of those, instead.
			*/
			UT_String tags_string;
			light->evalString(tags_string, "categories", 0, 0.0);
			UT_TagListPtr tags = tag_manager.createList(tags_string, errors);

			// Add the lights we have to turn off to the set
			if(!tags->match(*categories_expr))
			{
				i_nsi.Connect(
					light->getFullPath().toStdString(), "",
					cat_handle, "members");
			}
		}
	}

	std::string attributes_handle( i_exporter->handle() );
	attributes_handle += "|attributes";
	i_nsi.Create( attributes_handle, "attributes");
	i_nsi.Connect(
		attributes_handle, "", i_exporter->handle(), "geometryattributes" );

	// Turn off lights in the set for this object
	i_nsi.Connect(
		attributes_handle, "",
		cat_attr_handle, "visibility",
		(
			NSI::IntegerArg("value", false),
			NSI::IntegerArg("priority", 1)
		) );
}
