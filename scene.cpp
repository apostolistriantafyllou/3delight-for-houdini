#include "scene.h"

/* exporters { */
#include "camera.h"
#include "exporter.h"
#include "geometry.h"
#include "incandescence_light.h"
#include "instance.h"
#include "light.h"
#include "null.h"
#include "vop.h"
#include "vdb.h"
/* } */

#include "context.h"
#include "object_attributes.h"
#include "safe_interest.h"
#include "ROP_3Delight.h"

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

	\param i_check_visibility
		= true if must check Display flag / Objects to Render.

	Houdini's OBJ nodes will correspond to NSI transform. So we insert
	a null exported for each one of these.

	Other than that, we output any obj that is a OBJ_GEOMETRY
	and that is renderable in the current frame range and obey to Display
	flag / Objects to Render.
*/
void scene::process_obj_node(
	const context &i_context,
	OBJ_Node *obj,
	bool i_check_visibility,
	std::vector<exporter *> &o_to_export )
{
	/*
		Each object is its own null transform. Note that this null transform
		is *needed* if object is invisible because it could potentially have
		visible children.
	*/
	o_to_export.push_back( new null(i_context, obj) );

	if(i_context.m_ipr)
	{
		i_context.m_interests.emplace_back(
			obj,
			const_cast<context*>(&i_context),
			&null::changed_cb);
	}

	if( obj->getObjectType() & OBJ_NULL )
	{
		return;
	}

	if( obj->castToOBJLight() )
	{
		if(!i_check_visibility || i_context.object_displayed(*obj) )
		{
			o_to_export.push_back( new light(i_context, obj) );
		}

		if(i_context.m_ipr)
		{
			i_context.m_interests.emplace_back(
				obj,
				const_cast<context*>(&i_context),
				&light::changed_cb);
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
			i_context.m_interests.emplace_back(
				obj,
				const_cast<context*>(&i_context),
				&camera::changed_cb);
		}
		return;
	}

	bool visible = !i_check_visibility || i_context.object_displayed(*obj);

	/*
		Check for special node IncandescenceLight.
	*/
	if( obj->getOperator()->getName() == "IncandescenceLight" )
	{
		if(visible)
		{
			o_to_export.push_back( new incandescence_light(i_context, obj) );
		}
		if(i_context.m_ipr)
		{
			i_context.m_interests.emplace_back(
				obj,
				const_cast<context*>(&i_context),
				&incandescence_light::changed_cb);
		}
		return;
	}

	if(!obj->castToOBJGeometry() )
	{
		return;
	}

	SOP_Node *sop = obj->getRenderSopPtr();
	if(sop && visible)
	{
		o_to_export.push_back( new geometry(i_context, obj) );
	}
	if(i_context.m_ipr)
	{
		// Watch for OBJ-level changes
		i_context.m_interests.emplace_back(
			obj,
			const_cast<context*>(&i_context),
			&geometry::changed_cb);
		if(sop)
		{
			/*
				Watch for SOP-level changes. If there isn't a render SOP yet,
				the connection will be made later, when the render SOP change
				is trapped in geometry::changed_cb.
			*/
			i_context.m_interests.emplace_back(
				sop,
				const_cast<context*>(&i_context),
				&geometry::sop_changed_cb);
		}
	}
}

/// Inserts a newly created node into an existing NSI scene
void scene::insert_obj_node(
	OBJ_Node& i_node,
	const context& i_context )
{
	std::vector<exporter*> to_export;
	process_obj_node(i_context, &i_node, false, to_export);
	export_nsi(i_context, to_export);
}

/// Exports materials to the context's NSI stream.
void scene::export_materials(
	std::unordered_set<std::string>& i_materials,
	const context& i_context)
{
	std::vector<exporter*> to_export;
	create_materials_exporters(i_materials, i_context, to_export);
	export_nsi(i_context, to_export);
}

