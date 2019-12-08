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
	const GT_PrimPointMesh *pointmesh =
		static_cast<const GT_PrimPointMesh *>(default_gt_primitive().get());

	GT_AttributeListHandle attributes[4] =
	{
		pointmesh->getVertexAttributes(),
		pointmesh->getPointAttributes(),
		GT_AttributeListHandle(),
		pointmesh->getDetailAttributes(),
	};

	exporter::export_bind_attributes( attributes, nullptr /* not vertices */ );
	primitive::set_attributes();
}

void pointmesh::set_attributes_at_time(
	double i_time,
	const GT_PrimitiveHandle i_gt_primitive) const
{
	const GT_PrimPointMesh *pointmesh =
		static_cast<const GT_PrimPointMesh *>(i_gt_primitive.get());

	const char* width = "width";
	std::vector<const char *> to_export;
	to_export.push_back("P");
	to_export.push_back(width);
	to_export.push_back("id");

	/*
		Export per-particles attributes first. We make sure to specify the
		NSIParamPerVertex flag because, if the number of particles varies from
		one time step to the next, 3Delight might not be able to guess that
		those attributes are assigned per-vertex simply by looking at their
		count.
	*/
	std::vector<int> vertex_flags(to_export.size(), NSIParamPerVertex);
	export_attributes(
		&pointmesh->getPointAttributes(),
		1,
		i_time,
		to_export,
		&vertex_flags[0] );
	if(!to_export.empty())
	{
		// Export constant attributes if necessary
		export_attributes(
			&pointmesh->getDetailAttributes(),
			1,
			i_time,	
			to_export );
	}

	if(std::find(to_export.begin(), to_export.end(), width) != to_export.end())
	{
		// "width" not in attribute list. default to something.
		m_nsi.SetAttributeAtTime( m_handle, i_time, NSI::FloatArg(width, 0.1f) );
	}
}
