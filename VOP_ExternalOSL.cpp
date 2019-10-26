#include "VOP_ExternalOSL.h"

#include "osl_utilities.h"

#include <PRM/PRM_Include.h>
#include <PRM/PRM_SpareData.h>


/**
	Simple identity macro that does nothing but helps to identify allocations of
	memory that will never be deleted, until we find a way to do so.
*/
#define LEAKED(x) (x)

static const char* k_main_page = "";

static const char* k_position = "Position";
static const char* k_value = "Value";
static const char* k_color = "Color";
static const char* k_interpolation = "Interpolation";

// Additional parameters for the "vdbVolume" shader
static const char* k_grids_page = "Grids";
static const std::vector<const DlShaderInfo::Parameter*> GetVolumeParams()
{
	typedef DlShaderInfo::conststring conststring;
	typedef DlShaderInfo::Parameter Parameter;

	static const unsigned nparams = 5;
	static const unsigned nstrings = 4;
	static const char label[] = "label";

	static const char density_name[] = "density_grid_name";
	static const char temperature_name[] = "temperature_grid_name";
	static const char emission_name[] = "emission_grid_name";
	static const char velocity_name[] = "velocity_grid_name";
	static const char velocity_scale_name[] = "velocity_scale";

	static const conststring name_strings[nparams] =
	{
		conststring(
			density_name, density_name+sizeof(density_name)),
		conststring(
			temperature_name, temperature_name+sizeof(temperature_name)),
		conststring(
			emission_name, emission_name+sizeof(emission_name)),
		conststring(
			velocity_name, velocity_name+sizeof(velocity_name)),
		conststring(
			velocity_scale_name, velocity_scale_name+sizeof(velocity_scale_name))
	};

	static const char density_label[] = "Smoke";
	static const char temperature_label[] = "Temperature";
	static const char emission_label[] = "Emission Intensity";
	static const char velocity_label[] = "Velocity";
	static const char velocity_scale_label[] = "Velocity Scale";

	static const conststring label_strings[nparams] =
	{
		conststring(
			density_label, density_label+sizeof(density_label)),
		conststring(
			temperature_label, temperature_label+sizeof(temperature_label)),
		conststring(
			emission_label, emission_label+sizeof(emission_label)),
		conststring(
			velocity_label, velocity_label+sizeof(velocity_label)),
		conststring(
			velocity_scale_label, velocity_scale_label+sizeof(velocity_scale_label))
	};

	static const char density_default[] = "density";
	static const char temperature_default[] = "";
	static const char emission_default[] = "";
	static const char velocity_default[] = "";
	static const float velocity_scale_default = 1.0f;

	static const conststring default_strings[nstrings] =
	{
		conststring(
			density_default, density_default+sizeof(density_default)),
		conststring(
			temperature_default, temperature_default+sizeof(temperature_default)),
		conststring(
			emission_default, emission_default+sizeof(emission_default)),
		conststring(
			velocity_default, velocity_default+sizeof(velocity_default))
	};

	static Parameter meta[nparams];
	for(unsigned p = 0; p < nparams; p++)
	{
		Parameter& param = meta[p];
		param.name = conststring(label, label+sizeof(label));
		param.type.type = NSITypeString;
		param.type.arraylen = 0;
		param.validdefault = true;
		param.sdefault =
			DlShaderInfo::constvector<conststring>(
				label_strings+p, label_strings + p+1);
	}

	static Parameter params[nparams];
	for(unsigned p = 0; p < nparams; p++)
	{
		Parameter& param = params[p];
		param.name = name_strings[p];
		param.type.type = p < nstrings ? NSITypeString : NSITypeFloat;
		param.type.arraylen = 0;
		param.isoutput = false;
		param.validdefault = true;
		param.varlenarray = false;
		param.isstruct = false;
		param.isclosure = false;
		if(p < nstrings)
		{
			param.sdefault =
				DlShaderInfo::constvector<conststring>(
					default_strings+p, default_strings + p+1);
		}
		param.metadata =
			DlShaderInfo::constvector<Parameter>(meta + p, meta + p+1);

	}
	params[4].fdefault =
		DlShaderInfo::constvector<float>(
			&velocity_scale_default, &velocity_scale_default + 1);

	static std::vector<const Parameter*> out;
	for(unsigned p = 0; p < nparams; p++)
	{
		out.push_back(&params[p]);
	}

	return out;
}

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
			// We don't want to show matrix inputs in the UI
			assert(false);
			break;

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
	const osl_utilities::ParameterMetaData& i_meta)
{
	switch(i_osl_type.type)
	{
		case NSITypeFloat:
		case NSITypeDouble:
			return PRM_FLT;

		case NSITypeInteger:
			if(i_meta.m_widget && i_meta.m_widget == osl_utilities::k_check_box)
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
			// We don't want to show matrix inputs in the UI
			assert(false);
			break;

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
	const osl_utilities::ParameterMetaData& i_meta)
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
			// We don't want to show matrix inputs in the UI
			assert(false);
			break;

		case NSITypePointer:
			// We don't want to show "closure color" inputs in the UI
			assert(false);
			break;

		default:
			break;
	}

	return nullptr;
}

