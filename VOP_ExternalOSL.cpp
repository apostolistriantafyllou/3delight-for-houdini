#include "VOP_ExternalOSL.h"


// Returns a PRM_Type that corresponds to a DlShaderInfo::TypeDesc
static PRM_Type GetPRMType(const DlShaderInfo::TypeDesc& i_osl_type)
{
	switch(i_osl_type.type)
	{
		case NSITypeFloat:
		case NSITypeDouble:
			return PRM_FLT;

		case NSITypeInteger:
			return PRM_INT;

		case NSITypeString:
			return PRM_STRING;

		case NSITypeColor:
			return PRM_RGB;

		case NSITypePoint:
		case NSITypeVector:
		case NSITypeNormal:
			return PRM_XYZ;

		case NSITypeMatrix:
		case NSITypeDoubleMatrix:
			// FIXME
			return PRM_STRING;

		case NSITypePointer:
			// We don't want to show "closure color" inputs in the UI
			assert(false);
			break;

		default:
			break;
	}

	return PRM_STRING;
}

// Returns a VOP_Type that corresponds to a DlShaderInfo::TypeDesc
static VOP_Type GetVOPType(const DlShaderInfo::TypeDesc& i_osl_type)
{
	switch(i_osl_type.type)
	{
		case NSITypeFloat:
		case NSITypeDouble:
			return VOP_TYPE_FLOAT;

		case NSITypeInteger:
			return VOP_TYPE_INTEGER;

		case NSITypeString:
			return VOP_TYPE_STRING;

		case NSITypeColor:
			return VOP_TYPE_COLOR;

		case NSITypePoint:
			return VOP_TYPE_POINT;

		case NSITypeVector:
			return VOP_TYPE_VECTOR;

		case NSITypeNormal:
			return VOP_TYPE_NORMAL;

		case NSITypeMatrix:
		case NSITypeDoubleMatrix:
			return VOP_TYPE_MATRIX4;

		case NSITypePointer:
			// Corresponds to "closure color"
			return VOP_TYPE_COLOR;

		default:
			break;
	}

	return VOP_TYPE_STRING;
}


StructuredShaderInfo::StructuredShaderInfo(const DlShaderInfo* i_info)
	:	m_dl(*i_info)
{
	assert(i_info);

	unsigned nparams = m_dl.nparams();
	for(unsigned p = 0; p < nparams; p++)
	{
		const DlShaderInfo::Parameter* param = m_dl.getparam(p);
		assert(param);
		if(param->isoutput)
		{
			m_outputs.push_back(p);
		}
		else
		{
			m_inputs.push_back(p);
		}
	}
}

OP_Node*
VOP_ExternalOSL::alloc(OP_Network* net, const char* name, OP_Operator* entry)
{
	VOP_ExternalOSLOperator* osl_entry =
		dynamic_cast<VOP_ExternalOSLOperator*>(entry);
	assert(osl_entry);
    return new VOP_ExternalOSL(net, name, osl_entry);
}

