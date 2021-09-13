#include "alembic.h"
#include "context.h"

#include <GA/GA_Names.h>
#include <GT/GT_PackedAlembic.h>
#include <OBJ/OBJ_Node.h>
#include <nsi.hpp>
#include <UT/UT_Array.h>
#include <GT/GT_RefineParms.h>
#include <GU/GU_PrimPacked.h>

#include <type_traits>
#include <iostream>

namespace
{
	/// This seems important.
	void repair_alembic(GT_PackedAlembicArchive& io_alembic)
	{
		GT_RefineParms params;

		/**
			NOTE: this is a weird/secret call in the GT context. The refine()
			should have dealt with all this properly. This has been communicated
			to SideFx.

			FIXME: what to do with the first parameter ?
		*/
		io_alembic.bucketPrims( nullptr, &params, true );
	}

	/// Extracts the Alembic's archive file name.
	std::string get_archive_name(const GT_PackedAlembicArchive& i_alembic)
	{
		/*
			archiveName() returns a file name of the form "[<length>s<value>]",
			such as "[10s/a/b/c.abc]".
		*/

		std::string name = i_alembic.archiveName().toStdString();
		unsigned s = name.find('s');
		if(s >= name.length())
		{
			return {};
		}

		assert(name.back() == ']');
		return name.substr(s+1, name.length()-s-2);
	}
}

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

	// Retrieve a context that might redirect the attributes to a shared file
	NSI::Context& nsi = attributes_context();
	if(nsi.Handle() == NSI_BAD_CONTEXT)
	{
		return;
	}

	GT_PackedAlembicArchive *alembic =
		static_cast<GT_PackedAlembicArchive *>(default_gt_primitive().get());

	repair_alembic(*alembic);

	const UT_StringArray &names = alembic->getAlembicObjects();
    const GA_OffsetArray &offsets = alembic->getAlembicOffsets();

	std::vector< const char* > shapes;
	shapes.reserve(names.size());

	std::vector< double > transforms;
	transforms.reserve(16 * names.size());

	GU_DetailHandleAutoReadLock gdplock( alembic->parentDetail() );

	for( int i=0; i<names.size(); i++ )
	{
		shapes[i] = names[i].c_str();

		const GU_PrimPacked *prim = static_cast<const GU_PrimPacked *>
			(gdplock->getPrimitive(offsets[i]));

		UT_Matrix4D trs; prim->getFullTransform4( trs );
		memcpy( &transforms[i*16], trs.data(), sizeof(double)*16 ) ;
	}

	std::string file_name = get_archive_name(*alembic);
	if(file_name.empty())
	{
		return;
	}

	const char *k_subdiv = "_3dl_render_poly_as_subd";
	bool poly_as_subd =
		m_object->hasParm(k_subdiv) &&
		m_object->evalInt(k_subdiv, 0, m_context.m_current_time) != 0;

	nsi.Evaluate(
		(
		*NSI::Argument::New("transforms")
			->SetType(NSITypeDoubleMatrix)
			->SetCount(names.size())
			->SetValuePointer( &transforms[0]),
		*NSI::Argument::New("shapes")
			->SetType(NSITypeString)
			->SetCount(names.size())
			->SetValuePointer(&shapes[0]),
		NSI::StringArg( "type", "dynamiclibrary"),
		NSI::StringArg( "filename", "alembic" ),
		NSI::StringArg(	"parent_node", m_handle ),
		NSI::StringArg( "abc_file", file_name ),
		NSI::IntegerArg( "poly_as_subd", poly_as_subd ),
		NSI::IntegerArg( "do_mblur", m_context.MotionBlur() ),
		NSI::FloatArg( "fps", m_context.m_fps),
		NSI::DoubleArg( "frame", m_context.m_current_time*m_context.m_fps)) );

	::free( (void *) to_free );
}

void alembic::set_attributes_at_time(
	double i_time,
	const GT_PrimitiveHandle i_gt_primitive) const
{
}
