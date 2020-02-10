#include "scene.h"

/* exporters { */
#include "camera.h"
#include "exporter.h"
#include "geometry.h"
#include "light.h"
#include "null.h"
#include "vop.h"
#include "vdb.h"
/* } */

#include "context.h"
#include "safe_interest.h"

#include <GEO/GEO_Normal.h>
#include <OBJ/OBJ_Camera.h>
#include <OBJ/OBJ_Node.h>
#include <OP/OP_BundlePattern.h>
#include <OP/OP_Director.h>
#include <SOP/SOP_Node.h>
#include <UT/UT_String.h>
#include <UT/UT_TagManager.h>
#include <VOP/VOP_Node.h>

#include <set>


/**
	\brief Decide what to do with the given OP_Node.

	NULLs are output, always. They are the internal nodes of our scene
	tree. Other than that, we output any obj that is a OBJ_GEOMETRY
	and that is renderable in the current frame range.
*/
void scene::process_node(
	const context &i_context,
	OP_Node *i_node,
	std::vector<exporter *> &o_to_export,
	std::deque<safe_interest>& io_interests )
{
	if( i_node->castToVOPNode() )
	{
		/*
		   Our material network connections is done :) The connection
		   between nodes is done in \ref vop::connect
		   Read vop.cpp if you are bewildered by this simplicity.
		*/
		o_to_export.push_back(
			new vop(i_context, i_node->castToVOPNode()) );
		if(i_context.m_ipr)
		{
			io_interests.emplace_back(
				i_node,
				const_cast<context*>(&i_context),
				&vop::changed_cb);
		}
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
	if(i_context.m_ipr)
	{
		io_interests.emplace_back(
			i_node,
			const_cast<context*>(&i_context),
			&null::changed_cb);
	}

	if( obj->getObjectType() & OBJ_NULL )
	{
		return;
	}

	const char* rop_path = i_context.m_rop_path.c_str();

	if( obj->castToOBJLight() )
	{
		if(i_context.m_lights_to_render_pattern->match(obj, rop_path, true))
		{
			o_to_export.push_back( new light(i_context, obj) );
			if(i_context.m_ipr)
			{
				io_interests.emplace_back(
					i_node,
					const_cast<context*>(&i_context),
					&light::changed_cb);
			}
		}

		/*
			We don't return here because an OBJ_Light is also an OBJ_Camera
			and we want to export both. This will allow to render the scene
			from the point of view of a light source.
		*/
	}

	if( obj->castToOBJCamera() )
	{
		o_to_export.push_back( new camera(i_context, obj) );
		if(i_context.m_ipr)
		{
			io_interests.emplace_back(
				i_node,
				const_cast<context*>(&i_context),
				&camera::changed_cb);
		}
		return;
	}

	if(!i_context.m_objects_to_render_pattern->match(obj, rop_path, true))
	{
		return;
	}

	SOP_Node *sop = obj->getRenderSopPtr();

	if( !sop )
	{
		return;
	}

	if( obj->castToOBJGeometry() )
	{
		o_to_export.push_back( new geometry(i_context, obj) );
		return;
	}
}

/**
	\brief Scans for geometry, cameras, lights, vops etc... and produces
	a list of exporters.

	\ref exporter
*/
void scene::scan(
	const context &i_context,
	std::vector<exporter *> &o_to_export,
	std::deque<safe_interest>& io_interests )
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

			/* Don't get into SOP networks. Nothing to see there */
			if( node->castToSOPNode() )
				continue;

			process_node( i_context, node, o_to_export, io_interests );

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
void scene::convert_to_nsi(
	const context &i_context,
	std::deque<safe_interest>& io_interests )
{
	/* Start be getting the list of exporter for the scene */
	std::vector<exporter *> to_export;
	scan( i_context, to_export, io_interests );

	/*
		Create phase. This will create all the main NSI nodes from the Houdini
		objects that we support, so that connections can later be made in any
		particular order.
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

	/*
		Finally, set the attributes on each node, possibly creating privately
		managed nodes in the process.
		FIXME : parallel processing?
	*/
	for( auto &exporter : to_export )
	{
		exporter->set_attributes();
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
	find_lights(
		*i_context.m_lights_to_render_pattern,
		i_context.m_rop_path.c_str(),
		lights_to_render );

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
	const char* i_rop_path,
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
			if( obj &&
				(obj->castToOBJLight() ||
					!vdb::node_is_vdb_loader(obj, 0).empty()) )
			{
				if( i_light_pattern.match(obj, i_rop_path, true))
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

void scene::find_custom_aovs( std::vector<VOP_Node*>& o_custom_aovs )
{
	/* A traversal stack to avoid recursion */
	std::vector< OP_Node * > traversal;

	OP_Node *our_dear_leader = OPgetDirector();
	traversal.push_back( our_dear_leader->findNode( "/mat") );
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
			VOP_Node *vop_node = node->castToVOPNode();
			if( vop_node && vop::is_aov_definition(vop_node) )
			{
				o_custom_aovs.push_back(vop_node);
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
