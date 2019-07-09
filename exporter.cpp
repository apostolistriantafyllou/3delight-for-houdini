#include "exporter.h"

#include "context.h"

#include <GT/GT_Handles.h>
#include <OBJ/OBJ_Node.h>

GT_PrimitiveHandle exporter::sm_invalid_gt_primitive;

exporter::exporter(
	const context& i_context, OBJ_Node *i_object,
	const GT_PrimitiveHandle &i_gt_primitive )
:
	m_context(i_context),
	m_nsi(i_context.m_nsi),
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

exporter::exporter( const context &i_context, VOP_Node *i_node )
:
	m_context(i_context),
	m_nsi(i_context.m_nsi),
	m_vop(i_node)
{
}

const std::string &exporter::handle( void ) const
{
	return m_handle;
}

/**
	\brief Connect to the parent object of this exporter.
*/
void exporter::connect( void ) const
{
	// FIXME: assert( this object is not a vop )
	m_nsi.Connect(
		m_handle.c_str(), "",
		m_object->getFullPath().buffer(), "objects" );
}
