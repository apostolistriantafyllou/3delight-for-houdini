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

	std::string file_name = get_archive_name(*alembic);
	if(file_name.empty())
	{
		return;
	}

	const UT_StringArray &names = alembic->getAlembicObjects();

	/*
		Call the base-class version of set_attributes so it loops over our
		set_attributes_at_time, accumulating transforms for each time sample.
		This is needed because the Alembic procedural is called through
		NSIEvaluate instead of an NSI "procedural" node, which means it can't be
		exported in multiple independent calls.
	*/
	assert(m_transforms.empty());
	assert(m_transform_times.empty());
	primitive::set_attributes();

	std::vector< const char* > shapes;
	for( int i=0; i<names.size(); i++ )
	{
		shapes.push_back(names[i].c_str());
	}

	/*
		Transpose the list of transforms so they're grouped by shape instead of
		time.
	*/
	assert(m_transforms.size() == names.size() * m_transform_times.size());
	std::vector<double> transforms;
	transforms.resize(m_transforms.size()*16);
	for(unsigned sx = 0; sx < m_transforms.size(); sx++)
	{
		unsigned p = sx % names.size();
		unsigned t = sx / names.size();
		unsigned tx = p * m_transform_times.size() + t;
		unsigned td = tx * 16;
		memcpy(&transforms[td], m_transforms[sx].data(), sizeof(double)*16);
	}
	m_transforms.clear();

	const char *k_subdiv = "_3dl_render_poly_as_subd";
	bool poly_as_subd =
		m_object->hasParm(k_subdiv) &&
		m_object->evalInt(k_subdiv, 0, m_context.m_current_time) != 0;

	nsi.Evaluate(
		(
		*NSI::Argument::New("overrides.transforms")
			->SetArrayType(NSITypeDoubleMatrix, m_transform_times.size())
			->SetCount(names.size())
			->SetValuePointer(&transforms[0]),
		*NSI::Argument::New("overrides.transforms.times")
			->SetArrayType(NSITypeDouble, m_transform_times.size())
			->SetValuePointer(&m_transform_times[0]),
		*NSI::Argument::New("overrides.shapes")
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

	m_transform_times.clear();
}

void alembic::set_attributes_at_time(
	double i_time,
	const GT_PrimitiveHandle i_gt_primitive) const
{
	/*
		Since the Alembic procedural is called through NSIEvaluate instead of
		an NSI "procedural" node, this function simply accumulates data for each
		time sample, which will then be exported all at once in
		set_attributes().
	*/

	// Accumulate times
	m_transform_times.push_back(i_time);

	GT_PackedAlembicArchive* alembic =
		static_cast<GT_PackedAlembicArchive*>(i_gt_primitive.get());
	repair_alembic(*alembic);
	const GA_OffsetArray& offsets = alembic->getAlembicOffsets();

	GU_DetailHandleAutoReadLock gdplock(alembic->parentDetail());

	// Accumulate transforms (for each shape)
	for(auto offset : offsets)
	{
		const GU_PrimPacked* prim =
			static_cast<const GU_PrimPacked*>(gdplock->getPrimitive(offset));

		m_transforms.push_back(UT_Matrix4D());
		prim->getFullTransform4(m_transforms.back());
	}
}
