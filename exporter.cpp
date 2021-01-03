#include "exporter.h"

#include "context.h"
#include "vop.h"
#include "VOP_3DelightMaterialBuilder.h"

#include <nsi.hpp>

#include <OBJ/OBJ_Node.h>
#include <VOP/VOP_Node.h>

#include  <GT/GT_PrimPolygonMesh.h>

exporter::exporter(
	const context& i_context, OBJ_Node *i_object )
:
	m_context(i_context),
	m_nsi(i_context.m_nsi),
	m_object(i_object)
{
	assert(m_object);
	m_handle = handle(*m_object, i_context);
}

exporter::~exporter()
{
}


exporter::exporter( const context &i_context, VOP_Node *i_node )
:
	m_context(i_context),
	m_nsi(i_context.m_nsi),
	m_vop(i_node)
{
	assert(m_vop);
	m_handle = handle(*m_vop, i_context);
}

const std::string &exporter::handle( void ) const
{
	return m_handle;
}

std::string exporter::handle(const OP_Node& i_node, const context& i_ctx)
{
	return
		i_ctx.m_ipr
		?	std::to_string(i_node.getUniqueId())
		:	i_node.getFullPath().toStdString();
}

/**
	Utility to get an NSI type from a GT_Type.

	\returns NSITypeInvalid if we don't know what to do with
	the type

	NOTE: we don't try to resolve the "tuple size" here. For example,
	a float with a tuple size of 3 might be better of as a "point" in NSI.
	We let the caller take care of this.
*/
static NSIType_t gt_to_nsi_type( GT_Type i_type, GT_Storage i_storage )
{
	switch( i_type)
	{
	case GT_TYPE_POINT: return  NSITypePoint;
	case GT_TYPE_COLOR: return  NSITypeColor;
	case GT_TYPE_VECTOR: return  NSITypeVector;
	case GT_TYPE_TEXTURE: return  NSITypeFloat; // it's a float float[3]
	case GT_TYPE_NORMAL: return  NSITypeNormal;

	case GT_TYPE_NONE:
	{
		// HDK doc for GT_TYPE_NONE says "Implicit type based on data"
		switch( i_storage )
		{
		case GT_STORE_INT32: return NSITypeInteger;
		case GT_STORE_REAL32: return NSITypeFloat;
		case GT_STORE_REAL64: return NSITypeDouble;
		case GT_STORE_STRING: return NSITypeString;
		default:
			return NSITypeInvalid;
		}
	}

	default: return NSITypeInvalid;
	}
}

const char* exporter::transparent_surface_handle()
{
	return "3delight_transparent_surface";
}

