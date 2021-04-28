#include "alembic.h"
#include "context.h"

#include <GA/GA_Names.h>
#include <GT/GT_PackedAlembic.h>
#include <OBJ/OBJ_Node.h>
#include <nsi.hpp>

#include <type_traits>
#include <iostream>

alembic::alembic(
	const context& i_ctx,
	OBJ_Node *i_object,
	double i_time,
	const GT_PrimitiveHandle &i_gt_primitive,
	unsigned i_primitive_index )
	:	primitive(
			i_ctx,
			i_object,
			i_time,
			i_gt_primitive,
			i_primitive_index )
{
}

void alembic::create( void ) const
{
	/* This will be the anchor for our procedural. */
	m_nsi.Create( m_handle, "transform" );
}

/**
	\brief Output "nvertices" for this mesh.

	Those are not time varying and can be output once.
*/
void alembic::set_attributes( void ) const
{
    //const GT_TransformHandle &trs = alembic->getPrimitiveTransform();

	const GT_PackedAlembicArchive *alembic =
		static_cast<const GT_PackedAlembicArchive *>(default_gt_primitive().get());

	NSI::ArgumentList args;
	char *file_name = ::strdup( alembic->archiveName().c_str() );
	char *to_free = file_name;
	file_name = strchr( file_name, 's' );
	if( !file_name )
		return;

	file_name ++;
	file_name[ ::strlen(file_name)-1 ] = 0;

	const char *k_subdiv = "_3dl_render_poly_as_subd";
	bool poly_as_subd =
		m_object->hasParm(k_subdiv) &&
		m_object->evalInt(k_subdiv, 0, m_context.m_current_time) != 0;

	m_nsi.Evaluate(
		(
		NSI::StringArg( "type", "dynamiclibrary"),
		NSI::StringArg( "filename", "alembic" ),
		NSI::StringArg(	"parent_node", m_handle ),
		NSI::StringArg( "abc_file", file_name ),
		NSI::IntegerArg( "poly_as_subd", poly_as_subd ),
		NSI::IntegerArg( "do_mblur", m_context.MotionBlur() ),
		NSI::DoubleArg( "fps", m_context.m_fps),
		NSI::DoubleArg( "frame", m_context.m_current_time*m_context.m_fps)) );

	::free( (void *) to_free );
}

void alembic::set_attributes_at_time(
	double i_time,
	const GT_PrimitiveHandle i_gt_primitive) const
{
}
