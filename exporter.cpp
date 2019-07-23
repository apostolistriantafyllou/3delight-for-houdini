#include "exporter.h"

#include "context.h"

#include <GT/GT_Handles.h>
#include <OBJ/OBJ_Node.h>
#include <UT/UT_TagManager.h>

#include <iostream>

GT_PrimitiveHandle exporter::sm_invalid_gt_primitive;
static const std::string k_light_category_prefix = ".!@category:";

static int s_handle = 0;
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
	m_handle += "|object|";
	m_handle += std::to_string( s_handle ++ );
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
	\brief Connect to the parent object of this exporter and also
	perform local material assignment if this op node has a
	"shop_materialpath" parameter.
*/
void exporter::connect( void ) const
{
	if( !m_object )
		return;

	std::string parent;

	if( m_instanced )
	{
		/**
			This geometry will be connected to the "sourcemodels" of an NSI
			instances node and doesn't need to be connected to any other
			parent.

			\ref instancer::connect
		*/
		goto assign_material; // one goto for assembly gods.
	}

	/*
	   We support the transformation of the GT_Primitive by inserting
	   a local NSI transform node between the object and its parent.
	*/
	if( m_gt_primitive != sm_invalid_gt_primitive )
	{
		parent = m_handle + "|transform";
		const GT_TransformHandle &transform =
			m_gt_primitive->getPrimitiveTransform();
		UT_Matrix4D matrix;
		transform->getMatrix( matrix );

		m_nsi.Create( parent, "transform" );
		m_nsi.SetAttribute( parent,
				NSI::DoubleMatrixArg( "transformationmatrix", matrix.data() ) );
		m_nsi.Connect( parent, "", m_object->getFullPath().c_str(), "objects" );
	}
	else
	{
		parent = m_object->getFullPath().c_str();
	}

	m_nsi.Connect( m_handle, "", parent, "objects" );


assign_material:

	int lightcategories_index = m_object->getParmIndex("lightcategories");
	int shop_materialpath_index = m_object->getParmIndex( "shop_materialpath" );

	if(lightcategories_index < 0 && shop_materialpath_index < 0)
	{
		return;
	}

	std::string attributes( m_handle + "|attributes" );
	m_nsi.Create( attributes, "attributes" );
	m_nsi.Connect( attributes, "", m_handle, "geometryattributes" );

	// Do light linking
	if(lightcategories_index >= 0)
	{
		UT_String lightcategories;
		m_object->evalString(lightcategories, lightcategories_index, 0, 0.0f);
		export_light_categories(lightcategories, attributes);
	}

	/* Do local material assignment */
	if( shop_materialpath_index >= 0 )
	{
		UT_String material_path;
		m_object->evalString( material_path, "shop_materialpath", 0, 0.f );

		if( material_path.length() > 0 )
		{
			m_nsi.Connect( material_path.buffer(), "", attributes, "surfaceshader" );
		}
	}
}


/**
	Utility to get an NSI type from a GT_Type.

	\returns NSITypeInvalid if we don't know what to do with
	the type
*/
NSIType_t gt_to_nsi_type( GT_Type i_type )
{
	switch( i_type)
	{
	case GT_TYPE_POINT: return  NSITypePoint;
	case GT_TYPE_COLOR: return  NSITypeColor;
	case GT_TYPE_VECTOR: return  NSITypeVector;
	case GT_TYPE_TEXTURE: return  NSITypeFloat; // it's a float float[3]
	case GT_TYPE_NORMAL: return  NSITypeNormal;
	case GT_TYPE_NONE: return  NSITypeFloat;
	default:
		break;
	}
	return NSITypeInvalid;
}

/**
	gotchas:
	- When we find a texture coordinate attribute, we output "st".
	  regardless of the name we find (usually "uv"). This is to be
	  consistant with other packages.  After the first one we add a
	  number.
*/
void exporter::export_attributes(
	const GT_AttributeListHandle *i_attributes,
	int i_n, double i_time,
	std::vector<const char *> &io_which_ones ) const
{
	for( int i=0; i<i_n; i++ )
	{
		auto it = io_which_ones.begin();
		while( it != io_which_ones.end() )
		{
			const char *name = *it;

			const GT_DataArrayHandle &data = i_attributes[i]->get( name );
			if( data.get() == nullptr )
			{
				/* Name not part of this attributes list */
				it ++;
				continue;
			}

			NSIType_t nsi_type = gt_to_nsi_type( data->getTypeInfo() );
			if( nsi_type == NSITypeInvalid )
			{
				std::cout << "unsupported attribute type " << data->getTypeInfo()
					<< " of name " << name << " on " << m_handle;
				it ++ ;
				continue;
			}

			it = io_which_ones.erase( it );
			GT_DataArrayHandle buffer_in_case_we_need_it;

			if( data->getTypeInfo() == GT_TYPE_TEXTURE )
			{
				m_nsi.SetAttributeAtTime( m_handle, i_time,
					*NSI::Argument("st")
						.SetArrayType( NSITypeFloat, 3)
						->SetCount( data->entries() )
						->SetValuePointer(
							data->getF32Array(buffer_in_case_we_need_it)));
				continue;
			}

			m_nsi.SetAttributeAtTime( m_handle, i_time,
				*NSI::Argument(name)
					.SetType( nsi_type )
					->SetCount( data->entries() )
					->SetValuePointer( data->getF32Array(buffer_in_case_we_need_it)));
		}
	}

	return; // so that we don't fall into the void.
}

void exporter::export_light_categories(
	const UT_String& i_categories,
	const std::string& i_attributes_handle)const
{
	UT_String categories = i_categories;
	if(!categories.c_str())
	{
		categories = "";
	}

	UT_TagManager tag_manager;
	UT_String errors;
	UT_TagExpressionPtr categories_expr =
		tag_manager.createExpression(categories, errors);

	// Trivial and (hopefully) most common case
	if(categories_expr->isTautology())
	{
		/*
			The object sees all lights, which is the default, so no connections
			are necessary (ie : no lights to turn off).
		*/
		return;
	}

	std::string cat_handle = k_light_category_prefix + categories.toStdString();
	std::string cat_attr_handle = cat_handle + "|attributes";

	auto insertion = m_context.m_exported_lights_categories.insert(cat_handle);
	if(insertion.second)
	{
		/*
			This is the first time we use this NSI set, so we have to create it
			first!
		*/
		m_nsi.Create(cat_handle, "set");
		m_nsi.Create(cat_attr_handle, "attributes");
		m_nsi.Connect(cat_attr_handle, "", cat_handle, "geometryattributes");

		for(OBJ_Node* light : m_context.m_lights_to_render)
		{
			/*
				Parse the list of categories for the light. This shouldn't be
				expensive. If it is, we might want to store it in the light
				exporter and work on a list of those, instead.
			*/
			UT_String tags_string;
			light->evalString(tags_string, "categories", 0, 0.0);
			UT_TagListPtr tags = tag_manager.createList(tags_string, errors);

			// Add the lights we have to turn off to the set
			if(!tags->match(*categories_expr))
			{
				m_nsi.Connect(
					light->getFullPath().toStdString(), "",
					cat_handle, "members");
			}
		}
	}

	// Turn off lights in the set for this object
	m_nsi.Connect(
		i_attributes_handle, "",
		cat_attr_handle, "visibility",
		(
			NSI::IntegerArg("value", false),
			NSI::IntegerArg("priority", 1)
		) );
}
