#include "vdb.h"

#include "context.h"
#include "VOP_ExternalOSL.h"
#include "dl_system.h"

#include <nsi_dynamic.hpp>
#include <nsi.hpp>
#include "VDBQuery.h" /* From 3delight */

#include <openvdb/openvdb.h>
#include <GT/GT_Handles.h>
#include <GT/GT_PrimVDB.h>
#include <OBJ/OBJ_Node.h>
#include <SOP/SOP_Node.h>

#include <assert.h>
#include <iostream>

namespace
{
	// Follows input an connection on a SOP and updates the SOP type
	bool traverse_input(int i_index, SOP_Node*& io_sop, std::string& io_type)
	{
		if(io_sop->nInputs() <= i_index)
		{
			return false;
		}

		OP_Node* in = io_sop->getInput(i_index);
		io_sop = in->castToSOPNode();
		if(!io_sop)
		{
			return false;
		}

		io_type = io_sop->getOperator()->getName().toStdString();

		return true;
	}
}


vdb_file::vdb_file(
	const context& i_ctx, OBJ_Node *i_obj,
	double i_time,
	const GT_PrimitiveHandle &i_gt_primitive,
	unsigned i_primitive_index,
	const std::string &i_vdb_file )
:
	primitive( i_ctx, i_obj, i_time, i_gt_primitive, i_primitive_index ),
	m_vdb_file(i_vdb_file)
{
}

void vdb_file::create( void ) const
{
	m_nsi.Create( m_handle, "transform" );
	std::string volume = m_handle + "|volume";
	m_nsi.Create( volume, "volume" );

	m_nsi.Connect( volume, "", m_handle, "objects" );
}

void vdb_file::get_attributes_from_material(
	NSI::ArgumentList& io_arguments,
	const std::string& i_vdb_file,
	VOP_Node* i_volume,
	double i_time)
{
	std::vector<std::string> grid_names;
	if( !get_grid_names( i_vdb_file, grid_names))
	{
		return;
	}

	/*
		Retrieve the required grid names from the volume shader (see
		GetVolumeParams() in VOP_ExternalOSL.cpp), along with the velocity
		scale.
	*/

	UT_String density_grid;
	UT_String color_grid;
	UT_String temperature_grid;
	UT_String emissionintensity_grid;
	UT_String emission_grid;
	UT_String velocity_grid;
	double velocity_scale = 1.0;

	if(i_volume)
	{
		using namespace VolumeGridParameters;

		if(i_volume->hasParm(density_name))
		{
			i_volume->evalString(density_grid, density_name, 0, i_time);
		}
		if(i_volume->hasParm(color_name))
		{
			i_volume->evalString(color_grid, color_name, 0, i_time);
		}
		if(i_volume->hasParm(temperature_name))
		{
			i_volume->evalString(temperature_grid, temperature_name, 0, i_time);
		}
		if(i_volume->hasParm(emission_intensity_name))
		{
			i_volume->evalString(
				emissionintensity_grid, emission_intensity_name, 0, i_time);
		}
		if(i_volume->hasParm(emission_name))
		{
			i_volume->evalString(emission_grid, emission_name, 0, i_time);
		}
		if(i_volume->hasParm(velocity_name))
		{
			i_volume->evalString(velocity_grid, velocity_name, 0, i_time);
		}

		if(i_volume->hasParm(velocity_scale_name))
		{
			velocity_scale = i_volume->evalFloat(velocity_scale_name, 0, i_time);
		}
	}

	// Export required grid names if they're available
	for( const std::string& grid : grid_names )
	{
		if( grid == density_grid.toStdString() )
		{
			io_arguments.Add( new NSI::StringArg("densitygrid", grid) );
		}

		if( grid == color_grid.toStdString() )
		{
			io_arguments.Add( new NSI::StringArg( "colorgrid", grid) );
		}

		if( grid == temperature_grid.toStdString() )
		{
			io_arguments.Add( new NSI::StringArg( "temperaturegrid", grid) );
		}

		if( grid == emissionintensity_grid.toStdString() )
		{
			io_arguments.Add( new NSI::StringArg( "emissionintensitygrid", grid) );
		}

		if( grid == emission_grid.toStdString() )
		{
			io_arguments.Add( new NSI::StringArg( "emissiongrid", grid) );
		}

		if( grid == velocity_grid.toStdString() )
		{
			io_arguments.Add( new NSI::StringArg( "velocitygrid", grid) );
		}
	}

	io_arguments.Add( new NSI::DoubleArg( "velocityscale", velocity_scale ) );
}

