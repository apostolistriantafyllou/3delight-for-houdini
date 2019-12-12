#include "pointmesh.h"

#include "context.h"

#include <GT/GT_PrimPointMesh.h>
#include <OBJ/OBJ_Node.h>
#include <nsi.hpp>

pointmesh::pointmesh(
	const context& i_ctx,
	OBJ_Node *i_object,
	const GT_PrimitiveHandle &i_gt_primitive,
	unsigned i_primitive_index )
:
	primitive( i_ctx, i_object, i_gt_primitive, i_primitive_index )
{
}

void pointmesh::create( void ) const
{
	m_nsi.Create( m_handle.c_str(), "particles" );
}

void pointmesh::set_attributes( void ) const
{
	exporter::export_bind_attributes( *default_gt_primitive().get() );

	primitive::set_attributes();
}

void pointmesh::set_attributes_at_time(
	double i_time,
	const GT_PrimitiveHandle i_gt_primitive) const
{
	std::vector< std::string > to_export;
	to_export.push_back("P");
	to_export.push_back("width");
	to_export.push_back("id");

	export_attributes( *i_gt_primitive.get(), i_time, to_export );

	if( std::find(to_export.begin(), to_export.end(), "width")
		!= to_export.end() )
	{
		// "width" not in attribute list. default to something.
		m_nsi.SetAttributeAtTime(
			m_handle, i_time, NSI::FloatArg("width", 0.1f) );
	}
}
