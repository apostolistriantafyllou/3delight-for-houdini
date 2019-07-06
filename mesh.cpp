
#include "mesh.h"
#include "context.h"

#include <OBJ/OBJ_Node.h>
#include <SOP/SOP_Node.h>
#include <GU/GU_Detail.h>
#include <OP/OP_Context.h>

#include <nsi.hpp>

void mesh::export_object( const context &i_context, OBJ_Node *i_object )
{
	SOP_Node *sop = i_object->getRenderSopPtr();
	if( !sop )
		return;

	OP_Context context( i_context.m_start_time );

	const GU_ConstDetailHandle detail_handle(
		sop->getCookedGeoHandle(context) );

	if( !detail_handle.isValid() )
	{
		std::cout << "invalid detail for " << i_object->getFullPath() << std::endl;
		//assert( !"invalid detail handle" );
		return;
	}

	const GU_Detail &detail = *detail_handle.gdp();

	std::cout << i_object->getName() << " --> " << detail.getNumPoints() << " / " <<
		detail.getNumPrimitives() << std::endl;
}