/**
	\brief Go through all used materials and produce VOPs exporters.

	We also check the Atmosphere shader in the ROP, as we can't possibly find
	it otherwise.
*/
void scene::vop_scan(
	const context &i_context,
	std::vector<exporter *> &io_to_export )
{
	std::unordered_set< std::string > materials;
	for( auto E : io_to_export )
	{
		geometry *geo = dynamic_cast<geometry*>( E );
		if( !geo )
			continue;
		geo->get_all_material_paths( materials );
	}

	create_materials_exporters( materials, i_context, io_to_export );
	
	/* Deal with atmosphere */
	create_atmosphere_shader_exporter( i_context, io_to_export );
}

/**
	\brief Creates the exporters for all shaders node required by each material.
	
	\param i_materials
		List of materials paths for which to create exporters.
	\param i_context
		Current rendering context.
	\param io_to_export
		New exporters will be appended here.
*/
void scene::create_materials_exporters(
	const std::unordered_set<std::string>& i_materials,
	const context &i_context,
	std::vector<exporter *> &io_to_export )
{
	std::unordered_set<VOP_Node *> vops;
	for( const auto &M : i_materials )
	{
		VOP_Node *mat = OPgetDirector()->findVOPNode( M.data() );

		if( !mat )
			continue;

		vops.insert( mat );
	}

	std::vector<VOP_Node*> traversal;
	for( auto V : vops )
		traversal.push_back( V );

	/* re-purpose this set for de-duplication of VOPs */
	vops.clear();

	while( traversal.size() )
	{
		VOP_Node* node = traversal.back();
		traversal.pop_back();

		if( !node || !vop::is_renderable(node) ||
			vops.find(node) != vops.end() )
		{
			continue;
		}

		vops.insert( node );

		io_to_export.push_back( new vop(i_context, node) );
		if(i_context.m_ipr)
		{
			i_context.m_interests.emplace_back(
				node, const_cast<context*>(&i_context), &vop::changed_cb);
		}

		int ninputs = node->nInputs();
		for( int i = 0; i < ninputs; i++ )
		{
			VOP_Node *input = CAST_VOPNODE( node->getInput(i) );
			if( !input )
				continue;
			traversal.push_back( input );
		}
	}
}

/**
	\brief Creates the exporters required by the atmosphere shader.

	\param i_context
		Current rendering context.
	\param io_to_export
		New exporters will be appended here.
*/
void scene::create_atmosphere_shader_exporter(
	const context& i_context,
	std::vector<exporter *>& io_to_export )
{
	OP_Node *rop = OPgetDirector()->findNode( i_context.m_rop_path.c_str() );
	if( !rop )
	{
		return;
	}

	int index;
	if( (index=rop->getParmIndex("atmosphere")) == -1 )
		return;

	ROP_3Delight *r3 = (ROP_3Delight *) CAST_ROPNODE( rop );
	UT_String atmosphere_path;
	rop->evalString( atmosphere_path, "atmosphere", 0, r3->current_time() );

	if( atmosphere_path.length() > 0 )
	{
		std::string resolved;
		VOP_Node *atmosphere_shader =
			exporter::resolve_material_path(
				r3, atmosphere_path.c_str(), resolved );

		if( atmosphere_shader )
		{
			std::unordered_set<std::string> mat;
			mat.insert(resolved);
			create_materials_exporters(mat, i_context, io_to_export);
		}
	}
}

/**
	\brief Scans for geometry, cameras, lights and produces a list of exporters.

	We just scan the scene recursively, looking for objects.
	FIXME: can we do better and avoid scanning through unnecessary nodes ?

	\ref geometry
*/
void scene::obj_scan(
	const context &i_context,
	std::vector<exporter *> &o_to_export )
{
	std::vector<OP_Node *> traversal;
	traversal.push_back( OPgetDirector()->findNode("/obj") );

	while( traversal.size() )
	{
		OP_Node *current = traversal.back();
		traversal.pop_back();

		int nkids = current->getNchildren();
		for( int i=0; i< nkids; i++ )
		{
			OP_Node *node = current->getChild(i);
			OBJ_Node *obj = node->castToOBJNode();

			if( obj )
			{
				process_obj_node( i_context, obj, true, o_to_export );
			}

			traversal.push_back( node );
		}
	}
}

