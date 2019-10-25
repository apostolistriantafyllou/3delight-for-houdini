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
	const GT_PrimitiveHandle &i_gt_primitive,
	unsigned i_primitive_index )
:
	primitive( i_ctx, i_object, i_gt_primitive, i_primitive_index )
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

	if( curve->getBasis() == GT_BASIS_BSPLINE ||
		curve->getBasis() == GT_BASIS_CATMULLROM )
	{
		m_nsi.Create( m_handle.c_str(), "cubiccurves" );
	}
	else
	{
		/* render everything else as linear curves, including  */
		m_nsi.Create( m_handle.c_str(), "linearcurves" );
	}
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

	if( curve->getBasis() == GT_BASIS_BSPLINE )
	{
		m_nsi.SetAttribute( m_handle, NSI::CStringPArg("basis", "b-spline") );
	}

	if( curve->getBasis() == GT_BASIS_CATMULLROM )
	{
		m_nsi.SetAttribute( m_handle, NSI::CStringPArg("basis", "catmull-rom") );
	}

	primitive::set_attributes();
}

void curvemesh::set_attributes_at_time(
	double i_time,
	const GT_PrimitiveHandle i_gt_primitive) const
{
	const GT_PrimCurveMesh *curve =
		static_cast<const GT_PrimCurveMesh *>(i_gt_primitive.get());

	GT_AttributeListHandle attributes[] =
	{
		curve->getVertexAttributes(),
		curve->getUniformAttributes(),
		curve->getDetailAttributes()
	};


	std::vector<const char *> to_export;
	to_export.push_back("P");
	to_export.push_back("width");

	export_attributes(
		&attributes[0], sizeof(attributes)/sizeof(attributes[0]), i_time,
		to_export);

	if( to_export.size() == 1 )
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
