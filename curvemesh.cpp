#include "curvemesh.h"
#include "context.h"

#include <GA/GA_Names.h>
#include <GT/GT_PrimCurveMesh.h>
#include <OBJ/OBJ_Node.h>
#include <nsi.hpp>

#include <type_traits>
#include <iostream>

curvemesh::curvemesh(
	const context& i_ctx,
	OBJ_Node *i_object,
	double i_time,
	const GT_PrimitiveHandle &i_gt_primitive,
	unsigned i_primitive_index )
:
	primitive( i_ctx, i_object, i_time, i_gt_primitive, i_primitive_index )
{
}

void curvemesh::create( void ) const
{
	const GT_PrimCurveMesh *curve =
		static_cast<const GT_PrimCurveMesh *>(default_gt_primitive().get());

	if( !curve->isUniformOrder() )
	{
		std::cout <<
			"unable to render curves with non-uniform order." << std::endl;
		return;
	}

	m_nsi.Create( m_handle.c_str(), "curves" );
}

/**
	\brief Output "nvertices" for this mesh.

	Those are not time varying and can be output once.
*/
void curvemesh::set_attributes( void ) const
{
	const GT_PrimCurveMesh *curve =
		static_cast<const GT_PrimCurveMesh *>(default_gt_primitive().get());

	NSI::ArgumentList args;

	/*
		Prepare the 'nvertices' attribute which contains, for each curve,
		the total number of points.
	*/
	const GT_CountArray &count_array = curve->getCurveCountArray();
	std::unique_ptr<int> nvertices( new int[count_array.entries()] );
	for( GT_Size i=0; i<count_array.entries(); i++ )
	{
		nvertices.get()[i] = count_array.getCount(i);
	}

	m_nsi.SetAttribute( m_handle,
		*NSI::Argument( "nvertices" )
			.SetType(NSITypeInteger)
			->SetCount(count_array.entries())
			->SetValuePointer(nvertices.get())
		);

	const char* smooth_parm = "_3dl_smooth_curves";

	if( curve->getBasis() == GT_BASIS_BSPLINE )
	{
		m_nsi.SetAttribute( m_handle, NSI::CStringPArg("basis", "b-spline") );
	}
	else if( curve->getBasis() == GT_BASIS_CATMULLROM)
	{
		m_nsi.SetAttribute( m_handle, NSI::CStringPArg("basis", "catmull-rom") );
	}
	else if(m_object->hasParm(smooth_parm) &&
		m_object->evalInt(smooth_parm, 0, m_context.m_current_time) != 0)
	{
		m_nsi.SetAttribute(
			m_handle,
			(
				NSI::CStringPArg("basis", "catmull-rom"),
				NSI::IntegerArg("extrapolate", 1)
			) );
	}
	else
	{
		m_nsi.SetAttribute( m_handle, NSI::CStringPArg("basis", "linear") );
	}

	exporter::export_bind_attributes( *curve );

	primitive::set_attributes();
}

void curvemesh::set_attributes_at_time(
	double i_time,
	const GT_PrimitiveHandle i_gt_primitive) const
{
	const GT_Primitive *curve = i_gt_primitive.get();

	std::vector< std::string > to_export;
	to_export.push_back("P");
	to_export.push_back("width");

	export_attributes(to_export, *curve, i_time);

	if( std::find(to_export.begin(), to_export.end(), "width") !=
		to_export.end() )
	{
		/*
			"width" not in attribute list. default to something.
			We assume "P" is alwasy there. If not, everything is broken (tm)
			anyway.
		*/
		m_nsi.SetAttributeAtTime( m_handle, i_time,
			NSI::FloatArg("width", 0.001f) );
	}
}