void vdb_file::set_attributes( void ) const
{
	NSI::ArgumentList arguments;
	arguments.Add( new NSI::CStringPArg( "vdbfilename", m_vdb_file.c_str()) );

	double time = m_context.m_current_time;

	OP_Node* op = m_object->getMaterialNode(time);

	/*
		This might seem useless but it's not, as resolve_material_path might
		return a child of "i_material" instead of "i_material" itself, if it's a
		Material Builder.
	*/
	VOP_Node *mats[3] = { nullptr };
	if( op )
	{
		resolve_material_path( m_object, op->getFullPath().c_str(), mats );
	}

	get_attributes_from_material(arguments, m_vdb_file, mats[2], time);

	m_nsi.SetAttribute( m_handle + "|volume", arguments );

	primitive::set_attributes();
}

bool vdb_file::is_volume()const
{
	return true;
}

/**
	There is no motion blur parameters to set here on the volumes itself as the
	volume contains a velocity field that is supported by 3Delight. BUT,
	the actual volume container might be moving, so we need to set the
	right matrix.
*/
void vdb_file::set_attributes_at_time(
	double i_time,
	const GT_PrimitiveHandle i_gt_primitive) const
{
	/*
		FIXME: hopefully this will always be identity as the instancer takes
		care of the transform. If not we are in trouble.
	*/

#if 0
	const GT_TransformHandle &handle = i_gt_primitive->getPrimitiveTransform();
	UT_Matrix4D local;
	handle->getMatrix( local );

	/*
		FIXME : this is redundant with the attribute already set by
		primitive::connect. However, since our way of supporting VDBs involves
		them being read as instances, that part is skipped (for now). We don't
		want the transform to be applied twice!
	*/
	/* The stars are aligned for Houdini and NSI */
	m_nsi.SetAttributeAtTime(
		m_handle,
		i_time,
		NSI::DoubleMatrixArg( "transformationmatrix", local.data() ) );
#endif
}

bool vdb_file::get_grid_names(
	const std::string& i_vdb_path,
	std::vector<std::string>& o_names)
{
	o_names.clear();

	NSI::DynamicAPI api;
#ifdef __APPLE__
	/*
		FIXME : do we also need this fix in ROP_3Delight, context,
		shader_library and viewport_hook? Or is it obsolete?

		Houdini will nuke [DY]LD_LIBRARY_PATH on macOS. This is a bit insane
		but reality is more insane so that's fine. We will try to get the
		library path manually, something we usually try to avoid for robustness
		reasons.
	*/
	const char* macos_delight = ::getenv("DELIGHT");
	if (macos_delight)
	{
		std::string path_to_lib(macos_delight);
		path_to_lib += "/lib/lib3delight.dylib";
		api = NSI::DynamicAPI(path_to_lib.c_str());
	}
#endif

	/*
	*/
	decltype(&DlVDBGetGridNames) vdb_grids = nullptr;
	api.LoadFunction(vdb_grids, "DlVDBGetGridNames");
	decltype(&DlVDBFreeGridNames) free_grids = nullptr;
	api.LoadFunction(free_grids, "DlVDBFreeGridNames");

	if (!vdb_grids || !free_grids)
	{
		::fprintf(stderr,
			"3Delight for Houdini: unable to load VDB utility "
			"function from 3Delight dynamic library");
		return false;
	}

	int num_grids = 0;
	const char* const* grid_names = nullptr;
	if (!vdb_grids(i_vdb_path.c_str(), &num_grids, &grid_names) ||
		num_grids == 0 ||
		!grid_names)
	{
		::fprintf(stderr,
			"3Delight for Houdini: no usable grid in VDB %s",
			i_vdb_path.c_str());
		return false;
	}

	for(int g = 0; g < num_grids; g++)
	{
		o_names.emplace_back(grid_names[g]);
	}

	free_grids(grid_names);
	
	return true;
}


vdb_file_loader::vdb_file_loader(
	const context& i_ctx,
	OBJ_Node* i_obj,
	double i_time,
	const GT_PrimitiveHandle &i_handle,
	unsigned i_primitive_index)
	:	vdb_file(
			i_ctx,
			i_obj,
			i_time,
			i_handle,
			i_primitive_index,
			get_path(i_obj, i_time))
{
}

std::string vdb_file_loader::get_path(OBJ_Node* i_node, double i_time)
{
	SOP_Node* transform_sop = nullptr;
	std::string vdb_path;
	get_geo(i_node, i_time, transform_sop, vdb_path);
	return vdb_path;
}

