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

namespace
{
	/**
		\brief Utility exporter that simply deletes its associated NSI node(s).

		It's used to ensure that animated objects are deleted before being
		re-exported after a time change. We can't count on the exporters
		allocated inside process_obj_node to do that, because some of them will
		be missing if they just became invisible.

		The deleter class being a template (and the various exporters' "Delete"
		method being static), allows each exporter class to delete their NSI
		nodes properly, using the right handle and the correct "recursive" flag.
	*/
	template<typename node_exporter>
	struct deleter : public exporter
	{
		deleter(const context& i_context, OBJ_Node* i_node)
			:	exporter(i_context, i_node)
		{
			assert(m_object);
		}

		void create()const override
		{
			node_exporter::Delete(*m_object, m_context);
		}

		void set_attributes()const override {}
		void connect()const override {}
	};
}


/**
	\brief Decide what to do with the given OP_Node.

	\param i_re_export_instanced.
		= true if this run is for re exporting an instanced object that was
		ignored beause it was invisible. We find out about such objects later
		in the pipeline. When re-exportig, we don't check Display flag /
		Objects to Render (as this would return 'false') and we don't need
		to export the null transform at the top because it has already been
		exported in the first run. \ref scan_for_instanced

	Houdini's OBJ nodes will correspond to NSI transform. So we insert
	a null exported for each one of these.

	Other than that, we output any obj that is a OBJ_GEOMETRY
	and that is renderable in the current frame range and obey to Display
	flag / Objects to Render.
*/
void scene::process_obj_node(
	const context &i_context,
	OBJ_Node *obj,
	bool i_re_export_instanced,
	std::vector<exporter *> &o_to_export )
{
	/*
		We register callbacks so we can react to parameter changes in IPR, but
		we don't need to register them when we're re-scanning the scene after a
		time change.
	*/
	bool register_callbacks = i_context.m_ipr && !i_context.m_time_dependent;

	/*
		Each object is its own null transform. When re-exporting an invisible
		object that was tagged as an instance, we don't need to output the
		null exported as it is already present since the first scene
		scan.
	*/

	if( !i_re_export_instanced )
	{
		bool time_dependent_obj =
			time_sampler::is_time_dependent(
				*obj,
				i_context,
				time_sampler::e_transformation);

		/*
			We export non-animated nodes the first time and time-dependent nodes
			each time.
		*/
		bool needs_export = !i_context.m_time_dependent || time_dependent_obj;

		if(needs_export)
		{
			o_to_export.push_back( new null(i_context, obj) );
		}

		if(register_callbacks)
		{
			i_context.register_interest(obj, &null::changed_cb);
		}
	}

	if( obj->getObjectType() & OBJ_NULL )
	{
		return;
	}

	bool check_visibility = !i_re_export_instanced;

	bool visible = !check_visibility || i_context.object_displayed(*obj);

	bool is_incand = obj->getOperator()->getName().toStdString() ==
		"3Delight::IncandescenceLight" && i_context.object_displayed(*obj);

	bool time_dependent_obj =
		time_sampler::is_time_dependent(
			*obj,
			i_context,
			time_sampler::e_deformation);
	bool needs_delete = i_context.m_time_dependent && time_dependent_obj;
	bool needs_export = !i_context.m_time_dependent || time_dependent_obj;

	if( obj->castToOBJLight() || is_incand)
	{
		if( is_incand && !i_re_export_instanced)
		{
			/*
				We don't need the null. The incandescence light will create a
				set. \ref incandescence_light
			*/
			delete o_to_export.back();
			o_to_export.pop_back();
		}

		if(needs_delete)
		{
			o_to_export.push_back(
				is_incand
				?	(exporter*)new deleter<incandescence_light>(i_context, obj)
				:	(exporter*)new deleter<light>(i_context, obj));
		}
		if(needs_export && visible && i_context.m_rop_type != rop_type::stand_in)
		{
			o_to_export.push_back(
				is_incand
					?	(exporter*)new incandescence_light(i_context, obj)
					:	(exporter*)new light(i_context, obj) );
		}

		if(register_callbacks)
		{
			i_context.register_interest( obj,
				is_incand ?
					&incandescence_light::changed_cb :
					&light::changed_cb );
		}

		/*
			We normally don't return here because an OBJ_Light is also an
			OBJ_Camera and we might be trying to render the scene from the point
			of view of a light source.

			However, this can never be true when we are exporting an archive, so
			let's avoid exporting a flurry of unused cameras in that case!
		*/
		if(i_context.m_rop_type == rop_type::stand_in)
		{
			return;
		}
	}

	if( obj->castToOBJCamera() && !is_incand )
	{
		/*
			For now, we export all cameras because we don't know which ones are
			required, even though they might not be part of the "objects to
			render" bundle or their display flag might be turned off.
		*/
		if(needs_delete)
		{
			o_to_export.push_back( new deleter<camera>(i_context, obj) );
		}
		if(needs_export)
		{
			o_to_export.push_back( new camera(i_context, obj) );
		}

		if(register_callbacks)
		{
			i_context.register_interest(obj, &camera::changed_cb);
		}
		return;
	}

	if(!obj->castToOBJGeometry() )
	{
		return;
	}

	SOP_Node *sop = obj->getRenderSopPtr();
	if(needs_delete)
	{
		o_to_export.push_back(new deleter<geometry>(i_context, obj));
	}
	if(needs_export && sop && visible)
	{
		o_to_export.push_back( new geometry(i_context, obj) );
	}
	if(register_callbacks)
	{
		// Watch for OBJ-level changes
		i_context.register_interest(obj, &geometry::changed_cb);
		if(sop)
		{
			/*
				Watch for SOP-level changes. If there isn't a render SOP yet,
				the connection will be made later, when the render SOP change
				is trapped in geometry::changed_cb.
			*/
			i_context.register_interest(sop, &geometry::sop_changed_cb);
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
	\brief Returns all the vops traversed by a set of materials

	\param i_materials
		List of materials paths to explore for VOPs
	\param o_vops
		VOPs produced during scan
*/
void scene::get_material_vops(
	const std::unordered_set<std::string>& i_materials,
	std::vector<VOP_Node*> &o_vops )
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

		o_vops.push_back( node );

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
	std::vector<VOP_Node *> vops;
	get_material_vops( i_materials, vops );

	for( auto &V : vops )
	{
		if(i_context.m_ipr)
		{
			i_context.register_interest(V, &vop::changed_cb);
		}
		io_to_export.push_back( new vop(i_context,V) );
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
	ROP_Node *rop = (ROP_Node *)i_context.rop();

	assert( rop );

	if( !rop )
		return;

	int index;
	if( (index=rop->getParmIndex("atmosphere")) == -1 )
		return;

	ROP_3Delight *r3 = (ROP_3Delight *) CAST_ROPNODE( rop );
	UT_String atmosphere_path;
	rop->evalString( atmosphere_path, "atmosphere", 0, r3->current_time() );

	if( atmosphere_path.length() > 0 )
	{
		VOP_Node *mats[3] = { nullptr };
		exporter::resolve_material_path( r3, atmosphere_path.c_str(), mats );

		VOP_Node *atmosphere_shader = mats[2]; // volume
		if( atmosphere_shader )
		{
			std::unordered_set<std::string> mat;
			mat.insert(atmosphere_shader->getFullPath().toStdString());
			create_materials_exporters(mat, i_context, io_to_export);
		}
	}
}

/**
	\brief Scans for geometry, cameras, lights and produces a list of exporters.

	We just scan the scene recursively, looking for OBJ nodes. We skip  SOPs and
	DOPs and TOPs.

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

			if( node->castToTOPNode() || node->castToSOPNode() ||
				node->castToDOPNode()  )
			{
				continue;
			}

			OBJ_Node *obj = node->castToOBJNode();

			if( obj )
			{
				process_obj_node( i_context, obj, false, o_to_export );
			}

			traversal.push_back( node );
		}
	}
}

/**
	\brief Export instanced objects that might have been skipped because
	of visibility.
*/
void scene::scan_for_instanced(
	const context &i_context,
	std::vector<exporter *> &io_to_export )
{
	std::unordered_set< std::string > instanced;

	/*
		For each geometry, gather its instances and from there
		gather the instanced geometries.

		Note that light sources could also have an instanced
		geometry as they can reference a geometry object.
	*/
	for( const auto &E : io_to_export )
	{
		light *L = nullptr;
		geometry *G = dynamic_cast<geometry *>( E );

		if( G )
		{
			std::vector< const instance * > instances;
			G->get_instances( instances );

			for( auto I : instances )
				I->get_instanced( instanced );
		}
		else if( (L=dynamic_cast<light *>(E)) )
		{
			std::string geo = L->get_geometry_path();

			if( !geo.empty() )
				instanced.insert( geo );
		}
	}

	if( instanced.empty() )
		return;

	for( const auto E : io_to_export )
	{
		/*
			Only interested in null which represents the obj with
			getFullPathName().
		*/

		if( !dynamic_cast<null*>(E) )
			continue;

		/*
			Get the path of E. We can't use E->handle() as in IPR this
			could be a unique ID and not a path.
		*/
		std::string E_path( E->node()->getFullPath().toStdString() );
		auto it = instanced.find( E_path );
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
		OBJ_Node *o = OPgetDirector()->findOBJNode( E.c_str() );
		if( !o )
		{
			assert( false );
			continue;
		}

		process_obj_node(
			i_context, o, true /* re-export instance */, io_to_export );

		/*
		   Finally, make sure we don't render the source geometry as its
		   only rendered through instancing.
		*/
		for( int i=0; i<io_to_export.size(); i++ )
		{
			if( !dynamic_cast<null*>(io_to_export[i]))
				continue;

			if( io_to_export[i]->node()->getFullPath().toStdString() == E )
			{
				io_to_export[i]->set_as_instanced();
				break;
			}
		}
	}
}

/**
	\brief Converts a scene into exporters.
	\see process_obj_node
*/
void scene::create_exporters(
	const context &i_context,
	std::vector<exporter*> &o_to_export )
{
	assert( i_context.rop() );

	/*
		Start by getting the list of all OBJ exporters.
	*/
	obj_scan( i_context, o_to_export );

	/*
		Make sure instanced geometry is included in the list, regardless of
		display flag or scene elements.
	*/
	scan_for_instanced( i_context, o_to_export );

	/*
		Now, for the OBJs that are geometries, gather the list of materials and
		build a list of VOP exporters for these.
	*/
	vop_scan( i_context, o_to_export );
}

/**
	\brief Contains the high-level logic of scene conversion to
	a NSI representation.
*/
void scene::convert_to_nsi( const context &i_context )
{
	/*
		Start by getting the list of all OBJ exporters.
	*/
	std::vector<exporter *> to_export;
	create_exporters( i_context, to_export );

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
	context* ctx;
	if( rop )
	{
		ROP_3Delight *r3 = (ROP_3Delight *) CAST_ROPNODE( rop );
		time = r3 ? r3->current_time() : 0.0;
		if (r3)
		{
			ctx = new context(r3, time);
		}
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
					!vdb_file::node_is_vdb_loader(obj, time).empty()) )
			{
				if(!i_light_pattern ||
					i_light_pattern->match(obj, i_rop_path, true))
				{
					o_lights.push_back(obj);
				}
			}

			if( obj &&
				i_want_incandescence_lights &&
				ctx && ctx->object_displayed(*obj) &&
				obj->getOperator()->getName() ==
					"3Delight::IncandescenceLight" )
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

/*
	\brief find the "AVOGroup" in the scene.

	This is relly heavy duty as it uses a scene export to find the vops.
	But it is also robust as we use the same logic as scen export.
*/
void scene::find_custom_aovs(
	ROP_3Delight *i_node,
	double i_time,
	std::vector<VOP_Node*>& o_custom_aovs )
{
	assert( i_node );
	context ctx( i_node, i_time );

	/*
		Start by getting the list of all OBJ exporters.
	*/
	std::vector<exporter *> exporters;
	create_exporters( ctx, exporters );

	for( auto exporter : exporters )
	{
		vop *v = dynamic_cast<vop *>( exporter );
		if( !v )
			continue;

		VOP_Node *vop_node = CAST_VOPNODE( v->node() );
		assert( vop_node);

		if( vop::is_aov_definition(vop_node) )
			o_custom_aovs.push_back( vop_node );

		delete exporter;
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
				i_context.lights_to_render(),
				i_context.rop()->getFullPath().c_str(),
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

		for(OBJ_Node* light_source : io_lights_to_render)
		{
			/*
				Parse the list of categories for the light. This shouldn't be
				expensive. If it is, we might want to store it in the light
				exporter and work on a list of those, instead.
			*/
			UT_String tags_string;
			light_source->evalString(tags_string, "categories", 0, 0.0);
			UT_TagListPtr tags = tag_manager.createList(tags_string, errors);

			// Add the lights we have to turn off to the set
			if(!tags->match(*categories_expr))
			{
				nsi.Connect(
					light::handle(*light_source, i_context), "",
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
