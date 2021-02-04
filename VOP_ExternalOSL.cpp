#include "VOP_ExternalOSL.h"

#include "osl_utilities.h"
#include "dl_system.h"

#include <PRM/PRM_Include.h>
#include <PRM/PRM_SpareData.h>
#include <unordered_set>


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

	static const unsigned nparams = 6;
	static const unsigned nstrings = 5;
	static const char label[] = "label";

	using namespace VolumeGridParameters;

	static const conststring name_strings[nparams] =
	{
		conststring(
			density_name, density_name+sizeof(density_name)),
		conststring(
			color_name, color_name+sizeof(color_name)),
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
	static const char color_label[] = "Smoke Color";
	static const char temperature_label[] = "Temperature";
	static const char emission_label[] = "Emission Intensity";
	static const char velocity_label[] = "Velocity";
	static const char velocity_scale_label[] = "Velocity Scale";

	static const conststring label_strings[nparams] =
	{
		conststring(
			density_label, density_label+sizeof(density_label)),
		conststring(
			color_label, color_label+sizeof(color_label)),
		conststring(
			temperature_label, temperature_label+sizeof(temperature_label)),
		conststring(
			emission_label, emission_label+sizeof(emission_label)),
		conststring(
			velocity_label, velocity_label+sizeof(velocity_label)),
		conststring(
			velocity_scale_label, velocity_scale_label+sizeof(velocity_scale_label))
	};

	static Parameter meta[nparams];
	for(unsigned p = 0; p < nparams; p++)
	{
		Parameter& param = meta[p];
		param.name = conststring(label, label+sizeof(label));
		param.type.elementtype = NSITypeString;
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
		param.type.elementtype = p < nstrings ? NSITypeString : NSITypeFloat;
		param.type.arraylen = 0;
		param.isoutput = false;
		param.validdefault = true;
		param.varlenarray = false;
		param.isstruct = false;
		param.isclosure = false;
		param.metadata =
			DlShaderInfo::constvector<Parameter>(meta + p, meta + p+1);
	}

	/* This one is the actual NSI default so we define it here. */
	params[nstrings].fdefault =
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
	switch(i_osl_type.elementtype)
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
			return 3;
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
	switch(i_osl_type.elementtype)
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
			if(i_meta.m_widget && i_meta.m_widget == osl_utilities::k_filename)
			{
				return PRM_PICFILE;
			}
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
			return PRM_RGB;
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
	if(i_osl_type.elementtype == NSITypeFloat ||
	   i_osl_type.elementtype == NSITypeDouble)
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
	else if(i_osl_type.elementtype == NSITypeInteger)
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
	const DlShaderInfo::Parameter& i_param,
	unsigned i_nb_defaults)
{
	switch(i_param.type.elementtype)
	{
		case NSITypeFloat:
		case NSITypeDouble:
			if(i_param.fdefault.size() >= i_nb_defaults)
			{
				PRM_Default* def = new PRM_Default[i_nb_defaults];
				for(unsigned i = 0; i < i_nb_defaults; i++)
				{
					def[i] = PRM_Default(i_param.fdefault[i]);
				}
				return def;
			}
			return nullptr;

		case NSITypeInteger:
			if(i_param.idefault.size() >= i_nb_defaults)
			{
				PRM_Default* def = new PRM_Default[i_nb_defaults];
				for(unsigned i = 0; i < i_nb_defaults; i++)
				{
					def[i] = PRM_Default(i_param.idefault[i]);
				}
				return def;
			}
			return nullptr;

		case NSITypeString:
			if(i_param.sdefault.size() >= i_nb_defaults)
			{
				PRM_Default* def = new PRM_Default[i_nb_defaults];
				for(unsigned i = 0; i < i_nb_defaults; i++)
				{
					def[i] = PRM_Default(0.0f, i_param.sdefault[i].c_str());
				}
				return def;
			}
			return nullptr;

		case NSITypeColor:
		case NSITypePoint:
		case NSITypeVector:
		case NSITypeNormal:
		case NSITypePointer:
			if(i_param.fdefault.size() >= i_nb_defaults)
			{
				PRM_Default* def = new PRM_Default[i_nb_defaults];
				for(unsigned i = 0; i < i_nb_defaults; i++)
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
	if(!i_meta.m_options)
	{
		return nullptr;
	}

	char* options = LEAKED(strdup(i_meta.m_options));
	std::vector<PRM_Item>* items = LEAKED(new std::vector<PRM_Item>);

	if(i_osl_type.IsOneInteger())
	{
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
	}
	else if(i_osl_type.IsOneString())
	{
		while(options && *options)
		{
			// Items are separated by vertical bars.
			char *next = strchr(options, '|');
			if(next)
			{
				/* Cut the string. */
				*next = '\0';
				++next;
			}

			/* Give the cut string directly to the item. */
			items->push_back(PRM_Item(options, options));

			options = next;
		}
	}
	else
	{
		/* Whatever this is is not handled yet. */
		return nullptr;
	}

	items->push_back(PRM_Item());

	return new PRM_ChoiceList(PRM_CHOICELIST_SINGLE, &(*items)[0]);
}

VOP_Type VOP_ExternalOSL::GetVOPType(const DlShaderInfo::Parameter& i_osl_param)
{
	if( i_osl_param.isclosure )
	{
		/*
			Get the right VOPType for volume parameters.
		*/
		const char* tag = nullptr;
		osl_utilities::FindMetaData(tag, i_osl_param.metadata, "input_shader_type");
		if (tag != nullptr && strcmp(tag, "volume") == 0)
			return VOP_ATMOSPHERE_SHADER;

		/*
			Use the type of the shader node. This is what makes the network
			connect to the correct terminal type on the USD side of the
			universe. See getMaterialPrimOutputName() and isShaderVopTypeName()
			in husdshadertranslators/default.py

			Note that VOP_ATMOSPHERE_SHADER is remapped to volume for USD.
			Also, getShaderType() is used if there is no output parameter.
		*/
		return m_shader_info.ShaderType();
	}

	switch(i_osl_param.type.elementtype)
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
			return VOP_TYPE_COLOR;

		default:
			break;
	}

	return VOP_TYPE_STRING;
}

/**
	Initializes a fake parameter so it indicates the beginning and name of a
	sub-page.
*/
static void
InitSubPageMarker(DlShaderInfo::Parameter& o_fake_param, const char* i_name)
{
	assert(i_name);
	o_fake_param.type.elementtype = NSITypeInvalid;
	/*
		DlShaderInfo::conststring's constructor expects its "end" pointer to
		point *after* the string's terminating null character, hence the unusual
		+1 below.
	*/
	o_fake_param.name =
		DlShaderInfo::conststring(i_name, i_name + strlen(i_name) + 1);
}

/// Adds a title for a page's section to io_templates
static void
AddSubPageHeading(
	std::vector<PRM_Template>& io_templates,
	const DlShaderInfo::conststring& i_name)
{
	std::string identifier = i_name.string() + "_heading";
	std::replace(identifier.begin(), identifier.end(), ' ', '_');
	char* label_name = LEAKED(strdup(identifier.c_str()));
	char* label_string = LEAKED(strdup(i_name.c_str()));
	PRM_Name* name = LEAKED(new PRM_Name(label_name, label_string));
	io_templates.push_back(PRM_Template(PRM_HEADING, 0, name));
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
	PRM_Default* defau1t = LEAKED(NewPRMDefault(i_param, num_components));
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
	const DlShaderInfo& i_shader,
	const DlShaderInfo::Parameter& i_param,
	const osl_utilities::ParameterMetaData& i_meta)
{
	using namespace osl_utilities;
	using namespace osl_utilities::ramp;

	const DlShaderInfo::Parameter* knots = nullptr;
	const DlShaderInfo::Parameter* interpolation = nullptr;
	const DlShaderInfo::Parameter* shared_interpolation = nullptr;
	std::string base_name;
	if(!FindMatchingRampParameters(
		i_shader,
		i_param,
		knots,
		interpolation,
		shared_interpolation,
		base_name))
	{
		return;
	}

	// Create the names of the other variables controlled by the ramp widget
	char* pos_string =
		LEAKED(strdup((knots->name.string() + k_index_suffix).c_str()));
	char* value_string =
		LEAKED(strdup((i_param.name.string() + k_index_suffix).c_str()));
	char* inter_string =
		LEAKED(strdup((interpolation->name.string() + k_index_suffix).c_str()));
	bool color = i_param.type.elementtype == NSITypeColor;
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

	// Put the other dirty bits somewhere.
	PRM_SpareData *spare = LEAKED(new PRM_SpareData);
	spare->setMultiStartOffset(0);
	/* These are fetched by the USD translator and used to name the params. */
	spare->setRampKeysVar(knots->name.c_str());
	spare->setRampValuesVar(i_param.name.c_str());
	spare->setRampBasisVar(interpolation->name.c_str());

	// Create the main ramp template
	PRM_MultiType multi_type =
		color ? PRM_MULTITYPE_RAMP_RGB : PRM_MULTITYPE_RAMP_FLT;
	char* name_string = LEAKED(strdup(base_name.c_str()));
	PRM_Name* name = LEAKED(new PRM_Name(name_string, i_meta.m_label));
	io_templates.push_back(
		PRM_Template(
			multi_type,
			&(*nodes)[0],
			1,
			name,
			nullptr,
			nullptr,
			spare));
}


StructuredShaderInfo::StructuredShaderInfo(const DlShaderInfo* i_info)
	:	m_dl(*i_info), m_terminal(false)
{
	assert(i_info);

	// Retrieve the description of the shader's input and output parameters
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

	// Retrieve useful tags from the shader
	const DlShaderInfo::constvector<DlShaderInfo::Parameter>& meta =
		i_info->metadata();
	for(const DlShaderInfo::Parameter& param : meta)
	{
		if(param.name != "tags" || param.type.elementtype != NSITypeString)
		{
			continue;
		}

		for(const DlShaderInfo::conststring& tag : param.sdefault)
		{
			if( tag == "surface" || tag == "environment")
			{
				m_terminal = true;
				m_shader_type = VOP_SURFACE_SHADER;
				break;
			}
			if( tag == "displacement" )
			{
				m_terminal = true;
				m_shader_type = VOP_DISPLACEMENT_SHADER;
				break;
			}
			if( tag == "volume" )
			{
				m_terminal = true;
				m_shader_type = VOP_ATMOSPHERE_SHADER;
				break;
			}
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

		/*
			Since we are showing Closures as Colors on the UI we exclude
			aovGroup from it since on OSL shader aovGroup is defined as a closure.
		*/
		if(param.isclosure && param.name == "aovGroup")
		{
			continue;
		}

		/*
			Don't display matrices in the UI for now. They can still be
			communicated through connections.
		*/
		if(param.type.elementtype == NSITypeMatrix ||
			param.type.elementtype == NSITypeDoubleMatrix)
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
			!osl_utilities::ramp::IsRampWidget(widget))
		{
			continue;
		}

		const char* page_name = nullptr;
		osl_utilities::FindMetaData(page_name, param.metadata, "page");

		if( page_name == nullptr )
		{
			page_name = "Main";
		}

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

	/*
		Some pages are actually sub-pages. We merge them together and insert a
		fake parameter to mark transitions between sections.
	*/
	std::vector<DlShaderInfo::Parameter> fake_shader_parameters(page_list.size());
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

		// Mark the beginning of the section
		DlShaderInfo::Parameter& fake_param = fake_shader_parameters[p];
		InitSubPageMarker(fake_param, period+1);
		page.push_back(&fake_param);

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
			if(param->type.elementtype == NSITypeInvalid)
			{
				AddSubPageHeading(*templates, param->name);
				continue;
			}

			osl_utilities::ParameterMetaData meta;
			meta.m_label = param->name.c_str();
			osl_utilities::GetParameterMetaData(meta, param->metadata);

			if(osl_utilities::ramp::IsRampWidget(meta.m_widget))
			{
				AddRampParameterTemplate(
					*templates, i_shader_info.m_dl, *param, meta);
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
	if(m_shader_info.IsTerminal())
	{
		/*
			Tell Houdini that this shader can be used as a material.  It allows
			it to appear in the "Choose Operator" window that is displayed when
			one clicks on the associated button near an geometry object's
			"Material" parameter field.
			We were told to call this in an override of
			VOP_Node::initMaterialFlag(), but it works just fine here. After
			all, here is the earliest time we can call it.
		*/
		setMaterialFlag(true);
	}
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

void
VOP_ExternalOSL::opChanged(OP_EventType reason, void* data)
{
	VOP_Node::opChanged(reason, data);
	if(reason != OP_NAME_CHANGED)
	{
		return;
	}

	/*
		Execute the script with the node's full path as an argument.
		This callbacks traps nodes loaded from an existing scene (unlike
		runCreateScript), allowing us to add our OpenGL attributes to them.
	*/
	std::string init_cmd = "private/3Delight--VOP_OnCreated.cmd " +
		getFullPath().toStdString();
	OPgetDirector()->getCommandManager()->execute(init_cmd.c_str());
}

bool
VOP_ExternalOSL::runCreateScript()
{
	bool ret = VOP_Node::runCreateScript();

	/*
		Execute the script with the node's full path as an argument.
		This is only called when a node is first created, not when it's loaded
		from a scene.
	*/
	std::string init_cmd = "private/3Delight--VOP_OnCreated.cmd " +
		getFullPath().toStdString();
	OPgetDirector()->getCommandManager()->execute(init_cmd.c_str());

	SetRampParametersDefaults();

	if( m_shader_info.m_dl.shadername() == "vdbVolume" )
	{
		SetVDBVolumeDefaults();
	}

	Insert3DPlacementMatrix();

	CollapseMaterialsInputGroups();

	return ret;
}

bool
VOP_ExternalOSL::updateParmsFlags()
{
	bool changed = OP_Network::updateParmsFlags();

	unsigned nparams = m_shader_info.m_dl.nparams();
	for (unsigned p = 0; p < nparams; p++)
	{
		const DlShaderInfo::Parameter* param = m_shader_info.m_dl.getparam(p);
		assert(param);
		if (param->isclosure && param->name != "aovGroup")
		{
			changed |= enableParm(param->name.c_str(), false);
		}
		else
		{
			//Disable shader's parameter input when connected.
			changed |= enableParm(param->name.c_str(), !getInput(p));
		}
	}
	return changed;
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

/*
	This shader type is used by Houdini to figure out the terminal name in the
	USD universe, for shaders which don't have a specific output closure
	parameter. See GetVOPType() for the case where there is a parameter.
*/
VOP_Type VOP_ExternalOSL::getShaderType() const
{
	return m_shader_info.ShaderType();
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
	o_type_info.setType(GetVOPType(param));
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
	o_type_info.setType(GetVOPType(param));
}

void
VOP_ExternalOSL::SetRampParametersDefaults()
{
	using namespace osl_utilities;
	using namespace osl_utilities::ramp;

	for(unsigned p = 0; p < m_shader_info.NumInputs(); p++)
	{
		const DlShaderInfo::Parameter& param = m_shader_info.GetInput(p);
		const char* widget = "";
		FindMetaData(widget, param.metadata, "widget");
		if(!IsRampWidget(widget))
		{
			continue;
		}

		const DlShaderInfo::Parameter* knots = nullptr;
		const DlShaderInfo::Parameter* interpolation = nullptr;
		const DlShaderInfo::Parameter* shared_interpolation = nullptr;
		std::string base_name;
		if(!FindMatchingRampParameters(
				m_shader_info.m_dl,
				param,
				knots,
				interpolation,
				shared_interpolation,
				base_name))
		{
			continue;
		}

		// Retrieve the default parameters values
		const DlShaderInfo::constvector<float>& default_values = param.fdefault;
		bool color = param.type.elementtype == NSITypeColor;
		unsigned value_size = color ? 3 : 1;
		unsigned nb_default_values = default_values.size() / value_size;
		const DlShaderInfo::constvector<float>& default_knots = knots->fdefault;

		// The interpolation mode could be expressed in 2 different ways
		std::vector<int> default_inter;
		if(shared_interpolation && shared_interpolation->sdefault.size() == 1 &&
			interpolation->idefault.size() == 1 && interpolation->idefault[0] == -1)
		{
			int shared_inter =
				ToHoudiniInterpolation(shared_interpolation->sdefault[0].string());
			default_inter.insert(
				default_inter.end(),
				nb_default_values,
				shared_inter);
		}
		else
		{
			for(int i : interpolation->idefault)
			{
				default_inter.push_back(ToHoudiniInterpolation(i));
			}
		}

		unsigned nb_defaults =
			std::min(
				nb_default_values,
				std::min<unsigned>(default_knots.size(), default_inter.size()));

		if(nb_defaults == 0)
		{
			continue;
		}

		// Set the number of points in the curve
		setInt(base_name.c_str(), 0, 0.0, nb_defaults);

		// Set the parameters for each point on the curve
		std::string pos_string = knots->name.string();
		std::string value_string = param.name.string();
		std::string inter_string = interpolation->name.string();
		for(unsigned d = 0; d < nb_defaults; d++)
		{
			std::string index = ExpandedIndexSuffix(d);

			std::string pos_item = pos_string + index;
			setFloat(pos_item.c_str(), 0, 0.0, default_knots[d]);

			const float* v = &default_values[d*value_size];
			std::string value_item = value_string + index;
			for(unsigned c = 0; c < value_size; c++)
			{
				setFloat(value_item.c_str(), c, 0.0, v[c]);
			}

			std::string inter_item = inter_string + index;
			setInt(inter_item.c_str(), 0, 0.0, default_inter[d]);
		}
	}
}

/*
	Set the default values for the volume grids parameters which are added to
	the shader but will be exported on the volume node. They are set instead of
	being defined as default values because they are not the actual defaults of
	NSI. Velocity scale is a proper default in GetVolumeParams() because it is
	the same as the NSI default.
*/
void VOP_ExternalOSL::SetVDBVolumeDefaults()
{
	using namespace VolumeGridParameters;
	setString(density_default, CH_STRING_LITERAL, density_name, 0, 0.f);
	setString(color_default, CH_STRING_LITERAL, color_name, 0, 0.f);
	setString(temperature_default, CH_STRING_LITERAL, temperature_name, 0, 0.f);
	setString(emission_default, CH_STRING_LITERAL, emission_name, 0, 0.f);
	setString(velocity_default, CH_STRING_LITERAL, velocity_name, 0, 0.f);
}

void VOP_ExternalOSL::Insert3DPlacementMatrix()
{
	for (const DlShaderInfo::Parameter& param : m_shader_info.m_dl.metadata())
	{
		if (param.name != "tags" || param.type.elementtype != NSITypeString)
		{
			continue;
		}

		for (const DlShaderInfo::conststring& tag : param.sdefault)
		{
			/*
				Connect our placement matrix node with all the 3D Textures
				upon their creation.
			*/
			if (tag == "texture/3d")
			{
				int idx = m_shader_info.NumInputs() - 1;
				const char* placementMatrix = "makexform";
				insertNode(idx, placementMatrix, true);
			}
		}
	}
}


/*
	Collapse input groups of materials. Because some nodes have many parameters,
	this would result in a very long node network which would take up too much
	screen. To solve this we are collapsing the input groups of the nodes except
	the main group (first one) for nodes having "surface" tag.
*/
void VOP_ExternalOSL::CollapseMaterialsInputGroups()
{
	/*
		Insert all input groups ("pages") in a set so we can collapse them later.
		we are using set so we can insert a page only once (have page name unique).
		We also find the first page seperately because if we insert it in the
		unordered set, there is no guarantee that the page  we are skipping is
		the first page or not.
	*/
	const char* first_page = nullptr;
	osl_utilities::FindMetaData(first_page, m_shader_info.GetInput(0).metadata, "page");
	if (first_page == nullptr)
		first_page = "Main";

	std::unordered_set <std::string> pages;
	for (unsigned p = 1; p < m_shader_info.NumInputs(); p++)
	{
		const DlShaderInfo::Parameter& param = m_shader_info.GetInput(p);
		const char* page = nullptr;
		osl_utilities::FindMetaData(page, param.metadata, "page");
		if (page == nullptr)
			page = "Main";

		pages.insert(page);
	}

	/*
		We ignore the first input group for all materials
		with surface tag and collapse the rest of the groups.
	*/
	for (auto page_it = pages.begin(); page_it != pages.end(); page_it++)
	{
		if(*page_it != first_page || !m_shader_info.IsTerminal())
		VOP_ExternalOSL::setInputGroupExpanded(*page_it, false);
	}
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
			// Put nsi here so Houdini's Material Builder won't see our VOPs
			"nsi",
			nullptr,
			/*
				FIXME : this might be useless. We probably meant to set the
				material flag on the node, instead.
			*/
			i_shader_info.IsTerminal() ? OP_FLAG_OUTPUT : 0u,
			i_shader_info.NumOutputs()),
		m_shader_info(i_shader_info)
{
	const char* name = i_shader_info.m_dl.shadername().c_str();
	osl_utilities::FindMetaData(name, i_shader_info.m_dl.metadata(), "niceName");

	std::string better;
	if( ::islower(name[0]))
	{
		/*
			Many utility nodes have no nice name but their ID is just fine with
			an upper case. For example: leather, granite, etc.
		*/
		better = name;
		better[0] = std::toupper(better[0]);
		name = better.c_str();
	}
	setEnglish(name);

	// Set default icon name for those that are not already defined by
	// VOP_3Delight-xxx in ui
	const DlShaderInfo::conststring& shadername =
		i_shader_info.m_dl.shadername();
	if (shadername != "dlColorToFloat" &&
		shadername != "dlFloatToColor" &&
		shadername != "dlGlass" &&
		shadername != "dlHairAndFur" &&
		shadername != "dlMetal" &&
		shadername != "dlPrincipled" &&
		shadername != "dlRamp" &&
		shadername != "dlSkin" &&
		shadername != "dlSubstance" &&
		shadername != "dlThin" &&
		shadername != "dlToon" &&
		shadername != "vdbVolume")
	{
		setIconName("ROP_3Delight");
	}

	setOpTabSubMenuPath(i_menu_name.c_str());

	VOP_OperatorInfo* vop_info =
		static_cast<VOP_OperatorInfo*>(getOpSpecificData());

	/*
		The RenderMask is what ends up being the MaterialNetworkSelector in
		Hydra. If we don't set it, the default translator will not provide
		networks at all. And if it does not match the Hydra plugin, we won't
		see the networks there.
	*/
	vop_info->setRenderMask("nsi");
}

/*
	Setting a custom help URL by overriding the virtual GetOPHelpURL
	function of OP_Operator class.
*/
bool VOP_ExternalOSLOperator::getOpHelpURL(UT_String &url)
{
	const char* name = m_shader_info.m_dl.shadername().c_str();
	osl_utilities::FindMetaData(name, m_shader_info.m_dl.metadata(), "niceName");

	std::string shader_ui_name = name;
	if (shader_ui_name == "vdbVolume")
		shader_ui_name = "Open+VDB";

	//Replacing " " with "+" so it adapts to 3Delight URL.
	std::replace(shader_ui_name.begin(), shader_ui_name.end(), ' ', '+');
	std::string url_name = dl_system::delight_doc_url() + shader_ui_name;
	url.hardenIfNeeded(url_name.c_str());
	return true;
}
