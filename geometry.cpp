#include "geometry.h"

#include "context.h"
#include "curvemesh.h"
#include "instance.h"
#include "null.h"
#include "object_attributes.h"
#include "polygonmesh.h"
#include "pointmesh.h"
#include "safe_interest.h"
#include "scene.h"
#include "time_sampler.h"
#include "vdb.h"
#include "vop.h"
#include "shader_library.h"

#include <GT/GT_GEODetail.h>
#include <GT/GT_PrimInstance.h>
#include <GT/GT_PrimVolume.h>
#include <GT/GT_Refine.h>
#include <GT/GT_RefineParms.h>
#include <GU/GU_ConvertParms.h>
#include <GU/GU_PrimVDB.h>
#include <OBJ/OBJ_Node.h>
#include <OP/OP_Operator.h>
#include <VOP/VOP_Node.h>
#include <SOP/SOP_Node.h>
#include <SYS/SYS_Version.h>
#include <UT/UT_TempFileManager.h>

#include <iostream>
#include <algorithm>


namespace
{

/**
	\brief A GT refiner for an OBJ_Node.

	When an OBJ_Node is refined, the top node is a null to which
	we will connect all newly created primitives.
*/
struct OBJ_Node_Refiner : public GT_Refine
{
	/*
		Parameters used for refinement. We store them here so they can be easily
		re-used for recursive calls.
	*/
	GT_RefineParms m_params;

	OBJ_Node *m_node;

	/*
		Detail handle from which refinement was initiated. We store it wo we
		can restart refinement from the top with different parameters.
	*/
	const GU_DetailHandle& m_gu_detail;

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

	const context &m_context;
	double m_time;
	int m_level;
	bool m_stop;

	OBJ_Node_Refiner(
		OBJ_Node *i_node,
		const GU_DetailHandle& i_gu_detail,
		const context &i_context,
		double i_time,
		std::vector<primitive*> &io_result)
	:
		m_node(i_node),
		m_gu_detail(i_gu_detail),
		m_result(io_result),
		m_context(i_context),
		m_time(i_time),
		m_level(0),
		m_stop(false)
	{
		m_params.setAllowSubdivision( true );
		m_params.setAddVertexNormals( true );
		m_params.setCuspAngle( GEO_DEFAULT_ADJUSTED_CUSP_ANGLE );
	}

	// Constructor used for recursive calls
	explicit OBJ_Node_Refiner(const OBJ_Node_Refiner* i_parent)
	:	m_params(i_parent->m_params),
		m_node(i_parent->m_node),
		m_gu_detail(i_parent->m_gu_detail),
		m_result(i_parent->m_result),
		m_context(i_parent->m_context),
		m_time(i_parent->m_time),
		m_level(i_parent->m_level+1),
		m_stop(false)
	{
	}

	/**

		Diable threading at this level: no gain and all pain!

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
			if( op_name == "instance" && m_node->hasParm("instancepath") )
			{
				m_result.push_back(
					new instance(m_context, m_node, m_time, i_primitive, index));
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
				vdb_file::node_is_vdb_loader( m_node, m_context.m_current_time );

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
				m_result.push_back( new vdb_file(
					m_context, m_node, m_time, i_primitive, index, vdb_path) );
				ret.push_back( m_result.back () );
				m_return.push_back( m_result.back() );
			}
			else
			{
				OBJ_Node_Refiner recursive(this);
				I->geometry()->refine(recursive, &recursive.m_params);
				ret = recursive.m_return;
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
		{
#if SYS_VERSION_FULL_INT >= 0x12000214
			const GT_PrimVolume* gt_volume =
				static_cast<const GT_PrimVolume*>(i_primitive.get());
			assert(gt_volume);
			if(gt_volume->isHeightField())
			{
				if(!m_params.getHeightFieldConvert())
				{
					/*
						m_result should be completely replaced by the recursive
						call below.
					*/
					m_result.clear();
					m_return.clear();