PRM_Template*
VOP_ExternalOSL::GetTemplates(const StructuredShaderInfo& i_shader_info)
{
	typedef std::vector<const DlShaderInfo::Parameter*> page_components;
	typedef std::map<std::string, page_components> page_map_t;
	typedef std::vector<std::pair<const char*, page_components*> > page_list_t;

	/*
		Classify shader parameters into pages.  We sort them using a map, but we
		keep a separate, ordered page list to keep them in the same order as the
		first parameter of each page.
	*/
	page_map_t page_map;
	page_list_t page_list;
	for(unsigned p = 0; p < i_shader_info.NumInputs(); p++)
	{
		const DlShaderInfo::Parameter& param = i_shader_info.GetInput(p);
		const char* page_name = "";
		for(const DlShaderInfo::Parameter& meta : param.metadata)
		{
			if(meta.name == "page")
			{
				page_name = meta.sdefault[0].c_str();
				break;
			}
		}

		std::pair<page_map_t::iterator, bool> inserted =
			page_map.insert(page_map_t::value_type(page_name, page_components()));
		inserted.first->second.push_back(&param);

		if(inserted.second)
		{
			page_list.push_back(
				page_list_t::value_type(page_name, &inserted.first->second));
		}
	}

	/*
		The templates and their components (names and such) are dynamically
		allocated here but never deleted, since they're expected to be valid for
		the duration of the process. It's not that different from allocating
		them as static variables, which is the usual way to do this when only
		one type of operator is defined. Allocating them dynamically allows
		multiple operator types, each with an arbitrary number of parameters, to
		be created.
		It could be interesting to keep them around in VOP_ExternalOSLOperator
		and delete them when its destructor is called.
	*/
	std::vector<PRM_Template>* templates = new std::vector<PRM_Template>;

	// Create the pages switcher
	if(page_list.size() > 1)
	{
		PRM_Name* tabs_name = new PRM_Name("tabs");
		std::vector<PRM_Default>* tabs = new std::vector<PRM_Default>;
		for(const page_list_t::value_type& pa : page_list)
		{
			tabs->push_back(PRM_Default(pa.second->size(), pa.first));
		}

		templates->push_back(
			PRM_Template(PRM_SWITCHER, tabs->size(), tabs_name, &(*tabs)[0]));
	}

	// Create each page's components
	for(const page_list_t::value_type& pa : page_list)
	{
		for(const DlShaderInfo::Parameter* param : *pa.second)
		{
			// Closures can only be read through connections
			if(param->isclosure)
			{
				continue;
			}

			int num_components = param->type.arraylen;
			if(num_components == 0)
			{
				num_components = 1;
			}
			else if(num_components < 0)
			{
				// FIXME : support variable length arrays
				continue;
			}

			PRM_Name* name =
				new PRM_Name(param->name.c_str(), param->name.c_str());
			templates->push_back(
				PRM_Template(GetPRMType(param->type), num_components, name));
		}
	}

	templates->push_back(PRM_Template());

	return &(*templates)[0];
}

VOP_ExternalOSL::VOP_ExternalOSL(
	OP_Network* parent, const char* name, VOP_ExternalOSLOperator* entry)
    :	VOP_Node(parent, name, entry),
		m_shader_info(entry->m_shader_info)
{
}

VOP_ExternalOSL::~VOP_ExternalOSL()
{
}

const char*
VOP_ExternalOSL::inputLabel(unsigned i_idx) const
{
	assert(i_idx < m_shader_info.NumInputs());
	const DlShaderInfo::Parameter& param = m_shader_info.GetInput(i_idx);
	assert(!param.isoutput);
	return param.name.c_str();
}

const char*
VOP_ExternalOSL::outputLabel(unsigned i_idx) const
{
	assert(i_idx < m_shader_info.NumOutputs());
	const DlShaderInfo::Parameter& param = m_shader_info.GetOutput(i_idx);
	assert(param.isoutput);
	return param.name.c_str();
}

unsigned
VOP_ExternalOSL::getNumVisibleInputs() const
{
	return m_shader_info.NumInputs();
}

unsigned
VOP_ExternalOSL::orderedInputs() const
{
	return m_shader_info.NumInputs();
}

void
VOP_ExternalOSL::getInputNameSubclass(UT_String &in, int i_idx) const
{
	in = inputLabel(i_idx);
}

int
VOP_ExternalOSL::getInputFromNameSubclass(const UT_String &in) const
{
	unsigned num_inputs = m_shader_info.NumInputs();
	for(unsigned p = 0; p < num_inputs; p++)
	{
		if(m_shader_info.GetInput(p).name == in)
		{
			return p;
		}
	}

	return -1;
}

void
VOP_ExternalOSL::getInputTypeInfoSubclass(VOP_TypeInfo &o_type_info, int i_idx)
{
	assert(i_idx < m_shader_info.NumInputs());
	const DlShaderInfo::Parameter& param = m_shader_info.GetInput(i_idx);
	assert(!param.isoutput);
	o_type_info.setType(GetVOPType(param.type));
}

void	 
VOP_ExternalOSL::getAllowedInputTypeInfosSubclass( unsigned i_idx,
					      VOP_VopTypeInfoArray &o_type_infos)
{
	VOP_TypeInfo info;
	getInputTypeInfoSubclass(info, i_idx);
	o_type_infos.append(info);
}

void
VOP_ExternalOSL::getOutputNameSubclass(UT_String &name, int i_idx) const
{
	name = outputLabel(i_idx);
}

void
VOP_ExternalOSL::getOutputTypeInfoSubclass(VOP_TypeInfo& o_type_info, int i_idx)
{
	assert(i_idx < m_shader_info.NumOutputs());
	const DlShaderInfo::Parameter& param = m_shader_info.GetOutput(i_idx);
	assert(param.isoutput);
	o_type_info.setType(GetVOPType(param.type));
}