/**
	\brief Export instanced objects that might have been skipped because
	of visiblity.
*/
void scene::scan_for_instanced(
	const context &i_context,
	std::vector<exporter *> &io_to_export )
{
	std::unordered_set< std::string > instanced;

	/*
		For each geometry, gather its instances and from there
		gather the instanced geometries.
	*/
	for( const auto &E : io_to_export )
	{
		geometry *G = dynamic_cast<geometry *>( E );
		if( !G )
			continue;

		std::vector< const instance * > instances;
		G->get_instances( instances );

		for( auto I : instances )
			I->get_instanced( instanced );
	}

	if( instanced.empty() )
		return;

	for( const auto E : io_to_export )
	{
		auto it = instanced.find( E->handle() );
		if( it == instanced.end() )
			continue;

		OP_Node *node = E->node();
		OBJ_Node *obj = CAST_OBJNODE( node );
		if( !obj )
		{
			/* shouldn't be part of instanced! */
			assert( false );
			continue;
		}

		if( i_context.object_displayed(*obj) )
		{
			instanced.erase( it );
		}
	}

	/*
		"instanced" now contains all the objects that are instanced but which
		have not been exported (due to the Display flag or Scene Elements ->
		Objects to Render).
	*/
	for( auto E : instanced )
	{
		OBJ_Node *o = OPgetDirector()->findOBJNode( E.data() );
		if( !o )
			continue;

		int s = io_to_export.size(); assert( s>0 );
		process_obj_node( i_context, o, false, io_to_export );

		/*
		   Finally, make sure we don't render the source geometry as its
		   only rendered through instancing.
		*/
		for( int i=s-1; i<io_to_export.size(); i++ )
		{
			if( io_to_export[i]->handle() == E )
				io_to_export[i]->set_as_instanced();
		}
	}
}

/**
	\brief Contains the high-level logic of scene conversion to
	a NSI representation.

	\see process_obj_node
*/
void scene::convert_to_nsi( const context &i_context )
{
	/*
		Start by getting the list of all OBJ exporters.
	*/
	std::vector<exporter *> to_export;
	obj_scan( i_context, to_export );

	/*
		Make sure instanced geometry is included in the list, regardless of
		display flag or scene elements.
	*/
	scan_for_instanced( i_context, to_export );

	/*
		Now, for the OBJs that are geometries, gather the list of materials and
		build a list of VOP exporters for these.
	*/
	vop_scan( i_context, to_export );

	export_nsi(i_context, to_export);
}

/// Run the exporters to export NSI nodes and their attributes
void scene::export_nsi(
	const context &i_context,
	const std::vector<exporter*>& i_to_export)
{
	/*
		Create phase. This will create all the main NSI nodes from the Houdini
		objects that we support, so that connections can later be made in any
		particular order.
	*/
	for( auto &exporter : i_to_export )
	{
		exporter->create();
	}

	/*
		Now connect nodes together. This has to be done after the create
		so that all the nodes are present.
	*/
	for( auto &exporter : i_to_export )
	{
		exporter->connect();
	}

	/*
		Finally, set the attributes on each node, possibly creating privately
		managed nodes in the process.
		FIXME : parallel processing?
	*/
	for( auto &exporter : i_to_export )
	{
		exporter->set_attributes();
	}

	/*
		Scene export is done, with the exception of light linking and matte
		objects

		Remember "lightcategories" expressions that already have a matching NSI
		"set" node in the scene. All lights are on by default, so light linking
		is used to turn them off. This requires that the NSI sets we have
		exported contains the *complement* of their corresponding expression.
		Without this set, we would have to test every object's light categories
		expression against every light's categories list, which would be a
		waste because those expressions tend be re-used on multiple objects.
	*/
	std::set<std::string> exported_lights_categories;

	/*
		This stores the list of lights that are part of the rendered scene.
		This is not always required, since some (hopefully, many) objects see
		all lights and don't need to connect to light categories. This is
		especially true in IPR where we usually re-export only one object. So,
		it's filled lazily, at most once, by export_light_categories.
	*/
	std::vector<OBJ_Node*> lights_to_render;
	
	for( auto &exporter : i_to_export )
	{
		export_light_categories(
			i_context,
			exporter,
			exported_lights_categories,
			lights_to_render );
	}

	for( auto &exporter : i_to_export )
	{
		delete exporter;
	}
}

