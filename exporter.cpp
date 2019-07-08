#include "exporter.h"

#include "context.h"

#include <GT/GT_Handles.h>
#include <OBJ/OBJ_Node.h>

exporter::exporter(
	const context& i_ctx, OBJ_Node *i_object,
	const GT_PrimitiveHandle &i_gt_primitive )
:
	m_context(i_ctx),
	m_nsi(i_ctx.m_nsi),
	m_object(i_object),
	m_gt_primitive(i_gt_primitive)
{
	/*
		Geometry uses its full path + a prefix as a handle. So that
		it leaves the full path handle to the parent transform.
	*/
	m_handle = i_object->getFullPath();
	m_handle += "|geometry";
}

const std::string &exporter::handle( void ) const
{
	return m_handle;
}