					/*
						Restart refinement from the top-level GT primitive
						again, but with height field conversion parameters.
						We don't want to use those unless we know there is a
						height field because it makes other volumes disappear.
					*/
					OBJ_Node_Refiner recursive(this);
					recursive.m_params.setCoalesceVolumes(true);
					recursive.m_params.setHeightFieldConvert(true);
					GT_PrimitiveHandle gt =
						GT_GEODetail::makeDetail(m_gu_detail);
					gt->refine(recursive, &recursive.m_params);

					/*
						The recursive call above processed all the GT primitives
						in the OBJ node. We can stop here.
					*/
					m_stop = true;
					break;
				}
			}
			else
#endif
			{
				/*
					m_result should be completely replaced by the recursive call
					below.
				*/
				m_result.clear();
				m_return.clear();

				/*
					Convert all Houdini volumes in this OBJ to VDB volumes.
					We couldn't do this unconditionally before refinement, in
					case there was a height field in the OBJ, which we want to
					convert into a polygon mesh, not a VDB.
				*/
				GU_DetailHandle vdb_handle;
				vdb_handle.allocateAndSet(new GU_Detail(true), true);
				GU_ConvertParms conversion_params;
				GU_PrimVDB::convertVolumesToVDBs(
					*vdb_handle.gdpNC(),
					*m_gu_detail.gdp(),
					conversion_params,
					true, // flood_sdf
					false, // prune
					0.0, // tolerance
					true, // keep_original
					true); // activate_inside

				// Restart refinement from the top-level GT primitive again.
				GT_PrimitiveHandle gt = GT_GEODetail::makeDetail(vdb_handle);
				OBJ_Node_Refiner recursive(this);
				gt->refine(recursive, &recursive.m_params);

				/*
					The recursive call above processed all the GT primitives
					in the OBJ node. We can stop here.
				*/
				m_stop = true;
				break;
			}

#ifdef VERBOSE
			fprintf(
				stderr, "3Delight for Houdini: unsupported volume "
					"workflow for %s\n", m_node->getName().c_str() );
#endif
			break;
		}
		case GT_PRIM_VDB_VOLUME:
		{
			// i_primitive is a GT_PrimVDB

			std::string vdb_path =
				vdb_file::node_is_vdb_loader( m_node, m_context.m_current_time );

			if( !vdb_path.empty() )
			{
				/*
					Houdini calls us once per grid but we want a single exporter
					for all of them. Try to find a previously built one.
				*/
				auto volume_primitive = std::find_if(
					m_result.begin(), m_result.end(),
					[] (const primitive *e) { return e->is_volume(); } );
				if( volume_primitive != m_result.end() )
				{
					/* volume already output */
					return;
				}

				m_result.push_back(
					new vdb_file( m_context, m_node, m_time, i_primitive, index, vdb_path) );
				m_return.push_back( m_result.back() );
			}
			else
			{
				// Try to add the grid to an existing VDB exporter
				auto vdb_it =
					std::find_if(
						m_result.rbegin(),
						m_result.rend(),
						[] (primitive* p)
							{ return dynamic_cast<vdb_file_writer*>(p); } );
				if(vdb_it != m_result.rend() &&
					dynamic_cast<vdb_file_writer*>(*vdb_it)->
						add_grid(i_primitive))
				{
					return;
				}

				// Choose where the VDB should be exported
				std::string vdb_path;
				if(m_context.m_export_path_prefix.empty())
				{
					/*
						Unless we're exporting an NSI file, we use a temporary
						file that will be deleted at the end of the render.
					*/
					vdb_path =
						UT_TempFileManager::getTempFilename().toStdString();
					m_context.m_temp_filenames.push_back(vdb_path);
				}
				else
				{
					// Use the same prefix as the NSI scene file
					uint32 obj_hash = m_node->getFullPath().hash();
					vdb_path =
						m_context.m_export_path_prefix + "." +
						std::to_string(obj_hash) + "v" +
						std::to_string(index) + ".vdb";
				}

				// Create a new VDB exporter for that grid
				m_result.push_back(
					new vdb_file_writer(
						m_context,
						m_node,
						m_time,
						i_primitive,
						index,
						vdb_path));
				m_return.push_back(m_result.back());
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
			OBJ_Node_Refiner recursive(this);
			i_primitive->refine(recursive, &recursive.m_params);
			if( recursive.m_return.empty() )
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
			m_return.insert(
				m_return.end(),
				recursive.m_return.begin(),
				recursive.m_return.end() );
		}
		}
	}
};

}

