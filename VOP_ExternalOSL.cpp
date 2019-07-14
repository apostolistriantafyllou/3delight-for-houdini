#include "VOP_ExternalOSL.h"

#include <PRM/PRM_Include.h>


/**
	Simple identity macro that does nothing but helps to identify allocations of
	memory that will never be deleted, until we find a way to do so.
*/
#define LEAKED(x) (x)

const char* k_main_page = "";

struct ParameterMetaData
{
	const char* m_label = nullptr;
	const char* m_widget = nullptr;
	const char* m_options = nullptr;
	const int* m_imin = nullptr;
	const int* m_imax = nullptr;
	const int* m_slider_imin = nullptr;
	const int* m_slider_imax = nullptr;
	const float* m_fmin = nullptr;
	const float* m_fmax = nullptr;
	const float* m_slider_fmin = nullptr;
	const float* m_slider_fmax = nullptr;
};

/// Returns the number of scalar channels in the specified type
static unsigned GetNumChannels(const DlShaderInfo::TypeDesc& i_osl_type)
{
	switch(i_osl_type.type)
	{
		case NSITypeFloat:
		case NSITypeDouble:
		case NSITypeInteger:
		case NSITypeString:
			return 1;

		case NSITypeColor:
		case NSITypePoint:
		case NSITypeVector:
		case NSITypeNormal:
			return 3;

		case NSITypeMatrix:
		case NSITypeDoubleMatrix:
			return 16;

		case NSITypePointer:
			// We don't want to show "closure color" inputs in the UI
			assert(false);
			break;

		default:
			break;
	}

	return 1;
}