/// Returns a newly allocated PRM_ChoiceList built from a shader parameter.
PRM_ChoiceList*
NewPRMChoiceList(
	const DlShaderInfo::TypeDesc& i_osl_type,
	const osl_utilities::ParameterMetaData& i_meta)
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

		char* end = colon+1+offset;

		// Items are separated with vertical bar
		if(*end && *end != '|')
		{
			assert(false);
			return nullptr;
		}

		char* next = end;
		if(*next)
		{
			next++;
		}

		// Fill gaps with blank items
		while(items->size() <= value)
		{
			items->push_back(PRM_Item("", ""));
		}

		// Use our "options" copy as the strings passed to the item
		*colon = '\0';
		*end = '\0';
		(*items)[value] = PRM_Item(colon+1, options);

		options = next;
	}

	items->push_back(PRM_Item());

	return new PRM_ChoiceList(PRM_CHOICELIST_SINGLE, &(*items)[0]);
}

/// Returns a VOP_Type that corresponds to a DlShaderInfo::TypeDesc
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

/// Adds a PRM_Template to io_templates that matches i_param and its meta-data
static void
AddParameterTemplate(
	std::vector<PRM_Template>& io_templates,
	const DlShaderInfo::Parameter& i_param,
	const osl_utilities::ParameterMetaData& i_meta)
{
	int num_components = i_param.type.arraylen;
	assert(num_components >= 0);
	if(num_components == 0)
	{
		// Not an array
		num_components = 1;
	}
	num_components *= GetNumChannels(i_param.type);

	PRM_Name* name = LEAKED(new PRM_Name(i_param.name.c_str(), i_meta.m_label));
	PRM_Type type = GetPRMType(i_param.type, i_meta);
	PRM_Range* range = LEAKED(NewPRMRange(i_param.type, i_meta));
	PRM_Default* defau1t = LEAKED(NewPRMDefault(i_param));
	PRM_ChoiceList* choices = LEAKED(NewPRMChoiceList(i_param.type, i_meta));

	io_templates.push_back(
		PRM_Template(type, num_components, name, defau1t, choices, range));
}