geometry::geometry(const context& i_context, OBJ_Node* i_object)
	:	exporter(i_context, i_object)
{
	SOP_Node *sop = m_object->getRenderSopPtr();

	assert( sop );

	for(
		time_sampler t(m_context, *m_object, time_sampler::e_deformation); t ; t++)
	{
		double time = *t;
		OP_Context context(time);
		GU_DetailHandle detail_handle( sop->getCookedGeoHandle(context) );

		if( !detail_handle.isValid() )
		{
#ifdef VERBOSE
			std::cerr
				<< "3Delight for Houdini: " << m_object->getFullPath()
				<< " has no valid detail" << std::endl;
#endif
			return;
		}

		/*
			This seems to be important in order to avoid overwriting the handles
			of previous time samples with the current one.
		*/
		detail_handle.addPreserveRequest();

		std::vector<primitive *> result;

		GT_PrimitiveHandle gt( GT_GEODetail::makeDetail(detail_handle) );

		OBJ_Node_Refiner refiner(
			m_object, detail_handle, m_context, time, result);
		gt->refine( refiner, &refiner.m_params );

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
	m_nsi.Create(hub_handle(), "transform");
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

bool geometry::is_texture(VOP_Node* shader)
{
	std::string mat_path = vop::shader_path(shader);
	const shader_library& library = shader_library::get_instance();

	DlShaderInfo* shader_info = library.get_shader_info(mat_path.c_str());
	std::vector< std::string > shader_tags;
	osl_utilities::get_shader_tags(*shader_info, shader_tags);

	//Check if the attached material is a texture shader or not.
	for (const auto& tag : shader_tags)
	{
		if (tag == "texture/2d" || tag == "texture/3d")
		{
			return true;
		}
	}
	return false;
}



void geometry::update_materials_mapping(
	VOP_Node*& i_shader,
	const context& i_context,
	OBJ_Node* i_object)
{
	if (i_context.m_ipr)
	{
		std::unordered_set<std::string> materials;
		std::vector<VOP_Node*> vops;
		materials.insert(i_shader->getFullPath().toStdString());
		scene::get_material_vops(materials, vops);

		/*
			Traverse through materials during IPR and for each material or texture
			we add the objects that they are connected to. This will later be used
			on vop.cpp where we will re-export only the objects which have the
			materials that we change the debug mode, connected to them.
		*/
		for (auto& V : vops)
		{
			i_context.material_to_objects[V].insert(i_object->getFullPath().c_str());
			if (V->getDebug())
			{
				i_shader = V;
			}
		}
	}
}

void geometry::connect_texture(
	VOP_Node* i_shader,
	OBJ_Node* i_node,
	const context& i_context,
	std::string attr_handle)
{
	const shader_library& library = shader_library::get_instance();
	std::string obj_handle = geometry(i_context, i_node).m_handle;
	std::string mat_handle = vop::handle(*i_shader, i_context);

	const std::string passthrough_shader(obj_handle + "_passthrough");
	std::string path = library.get_shader_path("passthrough");

	i_context.m_nsi.Create(passthrough_shader, "shader");
	i_context.m_nsi.SetAttribute(passthrough_shader, NSI::StringArg("shaderfilename", path));
	i_context.m_nsi.Connect(mat_handle, "outColor", passthrough_shader, "i_color",
		NSI::IntegerArg("strength", 1));
	i_context.m_nsi.Connect(
		passthrough_shader, "",
		attr_handle, "surfaceshader");
}

void geometry::connect()const
{
	// Connect the geometry's hub transform to the null's transform
	m_nsi.Connect(hub_handle(), "", m_handle, "objects");

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
		hub_handle());

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
		m_nsi.Connect(matte_handle, "", hub_handle(), "geometryattributes");
	}

	/*
		OBJ-level material assignment.

		\see polygonmesh for primitive attribute assignment on polygonal faces
	*/
	std::string material_path;
	VOP_Node* mat = get_assigned_material(material_path);
	if( !mat )
		return;

	m_nsi.Create( attributes_handle(), "attributes" );
	m_nsi.Connect(
		attributes_handle(), "",
		hub_handle(), "geometryattributes" );

	update_materials_mapping(mat, m_context, m_object);
	/*
		Connect a passthrough shader if the attached material on the geometry is
		not a surface shader. This would make it possible to render a connected
		texture to iDisplay. This is needed when you want to debug the scene.
	*/
	if (is_texture(mat))
	{
		connect_texture(mat, m_object, m_context, attributes_handle());
	}

	else
	{
		m_nsi.Connect(
			vop::handle(*mat, m_context), "",
			attributes_handle(), volume ? "volumeshader" : "surfaceshader",
			NSI::IntegerArg("strength", 1));
	}
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
		else if(m_context.m_ipr)
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
		else if(m_context.m_ipr)
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
		else if(m_context.m_ipr)
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
		else if(m_context.m_ipr)
		{
			m_nsi.DeleteAttribute(
				override_nsi_handle, "visibility.refraction" );
			m_nsi.DeleteAttribute(
				override_nsi_handle, "visibility.refraction.priority" );
		}
	}

	// Visible in Refractions override
	if( m_object->hasParm("_3dl_override_visibility_shadow_enable") )
	{
		bool over_vis_shadow_enable =
			m_object->evalInt("_3dl_override_visibility_shadow_enable", 0, time);

		if( over_vis_shadow_enable )
		{
			bool over_vis_shadow =
				m_object->evalInt("_3dl_override_visibility_shadow", 0, time);

			arguments.Add(
				new NSI::IntegerArg("visibility.shadow", over_vis_shadow));
			arguments.Add(
				new NSI::IntegerArg("visibility.shadow.priority", 10));
		}
		else if(m_context.m_ipr)
		{
			m_nsi.DeleteAttribute(
				override_nsi_handle, "visibility.shadow" );
			m_nsi.DeleteAttribute(
				override_nsi_handle, "visibility.shadow.priority" );
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
		else if(m_context.m_ipr)
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
		m_nsi.Connect( hub_handle(), "",
			override_nsi_handle, "bounds" );
		m_nsi.Connect( override_nsi_handle, "",
			NSI_SCENE_ROOT, "geometryattributes" );

		m_nsi.Connect(
			transparent_surface_handle(), "",
			hub_handle(), "geometryattributes" );

		bool override_surface_shader = m_object->evalInt(
			"_3dl_override_surface_shader", 0, time);

		if( override_surface_shader )
		{
			std::string material;
			VOP_Node* vop_node = get_assigned_material( material );
			if(vop_node)
			{
				geometry::update_materials_mapping(vop_node, m_context, m_object);

				if (is_texture(vop_node))
				{
					connect_texture(vop_node, m_object, m_context, override_nsi_handle);
				}
				m_nsi.Connect(
					vop::handle(*vop_node, m_context), "",
					override_nsi_handle, "surfaceshader",
					(
						NSI::IntegerArg("priority", 10),
						NSI::IntegerArg("strength", 1)
					) );
			}
		}
	}

	else if(m_context.m_ipr)
	{
		m_nsi.Disconnect( hub_handle(), "",
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
	m_object->evalString(
		material_path, "shop_materialpath", 0, m_context.m_current_time );

	if( material_path.length()==0 )
	{
		return nullptr;
	}

	VOP_Node* vop = resolve_material_path( material_path.c_str() );
	if(vop)
	{
		o_path = vop->getFullPath();
	}

	return vop;
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

/**
	\brief Callback that should be connected to an OBJ_Node that has an
	associated geometry exporter.
*/
void geometry::changed_cb(
	OP_Node* i_caller,
	void* i_callee,
	OP_EventType i_type,
	void* i_data)
{
	OBJ_Node* obj = i_caller->castToOBJNode();

	if( !obj )
	{
		assert( false );
		return;
	}

	context* ctx = (context*)i_callee;

	intptr_t parm_index = -1;

	switch(i_type)
	{
		case OP_CHILD_SWITCHED:
		{
			SOP_Node* render_sop = obj->getRenderSopPtr();
			OP_Node* child = (OP_Node*)i_data;
			if(render_sop != child)
			{
				return;
			}

			/*
				This could be a new render SOP, so we connect a callback to
				trap SOP_level updates to this OBJ node. A connection to a
				single node (the root of the network) seems to be sufficient to
				catch updates from all its dependencies, through the
				OP_INPUT_CHANGED event.
				FIXME : avoid creating duplicate interests for the same SOP.
			*/
			ctx->register_interest(render_sop, &geometry::sop_changed_cb);

			re_export(*ctx, *obj);
			break;
		}

		case OP_PARM_CHANGED:
		{
			parm_index = reinterpret_cast<intptr_t>(i_data);

			if(null::is_transform_parameter_index(parm_index))
			{
				// This will be handled by the associated null exporter
				return;
			}

			if(parm_index < 0)
			{
				return;
			}
			
			const PRM_Parm& parm = obj->getParm(parm_index);
			if(parm.getType().isSwitcher())
			{
				/*
					Avoid re-exporting the geometry when the user selects a
					different tab folder in the UI.
				*/
				return;
			}

			bool new_material =
				parm.getToken() == std::string("shop_materialpath");
			re_export(*ctx, *obj, new_material);
			break;
		}

		/*
			FIXME : We normally shouldn't react to OP_UI_MOVED because this type
			of event is supposed to be generated only when the node's location
			in the graph view changes, which has nothing to do with its actual
			geometry in the 3D scene. However, it seems to be the only event we
			can trap after the copy & paste of an object, once its render SOP
			gets a valid detail.
		*/
		case OP_UI_MOVED:

		case OP_FLAG_CHANGED:
		case OP_INPUT_CHANGED:
		case OP_SPAREPARM_MODIFIED:
		{
			re_export(*ctx, *obj);
			break;
		}

		case OP_NODE_PREDELETE:
		{
			Delete(*obj, *ctx);
			break;
		}
		
		default:
			return;
	}

	ctx->m_nsi.RenderControl(NSI::CStringPArg("action", "synchronize"));
}

/**
	\brief Callback that should be connected to the render SOP of an OBJ_Node
	that has an	associated geometry exporter.
	
	It allows us to trap changes to the whole SOP network contained in the OBJ
	node, which would go unnoticed if we had only connected
	geometry::changed_cb.
*/
void geometry::sop_changed_cb(
	OP_Node* i_caller,
	void* i_callee,
	OP_EventType i_type,
	void* i_data)
{
	if(i_type != OP_PARM_CHANGED && i_type != OP_FLAG_CHANGED &&
		i_type != OP_INPUT_CHANGED  && i_type != OP_SPAREPARM_MODIFIED)
	{
		return;
	}

	context* ctx = (context*)i_callee;

	OP_Network* parent = i_caller->getParent();
	assert(parent);
	OBJ_Node* obj = parent->castToOBJNode();
	assert(obj);

	re_export(*ctx, *obj);
	ctx->m_nsi.RenderControl(NSI::CStringPArg("action", "synchronize"));

}

void geometry::Delete(OBJ_Node& i_node, const context& i_context)
{
	i_context.m_nsi.Delete(
		hub_handle(i_node, i_context),
		NSI::IntegerArg("recursive", 1));
}

void geometry::re_export(
	const context& i_ctx,
	OBJ_Node& i_node,
	bool i_new_material)
{
	Delete(i_node, i_ctx);

	if(!i_node.getRenderSopPtr() || !i_ctx.object_displayed(i_node))
	{
		return;
	}

	geometry geo(i_ctx, &i_node);

	if(i_new_material)
	{
		/*
			Material assignment has changed, ensure that the new material exists
			in the NSI scene before connecting to it.
		*/
		std::unordered_set<std::string> materials;
		geo.get_all_material_paths(materials);
		scene::export_materials(materials, i_ctx);
	}

	geo.create();
	geo.connect();
	geo.set_attributes();
}