/// Returns a PRM_Type that corresponds to a DlShaderInfo::TypeDesc
static PRM_Type GetPRMType(
	const DlShaderInfo::TypeDesc& i_osl_type,
	const ParameterMetaData& i_meta)
{
	switch(i_osl_type.type)
	{
		case NSITypeFloat:
		case NSITypeDouble:
			return PRM_FLT;

		case NSITypeInteger:
			if(i_meta.m_widget && strcmp(i_meta.m_widget, "checkBox") == 0)
			{
				return PRM_TOGGLE;
			}
			else if(i_meta.m_options)
			{
				return PRM_ORD;
			}

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

/**
	Returns a newly allocated PRM_Range built from the shader parameter's
	meta-data.
	Unfortunately, Houdini doesn't seem to allow the specification of a soft,
	"UI" minimum along with a hard minimum (or the equivalent for the maximum).
	So, there is only one min and one max, and each of them can be either soft
	or hard. This means that a "soft" bound imply an infinite corresponding
	"hard" bound, since the "soft" bound is only appolied in the UI.
*/
static PRM_Range*
NewPRMRange(
	const DlShaderInfo::TypeDesc& i_osl_type,
	const ParameterMetaData& i_meta)
{
	if(i_osl_type.type == NSITypeFloat || i_osl_type.type == NSITypeDouble)
	{
		if(!(i_meta.m_fmin || i_meta.m_slider_fmin) ||
			!(i_meta.m_fmax || i_meta.m_slider_fmax))
		{
			return nullptr;
		}

		return
			new PRM_Range(
				i_meta.m_fmin ? PRM_RANGE_RESTRICTED : PRM_RANGE_UI,
				i_meta.m_fmin ? *i_meta.m_fmin : *i_meta.m_slider_fmin,
				i_meta.m_fmax ? PRM_RANGE_RESTRICTED : PRM_RANGE_UI,
				i_meta.m_fmax ? *i_meta.m_fmax : *i_meta.m_slider_fmax);
	}
	else if(i_osl_type.type == NSITypeInteger)
	{
		if(!(i_meta.m_imin || i_meta.m_slider_imin) ||
			!(i_meta.m_imax || i_meta.m_slider_imax))
		{
			return nullptr;
		}

		return
			new PRM_Range(
				i_meta.m_imin ? PRM_RANGE_RESTRICTED : PRM_RANGE_UI,
				float(i_meta.m_imin ? *i_meta.m_imin : *i_meta.m_slider_imin),
				i_meta.m_imax ? PRM_RANGE_RESTRICTED : PRM_RANGE_UI,
				float(i_meta.m_imax ? *i_meta.m_imax : *i_meta.m_slider_imax));
	}

	return nullptr;
}

/// Returns a newly allocated array of PRM_Defaults built from a shader parameter.
static PRM_Default* NewPRMDefault(
	const DlShaderInfo::Parameter& i_param)
{
	switch(i_param.type.type)
	{
		case NSITypeFloat:
		case NSITypeDouble:
			if(!i_param.fdefault.empty())
			{
				PRM_Default* def = new PRM_Default[1];
				def[0] = PRM_Default(i_param.fdefault[0]);
				return def;
			}
			return nullptr;

		case NSITypeInteger:
			if(!i_param.idefault.empty())
			{
				PRM_Default* def = new PRM_Default[1];
				def[0] = PRM_Default(i_param.idefault[0]);
				return def;
			}
			return nullptr;

		case NSITypeString:
			if(!i_param.sdefault.empty())
			{
				PRM_Default* def = new PRM_Default[1];
				def[0] = PRM_Default(0.0f, i_param.sdefault[0].c_str());
				return def;
			}
			return nullptr;

		case NSITypeColor:
		case NSITypePoint:
		case NSITypeVector:
		case NSITypeNormal:
			if(i_param.fdefault.size() >= 3)
			{
				PRM_Default* def = new PRM_Default[3];
				for(unsigned i = 0; i < 3; i++)
				{
					def[i] = PRM_Default(i_param.fdefault[i]);
				}
				return def;
			}
			return nullptr;

		case NSITypeMatrix:
		case NSITypeDoubleMatrix:
			if(i_param.fdefault.size() >= 16)
			{
				PRM_Default* def = new PRM_Default[16];
				for(unsigned i = 0; i < 16; i++)
				{
					def[i] = PRM_Default(i_param.fdefault[i]);
				}
				return def;
			}
			return nullptr;

		case NSITypePointer:
			// We don't want to show "closure color" inputs in the UI
			assert(false);
			break;

		default:
			break;
	}

	return nullptr;
}

// Returns a newly allocated PRM_ChoiceList built from a shader parameter.
PRM_ChoiceList*
NewPRMChoiceList(
	const DlShaderInfo::TypeDesc& i_osl_type,
	const ParameterMetaData& i_meta)
{
	if(i_osl_type.type != NSITypeInteger || !i_meta.m_options)
	{
		return nullptr;
	}

	char* options = LEAKED(strdup(i_meta.m_options));
	std::vector<PRM_Item>* items = LEAKED(new std::vector<PRM_Item>);
	while(*options)
	{
		// The label is terminated by a colon
		char* colon = strchr(options, ':');
		if(!colon)
		{
			assert(false);
			return nullptr;
		}

		// Then comes the value
		int value = 0;
		int offset = 0;
		int read = sscanf(colon+1, "%d%n", &value, &offset);
		if(read == 0 || value < 0)
		{
			assert(false);
			return nullptr;
		}

		char* next = colon+1+offset;

		// Items are separated with vertical bar
		if(*next && *next != '|')
		{
			assert(false);
			return nullptr;
		}
		if(*next)
		{
			next++;
		}

		// Fill gaps with blank items
		while(items->size() <= value)
		{
			items->push_back(PRM_Item("", ""));
		}

		// Use our "options" copy as the string passed to the item
		*colon = '\0';
		(*items)[value] = PRM_Item("", options);

		options = next;
	}

	items->push_back(PRM_Item());

	return new PRM_ChoiceList(PRM_CHOICELIST_SINGLE, &(*items)[0]);
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

static void
FindMetaData(
	const char*& o_value,
	const DlShaderInfo::constvector<DlShaderInfo::Parameter>& i_metadata,
	const char* i_name)
{
	for(const DlShaderInfo::Parameter& meta : i_metadata)
	{
		if(meta.name == i_name)
		{
			o_value = meta.sdefault[0].c_str();
			return;
		}
	}
}

static void
GetParameterMetaData(
	ParameterMetaData& o_data,
	const DlShaderInfo::constvector<DlShaderInfo::Parameter>& i_metadata)
{
	for(const DlShaderInfo::Parameter& meta : i_metadata)
	{
		if(meta.name == "label")
		{
			if(!meta.sdefault.empty())
			{
				o_data.m_label = meta.sdefault[0].c_str();
			}
		}
		if(meta.name == "widget")
		{
			if(!meta.sdefault.empty())
			{
				o_data.m_widget = meta.sdefault[0].c_str();
			}
		}
		if(meta.name == "options")
		{
			if(!meta.sdefault.empty())
			{
				o_data.m_options = meta.sdefault[0].c_str();
			}
		}
		else if(meta.name == "min")
		{
			if(!meta.idefault.empty())
			{
				o_data.m_imin = &meta.idefault[0];
			}
			if(!meta.fdefault.empty())
			{
				o_data.m_fmin = &meta.fdefault[0];
			}
		}
		else if(meta.name == "max")
		{
			if(!meta.idefault.empty())
			{
				o_data.m_imax = &meta.idefault[0];
			}
			if(!meta.fdefault.empty())
			{
				o_data.m_fmax = &meta.fdefault[0];
			}
		}
		else if(meta.name == "slidermin")
		{
			if(!meta.idefault.empty())
			{
				o_data.m_slider_imin = &meta.idefault[0];
			}
			if(!meta.fdefault.empty())
			{
				o_data.m_slider_fmin = &meta.fdefault[0];
			}
		}
		else if(meta.name == "slidermax")
		{
			if(!meta.idefault.empty())
			{
				o_data.m_slider_imax = &meta.idefault[0];
			}
			if(!meta.fdefault.empty())
			{
				o_data.m_slider_fmax = &meta.fdefault[0];
			}
		}
	}
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
	std::pair<page_map_t::iterator, bool> main =
		page_map.insert(page_map_t::value_type(k_main_page, page_components()));
	page_list_t page_list(1, page_list_t::value_type(k_main_page, &main.first->second));
	for(unsigned p = 0; p < i_shader_info.NumInputs(); p++)
	{
		const DlShaderInfo::Parameter& param = i_shader_info.GetInput(p);

		// Closures can only be read through connections
		if(param.isclosure)
		{
			continue;
		}

		const char* widget = "";
		FindMetaData(widget, param.metadata, "widget");
		if(strcmp(widget, "null") == 0)
		{
			continue;
		}

		const char* page_name = "";
		FindMetaData(page_name, param.metadata, "page");

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
		and delete them when its destructor is called. Unfortunately, even
		VOP_Operator's destructor doesn't seem to even be called, so we simply
		use the LEAKED macro to mark them until we find out what we should do.
	*/
	std::vector<PRM_Template>* templates = LEAKED(new std::vector<PRM_Template>);

	// Prepare a tab for each page (except the main one)
	assert(!page_list.empty());
	assert(page_list[0].first == k_main_page);
	PRM_Name* tabs_name = LEAKED(new PRM_Name("tabs"));
	std::vector<PRM_Default>* tabs = LEAKED(new std::vector<PRM_Default>);
	for(unsigned p = 1; p < page_list.size(); p++)
	{
		const page_list_t::value_type& pa = page_list[p];
		if(!pa.second->empty())
		{
			tabs->push_back(PRM_Default(pa.second->size(), pa.first));
		}
	}

	// Create each page's components
	bool needs_switcher = tabs->size() > 0;
	for(const page_list_t::value_type& pa : page_list)
	{
		for(const DlShaderInfo::Parameter* param : *pa.second)
		{
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

			num_components *= GetNumChannels(param->type);

			ParameterMetaData meta;
			meta.m_label = param->name.c_str();
			GetParameterMetaData(meta, param->metadata);

			PRM_Name* name =
				LEAKED(new PRM_Name(param->name.c_str(), meta.m_label));
			PRM_Type type = GetPRMType(param->type, meta);
			PRM_Range* range = LEAKED(NewPRMRange(param->type, meta));
			PRM_Default* defau1t = LEAKED(NewPRMDefault(*param));
			PRM_ChoiceList* choices =
				LEAKED(NewPRMChoiceList(param->type, meta));

			templates->push_back(
				PRM_Template(type, num_components, name, defau1t, choices, range));
		}

		// Add the switcher once the main page's parameters have been created
		if(needs_switcher)
		{
			templates->push_back(
				PRM_Template(PRM_SWITCHER, tabs->size(), tabs_name, &(*tabs)[0]));
			needs_switcher = false;
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


VOP_ExternalOSLOperator::VOP_ExternalOSLOperator(
	const StructuredShaderInfo& i_shader_info)
	:	VOP_Operator(
			("3Delight::" + i_shader_info.m_dl.shadername().string()).c_str(),
			i_shader_info.m_dl.shadername().c_str(),
			VOP_ExternalOSL::alloc,
			VOP_ExternalOSL::GetTemplates(i_shader_info),
			VOP_ExternalOSL::theChildTableName,
			i_shader_info.NumInputs(),
			i_shader_info.NumInputs(),
			"*",
			nullptr,
			i_shader_info.IsTerminal() ? OP_FLAG_OUTPUT : 0u,
			i_shader_info.NumOutputs()),
		m_shader_info(i_shader_info)
{
	const char* name = i_shader_info.m_dl.shadername().c_str();
	FindMetaData(name, i_shader_info.m_dl.metadata(), "niceName");
	setEnglish(name);

	setOpTabSubMenuPath("3Delight");
}