/**
	\brief Find all renderable lights in the scene, as well as matte
	objects.

	\param i_light_pattern
		If not null, only lights that match this pattern will be considered as
		renderable.
	\param i_rop_path
		Path of the 3Delight ROP, to be used with i_light_pattern.
	\param o_lights
		Will be filled with the scene's renderable light sources.
*/
void scene::find_lights(
	const OP_BundlePattern* i_light_pattern,
	const char* i_rop_path,
	bool i_want_incandescence_lights,
	std::vector<OBJ_Node*>& o_lights )
{
	/* A traversal stack to avoid recursion */
	std::vector< OP_Node * > traversal;

	OP_Node *director = OPgetDirector();
	traversal.push_back( director->findNode( "/obj") );

	/* FIXME: not a nice way to access current time. */
	double time = 0;
	OP_Node *rop = director->findNode( i_rop_path );
	if( rop )
	{
		ROP_3Delight *r3 = (ROP_3Delight *) CAST_ROPNODE( rop );
		time = r3 ? r3->current_time() : 0.0;
	}

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
					!vdb::node_is_vdb_loader(obj, time).empty()) )
			{
				if(!i_light_pattern ||
					i_light_pattern->match(obj, i_rop_path, true))
				{
					o_lights.push_back(obj);
				}
			}

			if( obj &&
				i_want_incandescence_lights &&
				obj->getOperator()->getName() == "IncandescenceLight" )
			{
				o_lights.push_back(obj);
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
	const context &i_context,
	exporter *i_exporter,
	std::set<std::string> &io_exported_lights_categories,
	std::vector<OBJ_Node*> &io_lights_to_render )
{
	assert( i_exporter );

	OP_Node *node = i_exporter->node();

	OBJ_Node *i_object = CAST_OBJNODE( node );
	if( !i_object )
	{
		return;
	}

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

	NSI::Context& nsi = i_context.m_nsi;
	
	std::string cat_handle = k_light_category_prefix + categories.toStdString();
	std::string cat_attr_handle = cat_handle + "|attributes";

	auto insertion = io_exported_lights_categories.insert(cat_handle);
	if(insertion.second)
	{
		// Fill io_lights_to_render lazily since it won't always be needed
		if(io_lights_to_render.empty())
		{
			find_lights(
				i_context.m_lights_to_render_pattern,
				i_context.m_rop_path.c_str(),
				false,
				io_lights_to_render);
		}

		/*
			This is the first time we use this NSI set, so we have to create it
			first!
		*/
		nsi.Create(cat_handle, "set");
		nsi.Create(cat_attr_handle, "attributes");
		nsi.Connect(cat_attr_handle, "", cat_handle, "geometryattributes");

		for(OBJ_Node* light : io_lights_to_render)
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
				nsi.Connect(
					light->getFullPath().toStdString(), "",
					cat_handle, "members");
			}
		}
	}

	std::string attributes_handle( i_exporter->handle() );
	attributes_handle += "|attributes";
	nsi.Create( attributes_handle, "attributes");
	nsi.Connect(
		attributes_handle, "", i_exporter->handle(), "geometryattributes" );

	// Turn off lights in the set for this object
	nsi.Connect(
		attributes_handle, "",
		cat_attr_handle, "visibility",
		(
			NSI::IntegerArg("value", false),
			NSI::IntegerArg("priority", 1)
		) );
}