void vdb_file_loader::set_attributes_at_time(
	double i_time,
	const GT_PrimitiveHandle i_gt_primitive)const
{
	vdb_file::set_attributes_at_time(i_time, i_gt_primitive);

	SOP_Node* transform_sop = nullptr;
	std::string vdb_path;
	get_geo(m_object, i_time, transform_sop, vdb_path);

	if(!transform_sop)
	{
		return;
	}

	/*
		Compute the transform matrix applied by this SOP node.
		There doesn't seem to be a direct way to do it. The other simple way
		this could be done would be to rely on the "addattrib" and
		"outputattrib" parameters in order to write the resulting matrix to an
		attribute accessible from the the node's "detail", but it's less
		reliable because these parameters are accessible to the user.
		Also, it ignores the "pre-transform" part of the transform node.
	*/

	/*
		We need to retrieve the string, and not the menu item position of
		"xOrd", since the values in the menu are in the *reverse order* compared
		to the values of enum UT_XformOrder::rstOrder.
		Parameter "rOrd" doesn't have this problem, but processing it the same
		way seems safer, since it decouples the UI from the implementation.
	*/
	UT_String xOrd;
	transform_sop->evalString(xOrd, "xOrd", 0, i_time);
	UT_String rOrd;
	transform_sop->evalString(rOrd, "rOrd", 0, i_time);
	UT_XformOrder order(xOrd.c_str(), rOrd.c_str());

	double t[3];
	transform_sop->evalFloats("t", t, i_time);
	double r[3];
	transform_sop->evalFloats("r", r, i_time);
	double s[3];
	transform_sop->evalFloats("s", s, i_time);
	double shear[3];
	transform_sop->evalFloats("shear", shear, i_time);
	double scale = transform_sop->evalFloat("scale", 0, i_time);

	double p[3];
	transform_sop->evalFloats("p", p, i_time);
	double pr[3];
	transform_sop->evalFloats("pr", pr, i_time);
	/*
		This should be
		UT_Matrix4D::PivotSpace pivot(UT_Vector3D(p), UT_Vector3D(pr));
		but gcc doesn't like it!
	*/
	UT_Matrix4D::PivotSpace pivot =
		UT_Matrix4D::PivotSpace(UT_Vector3D(p), UT_Vector3D(pr));

	int invertxform = transform_sop->evalInt("invertxform", 0, i_time);

	/*
		NB : shear and invertxform don't mix well : at least, the "xform" call
		below doesn't result in the same matrix as the SOP's "output attribute"
		feature when both parameters are used. This is probably not a big deal
		as neither is likely to be used in that context.
	*/
	UT_Matrix4D transform(1.0);
	transform.xform(
		order,
		t[0], t[1], t[2],
		r[0], r[1], r[2],
		s[0]*scale, s[1]*scale, s[2]*scale,
		shear[0], shear[1], shear[2],
		pivot,
		invertxform);

	// Also retrieve the "pre-transform"

	transform_sop->evalString(xOrd, "prexform_xOrd", 0, i_time);
	transform_sop->evalString(rOrd, "prexform_rOrd", 0, i_time);
	UT_XformOrder prexform_order(xOrd.c_str(), rOrd.c_str());

	transform_sop->evalFloats("prexform_t", t, i_time);
	transform_sop->evalFloats("prexform_r", r, i_time);
	transform_sop->evalFloats("prexform_s", s, i_time);
	transform_sop->evalFloats("prexform_shear", shear, i_time);


	/*
		Apply the "pre-transform". It's actually applied afterwards, further
		from the VDB node, so the "pre-" prefix seems a bit strange.
	*/
	transform.xform(
		prexform_order,
		t[0], t[1], t[2],
		r[0], r[1], r[2],
		s[0], s[1], s[2],
		shear[0], shear[1], shear[2]);
	m_nsi.SetAttributeAtTime(
		m_handle,
		i_time,
		NSI::DoubleMatrixArg("transformationmatrix", transform.data()));
}

void vdb_file_loader::get_geo(
	OBJ_Node* i_obj,
	double i_time,
	SOP_Node*& o_transform_sop,
	std::string& o_vdb_path)
{
	o_transform_sop = nullptr;

	SOP_Node* sop = i_obj->getRenderSopPtr();
	if(!sop)
	{
		return;
	}

	std::string sop_type = sop->getOperator()->getName().toStdString();

	// Ignore a possible timeshift node
	bool timeshift = false;
	if(sop_type == "timeshift")
	{
		timeshift = true;
		if(!traverse_input(0, sop, sop_type))
		{
			return;
		}
	}

	// Retrieve the Transform SOP if there is one
	if(sop_type == "xform")
	{
		o_transform_sop = sop;
		if(!traverse_input(0, sop, sop_type))
		{
			return;
		}
	}

	// Ignore a possible timeshift node
	if(sop_type == "timeshift")
	{
		timeshift = true;
		if(!traverse_input(0, sop, sop_type))
		{
			return;
		}
	}

	// Check that the SOP is a file loader
	if(sop_type != "file")
	{
		return;
	}

	// Retrieve the file name
	UT_String file;
	if(timeshift)
	{
		/*
			When a "timeshift" SOP is present AND the file SOP is set to load
			Packed Disk Primitives, we should find a "path" primitive attribute
			that contains the time-shifted file path.
		*/
		OP_Context context(i_time);
		GU_DetailHandle detail_handle(
			i_obj->getRenderSopPtr()->getCookedGeoHandle(context));

		if(!detail_handle.isValid())
		{
			return;
		}

		GA_RWHandleS h(detail_handle.gdp(), GA_ATTRIB_PRIMITIVE, "path");
		if(!h.isValid())
		{
			return;
		}

		file = h.get(0);
	}
	else
	{
		if(!sop->hasParm("file"))
		{
			return;
		}
		sop->evalString(file, "file", 0, i_time);
	}

	if(!file.fileExtension() || ::strcmp(file.fileExtension(), ".vdb") != 0)
	{
		return;
	}

	o_vdb_path = file.toStdString();
}