/**
	Adds a PRM_Template to io_templates that matches i_param, represented by a
	float or color ramp.
*/
static void
AddRampParameterTemplate(
	std::vector<PRM_Template>& io_templates,
	const DlShaderInfo::Parameter& i_param,
	const osl_utilities::ParameterMetaData& i_meta)
{
	using namespace osl_utilities;
	using namespace osl_utilities::ramp;

	assert(i_meta.m_widget);

	// Remove the value suffix from the variable name
	eType ramp_type = GetType(i_meta.m_widget);
	const std::string& value_suffix = GetValueSuffix(ramp_type);
	std::string root_name = RemoveSuffix(i_param.name.string(), value_suffix);

	// Create the names of the other variables controlled by the ramp widget
	const std::string& position_suffix = GetPositionSuffix(ramp_type);
	char* pos_string =
		LEAKED(strdup((root_name + position_suffix + k_index_suffix).c_str()));
	char* value_string =
		LEAKED(strdup((root_name + value_suffix + k_index_suffix).c_str()));
	char* inter_string =
		LEAKED(strdup((root_name + k_interpolation_suffix + k_index_suffix).c_str()));
	bool color = IsColor(ramp_type);
	PRM_Name* pos = LEAKED(new PRM_Name(pos_string, k_position));
	PRM_Name* value = LEAKED(new PRM_Name(value_string, color ? k_color : k_value));
	PRM_Name* inter = LEAKED(new PRM_Name(inter_string, k_interpolation));

	// Create the templates for a single control point
	PRM_Type type = GetPRMType(i_param.type, i_meta);
	unsigned num_channels = GetNumChannels(i_param.type);
	std::vector<PRM_Template>* nodes = LEAKED(new std::vector<PRM_Template>);
	nodes->push_back(
		PRM_Template(PRM_FLT, 1, pos, PRMzeroDefaults));
	nodes->push_back(
		PRM_Template(type, num_channels, value, PRMzeroDefaults));
	nodes->push_back(
		PRM_Template(PRM_ORD, 1, inter, PRMoneDefaults, PRMgetRampInterpMenu()));
	nodes->push_back(
		PRM_Template());

	// Create the main ramp template
	PRM_MultiType multi_type =
		color ? PRM_MULTITYPE_RAMP_RGB : PRM_MULTITYPE_RAMP_FLT;
	char* name_string = LEAKED(strdup(root_name.c_str()));
	PRM_Name* name = LEAKED(new PRM_Name(name_string, i_meta.m_label));
	io_templates.push_back(
		PRM_Template(
			multi_type,
			&(*nodes)[0],
			1,
			name,
			nullptr,
			nullptr,
			&PRM_SpareData::multiStartOffsetZero));
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

		/*
			Don't display matrices in the UI for now. They can still be
			communicated through connections.
		*/
		if(param.type.type == NSITypeMatrix ||
			param.type.type == NSITypeDoubleMatrix)
		{
			continue;
		}

		const char* widget = "";
		osl_utilities::FindMetaData(widget, param.metadata, "widget");
		if(widget == osl_utilities::k_null)
		{
			continue;
		}

		// FIXME : support variable length arrays
		if(param.type.arraylen < 0 &&
			!osl_utilities::ramp::IsRamp(osl_utilities::ramp::GetType(widget)))
		{
			continue;
		}

		const char* page_name = "";
		osl_utilities::FindMetaData(page_name, param.metadata, "page");

		// Ensure that the page exists in page_map
		std::pair<page_map_t::iterator, bool> inserted =
			page_map.insert(page_map_t::value_type(page_name, page_components()));
		// Add the parameter to the page
		page_components& page = inserted.first->second;
		page.push_back(&param);

		// If it's a new page, also add it to page_list
		if(inserted.second)
		{
			page_list.push_back(page_list_t::value_type(page_name, &page));
		}
	}

	// FIXME : this has nothing to do here
	if(i_shader_info.m_dl.shadername() == "vdbVolume")
	{
		// Insert a "Grids" page for the "vdbVolume" shader
		std::pair<page_map_t::iterator, bool> inserted =
			page_map.insert(
				page_map_t::value_type(k_grids_page, page_components()));
		assert(inserted.second);
		page_components& page = inserted.first->second;
		page_list.push_back(page_list_t::value_type(k_grids_page, &page));
		page = GetVolumeParams();
	}

	// Some pages are actually sub-pages, so we merge them together.
	for(unsigned p = 0; p < page_list.size(); p++)
	{
		page_list_t::value_type& pa = page_list[p];
		const char* period = strchr(pa.first, '.');
		if(!period)
		{
			continue;
		}

		page_components& sub_page = *pa.second;
		std::string actual_page_name(pa.first, period);

		// Ensure that the actual page exists in page_map
		std::pair<page_map_t::iterator, bool> inserted =
			page_map.insert(
				page_map_t::value_type(actual_page_name, page_components()));
		page_components& page = inserted.first->second;

		// Append the contents of the sub-page
		page.insert(page.end(), sub_page.begin(), sub_page.end());
		// Clear the sub-page so it's ignored later
		sub_page.clear();

		/*
			If the actual page didn't already exist, add it to page_list at
			the current position.
		*/
		if(inserted.second)
		{
			page_list[p].first = LEAKED(strdup(actual_page_name.c_str()));
			page_list[p].second = &inserted.first->second;
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
	std::vector<PRM_Default>* tabs = LEAKED(new std::vector<PRM_Default>);
	for(unsigned p = 1; p < page_list.size(); p++)
	{
		const page_list_t::value_type& pa = page_list[p];
		if(!pa.second->empty())
		{
			tabs->push_back(PRM_Default(pa.second->size(), pa.first));
		}
	}

	static PRM_Name tabs_name("tabs");

	// Create each page's components
	bool needs_switcher = tabs->size() > 0;
	for(const page_list_t::value_type& pa : page_list)
	{
		for(const DlShaderInfo::Parameter* param : *pa.second)
		{
			osl_utilities::ParameterMetaData meta;
			meta.m_label = param->name.c_str();
			osl_utilities::GetParameterMetaData(meta, param->metadata);

			if(osl_utilities::ramp::IsRamp(
				osl_utilities::ramp::GetType(meta.m_widget)))
			{
				AddRampParameterTemplate(*templates, *param, meta);
			}
			else
			{
				AddParameterTemplate(*templates, *param, meta);
			}
		}

		// Add the switcher once the main page's parameters have been created
		if(needs_switcher)
		{
			templates->push_back(
				PRM_Template(PRM_SWITCHER, tabs->size(), &tabs_name, &(*tabs)[0]));
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
VOP_ExternalOSL::minInputs() const
{
	return 0;
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

#if HDK_API_VERSION >= 18000000
UT_StringHolder VOP_ExternalOSL::getShaderName(
	VOP_ShaderNameStyle style,
	VOP_Type shader_type) const
{
	/* This name is what becomes the shader id in USD land. Our Hydra plugin
	   will use it to find the correct shader. */
	if( style == VOP_ShaderNameStyle::PLAIN )
		return m_shader_info.m_dl.shadername().c_str();

	return VOP_Node::getShaderName(style, shader_type);
}
#endif

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
VOP_ExternalOSL::getAllowedInputTypeInfosSubclass(
	unsigned i_idx,
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
	const StructuredShaderInfo& i_shader_info,
	const std::string& i_menu_name)
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
	osl_utilities::FindMetaData(name, i_shader_info.m_dl.metadata(), "niceName");
	setEnglish(name);

	setOpTabSubMenuPath(i_menu_name.c_str());
	/*
	FIXME:
	We need to setShaderType(VPO_BSDF_SHADER) on surface meterials. Such OSL
	shaders have tags[] = {"surface",..} in them.
	*/

	/*
		The RenderMask is what ends up being the MaterialNetworkSelector in
		Hydra. If we don't set it, the default translator will not provide
		networks at all. And if it does not match the Hydra plugin, we won't
		see the networks there.
	*/
	static_cast<VOP_OperatorInfo*>(getOpSpecificData())->setRenderMask("nsi");
}
