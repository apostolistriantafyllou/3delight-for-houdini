
#include "mesh.h"

#include <OBJ/OBJ_Node.h>
#include <SOP/SOP_Node.h>
#include <GU/GU_Detail.h>
#include <OP/OP_Context.h>

#include <nsi.hpp>

void mesh::export_object( OBJ_Node *i_object )
{
	SOP_Node *sop = i_object->getRenderSopPtr();
	if( !sop )
		return;

	OP_Context context( 0 );

	const GU_DetailHandleAutoReadLock detail_handle( sop->getCookedGeoHandle(context) );
	if( !detail_handle.isValid() )
	{
		std::cout << "invalid detail for " << i_object->getFullPath() << std::endl;
		//assert( !"invalid detail handle" );
		return;
	}

	const GU_Detail &detail = *detail_handle;

	std::cout << i_object->getName() << " --> " << detail.getNumPoints() << " / " <<
		detail.getNumPrimitives() << std::endl;
}