bool vdb_file::has_vdb_light_layer(OBJ_Node* i_node, double i_time)
{
	std::string vdb_path =
		vdb_file_loader::get_path(i_node, i_time);
	if (vdb_path.empty() || !dl_system::file_exists(vdb_path.c_str()))
	{
		return false;
	}

	std::vector<std::string> grid_names;
	if (!get_grid_names(vdb_path, grid_names))
	{
		return false;
	}

	OP_Node* op = i_node->getMaterialNode(i_time);

	VOP_Node* mats[3] = { nullptr };
	if (op)
	{
		resolve_material_path(i_node, op->getFullPath().c_str(), mats);
	}

	VOP_Node* material = mats[2]; // volume
	UT_String emission_grid;
	UT_String temperature_grid;
	UT_String heat_grid;

	if (material)
	{
		using namespace VolumeGridParameters;

		if (material->hasParm(emission_name))
		{
			material->evalString(emission_grid, emission_name, 0, i_time);
		}

		if (material->hasParm(emission_intensity_name))
		{
			material->evalString(heat_grid, emission_intensity_name, 0, i_time);
		}

		if (material->hasParm(temperature_name))
		{
			material->evalString(temperature_grid, temperature_name, 0, i_time);
		}
	}

	/*
		Check if the emission, temperature or heat grid used is one of the
		existing grids of the VDB in order to include it as a light AOV.
	*/
	for (const std::string& grid : grid_names)
	{
		if (grid == emission_grid.toStdString()
			|| grid == temperature_grid.toStdString()
			|| grid == heat_grid.toStdString())
		{
			return true;
		}
	}
	return false;
}

vdb_file_writer::vdb_file_writer(
	const context& i_ctx,
	OBJ_Node* i_obj,
	double i_time,
	const GT_PrimitiveHandle& i_handle,
	unsigned i_primitive_index,
	const std::string& i_vdb_path)
	:	vdb_file(i_ctx, i_obj, i_time, i_handle, i_primitive_index, i_vdb_path)
{
	assert(dynamic_cast<GT_PrimVDB*>(i_handle.get()));
	m_grids.push_back(i_handle);
}

bool vdb_file_writer::add_grid(const GT_PrimitiveHandle& i_handle)
{
	GT_PrimVDB* gt_vdb = dynamic_cast<GT_PrimVDB*>(i_handle.get());
	assert(gt_vdb);

	assert(!m_grids.empty());

	// Reject grids that don't have the same transform
	UT_Matrix4D matrix;
	m_grids.front()->getPrimitiveTransform()->getMatrix(matrix);
	UT_Matrix4D new_matrix;
	i_handle->getPrimitiveTransform()->getMatrix(new_matrix);
	if(matrix != new_matrix)
	{
		return false;
	}
	
	/*
		Reject grids that would introduce a name duplicate (should we merge them
		instead?)
	*/
	std::string grid_name = gt_vdb->getGrid()->getName();
	for(const GT_PrimitiveHandle& h : m_grids)
	{
		GT_PrimVDB* v = dynamic_cast<GT_PrimVDB*>(h.get());
		assert(v);
		if(v->getGrid()->getName() == grid_name)
		{
			return false;
		}
	}
	
	m_grids.push_back(i_handle);
	return true;
}

void vdb_file_writer::create()const
{
	if(!m_grids.empty())
	{
		// Retrieve the OpenVDB grids
		openvdb::GridCPtrVec grids;
		for(const GT_PrimitiveHandle& handle : m_grids)
		{
			GT_PrimVDB* gt_vdb = dynamic_cast<GT_PrimVDB*>(handle.get());
			assert(gt_vdb);

			/*
				Build a shared_ptr with an empty deleter so it doesn't try to
				delete Houdini's grid when the grids vector goes out of scope.
			*/
			std::shared_ptr<const openvdb::GridBase> grid_ptr(
					gt_vdb->getGrid(),
					[](const openvdb::GridBase*){});

			grids.push_back(grid_ptr);
		}

		// Write the VDB to file
		openvdb::io::File file(path());
		file.write(grids);

		// Don't export that file again
		m_grids.clear();
	}
	
	vdb_file::create();
}
