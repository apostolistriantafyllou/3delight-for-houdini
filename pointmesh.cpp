#include "pointmesh.h"

#include "context.h"

#include <GT/GT_PrimPointMesh.h>
#include <OBJ/OBJ_Node.h>
#include <nsi.hpp>

pointmesh::pointmesh(
	const context& i_ctx,
	OBJ_Node *i_object,
	const GT_PrimitiveHandle &i_gt_primitive )
:
	primitive( i_ctx, i_object, i_gt_primitive )
{
}

void pointmesh::create( void ) const
{
	m_nsi.Create( m_handle.c_str(), "particles" );
}

void pointmesh::set_attributes( void ) const
{
	set_attributes_at_time(m_context.m_current_time);
}

void pointmesh::set_attributes_at_time( double i_time ) const
{
	const GT_PrimPointMesh *pointmesh =
		static_cast<const GT_PrimPointMesh *>(m_gt_primitive.get());

	/*
		NSI doesn't care if it's "vertex" or "uniform". It guess that
		from how many data its given. So just pass through the attributes
		as one list.
	*/
	const GT_AttributeListHandle all_attributes[2] =
	{
		pointmesh->getPointAttributes(),
		pointmesh->getDetailAttributes()
	};

	std::vector<const char *> to_export;
	to_export.push_back("P");
	to_export.push_back("width");

	export_attributes( &all_attributes[0], 2, i_time, to_export );

	if( to_export.size() == 1 )
	{
		/*
			"width" not in attribute list. default to something.
			We assume "P" is alwasy there. If not, everything is broken (tm)
			anyway.
		*/
		m_nsi.SetAttributeAtTime( m_handle, i_time,
			NSI::FloatArg("width", 0.1f) );
	}
}
