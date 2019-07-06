#include "exporter.h"

#include <GT/GT_Handles.h>
#include <OBJ/OBJ_Node.h>

exporter::exporter(
	NSI::Context &i_nsi, OBJ_Node *i_object,
	const GT_PrimitiveHandle &i_gt_primitive )
:
	m_nsi(i_nsi),
	m_object(i_object),
	m_gt_primitive(i_gt_primitive)
{
	/*
		Geomtry uses its fille path + a prefix as a handle. So that
		it leaves the full path handle to the parent transform.
	*/
	m_handle = i_object->getFullPath();
	m_handle += "|geometry";
}

const std::string &exporter::handle( void ) const
{
	return m_handle;
}