/**
	gotchas:
	- When we find a texture coordinate attribute, we output "st".
	  regardless of the name we find (usually "uv"). This is to be
	  consistent with other packages.
	- We have a similar patch for "rest" and "rnml" that we tranform
	  to "Pref" and "Nref". We also need to put them as point/normals
	  for correct transform to object space (as they are defined as
	  float[3]).
	- When converting heightfields to meshes, the GT API use float[3] for P
	  insteda of a point type. We patch that as well.
	- \ref find_attribute() will scan the attibutes in the right order
	  so to respect Houdini's attribute priorities.
*/
void exporter::export_attributes(
	std::vector<std::string> &io_which_ones,
	const GT_Primitive &i_primitive,
	double i_time,
	GT_DataArrayHandle i_vertices_list) const
{

	// Retrieve a context that might redirect the attributes to a shared file
	NSI::Context& nsi = attributes_context();

	if(nsi.Handle() == NSI_BAD_CONTEXT)
	{
		// Those attributes have already been exported in a previous frame
		return;
	}

	/* Get the vertices list if it's provided */
	GT_DataArrayHandle buffer_in_case_we_need_it;
	const int *vertices = nullptr;
	if( i_vertices_list )
	{
		vertices = i_vertices_list->getI32Array( buffer_in_case_we_need_it );
	}

	for(int w = io_which_ones.size()-1; w >= 0; w--)
	{
		std::string name = io_which_ones[w];

		GT_Owner owner;
		GT_DataArrayHandle data = i_primitive.findAttribute( name, owner, 0 );
		if( !data )
			continue;

		int nsi_flags = owner == GT_OWNER_VERTEX ? NSIParamPerVertex : 0;
		NSIType_t nsi_type = gt_to_nsi_type( data->getTypeInfo(), data->getStorage());
		if( nsi_type == NSITypeInvalid || nsi_type == NSITypeString )
		{
			continue;
		}

		io_which_ones.erase(io_which_ones.begin()+w);

		if( name == "uv" )
			name = "st";

		if( name == "rest" )
		{
			name = "Pref";
			nsi_type = NSITypePoint;
		}

		if( name == "rnml" )
		{
			name = "Nref";
			nsi_type = NSITypeNormal;
		}

		if( name == "P" )
		{
			/* heightfields declare this as float[3] :( */
			nsi_type = NSITypePoint;
		}

		if( name == "pscale" )
		{
			name = "width";
		}

		if( owner==GT_OWNER_POINT && i_vertices_list )
		{
			nsi.SetAttribute( m_handle,
				*NSI::Argument( name + ".indices" )
					.SetType( NSITypeInteger )
					->SetCount( i_vertices_list->entries() )
					->SetValuePointer( vertices ) );
		}

		GT_DataArrayHandle buffer_in_case_we_need_it_2;
		if( data->getTypeInfo() == GT_TYPE_TEXTURE )
		{
			nsi.SetAttributeAtTime( m_handle, i_time,
				*NSI::Argument(name)
					.SetArrayType( NSITypeFloat, 3)
					->SetCount( data->entries() )
					->SetValuePointer(
						data->getF32Array(buffer_in_case_we_need_it_2))
					->SetFlags(nsi_flags));
			continue;
		}

		const void* nsi_data = nullptr;
		switch(nsi_type)
		{
			case NSITypeInteger:
				nsi_data = data->getI32Array(buffer_in_case_we_need_it_2);
				break;
			case NSITypeDouble:
			case NSITypeDoubleMatrix:
				nsi_data = data->getF64Array(buffer_in_case_we_need_it_2);
				break;
			default:
				nsi_data = data->getF32Array(buffer_in_case_we_need_it_2);
		}

		if( nsi_type == NSITypeFloat && data->getTupleSize()>1 )
		{
			nsi.SetAttributeAtTime( m_handle, i_time,
				*NSI::Argument(name)
					.SetArrayType( nsi_type, data->getTupleSize() )
					->SetCount( data->entries() )
					->SetValuePointer( nsi_data )
					->SetFlags(nsi_flags));
		}
		else
		{
			if( name == "width" )
			{
				/* To match Houdini/Mantra */
				float *fdata = (float*)nsi_data;
				for( int i=0; i<data->entries(); i++ )
				{
					fdata[i] *= 2.0f;
				}
			}

			nsi.SetAttributeAtTime( m_handle, i_time,
				*NSI::Argument(name)
					.SetType( nsi_type )
					->SetCount( data->entries() )
					->SetValuePointer( nsi_data )
					->SetFlags(nsi_flags));
		}
	}

	return; // so that we don't fall into the void.
}

void exporter::resolve_material_path(
	OP_Node *i_relative_path_root, const char *i_path,
	VOP_Node *o_materials[3] )
{
	assert( i_path );
	assert( o_materials );
	assert( i_relative_path_root );

	o_materials[0] = o_materials[1] = o_materials[2] = nullptr;

	if( !i_path || !i_path[0] || !o_materials)
	{
		return;
	}


	OP_Node* op_node = OPgetDirector()->findNode( i_path );
	VOP_Node *vop_node = nullptr;

	if( op_node )
	{
		vop_node = op_node->castToVOPNode();
	}

	if( !vop_node && i_relative_path_root )
	{
		/* Try a relative search */
		vop_node = i_relative_path_root->findVOPNode( i_path );
	}

	/*
		If the material is actually a 3Delight MaterialBuilder, use its
		materials instead so the builders is kept out of the export logic.
	*/
	VOP_3DelightMaterialBuilder* builder =
		dynamic_cast<VOP_3DelightMaterialBuilder*>(vop_node);

	if( builder )
	{
		/*
			The Material builder will use the intput type to determine
			which kind of meterial this is.
		*/
		builder->get_materials( o_materials );
	}
	else if( vop_node )
	{
		/*
			Use shader's tag to determine if this is a surface, displcement
			or volume. Patterns/textures are assigned to the surface slot as
			this allow the material debugging feature.
		*/
		vop::osl_type stype = vop::shader_type( vop_node );
		if( stype == vop::osl_type::e_surface || stype == vop::osl_type::e_other )
			o_materials[0] = vop_node;
		else if( stype == vop::osl_type::e_displacement )
			o_materials[1] = vop_node;
		if( stype == vop::osl_type::e_volume )
			o_materials[2] = vop_node;
	}
}

/**
*/
NSI::Context&
exporter::attributes_context(
	time_sampler::blur_source i_type)const
{
	bool animated =
		time_sampler::is_time_dependent(
			*m_object,
			m_context.m_current_time,
			i_type);
	return animated ? m_context.m_nsi : m_context.m_static_nsi;
}
