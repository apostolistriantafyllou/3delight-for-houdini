#include "OBJ_IncandescenceLight.h"
#include "dl_system.h"

#include <OP/OP_OperatorTable.h>
#include <PRM/PRM_SpareData.h>

void
OBJ_IncandescenceLight::Register(OP_OperatorTable* io_table)
{
	io_table->addOperator(new OP_IncandescenceLightOperator());
}

OP_Node*
OBJ_IncandescenceLight::alloc(OP_Network* net, const char* name, OP_Operator* entry)
{
	OP_IncandescenceLightOperator* op_entry =
		dynamic_cast<OP_IncandescenceLightOperator*>(entry);
	assert(op_entry);
    return new OBJ_IncandescenceLight(net, name, op_entry);
}

static PRM_Name separator("separator", "");

static PRM_Name color("light_color", "Color");
static PRM_Default color_d[3] = {1.0f, 1.0f, 1.0f};

static PRM_Name childcomp("childcomp", "ChildComp");
static PRM_Default childcomp_d(0);

static PRM_Name intensity("light_intensity", "Intensity");
static PRM_Default intensity_d(1.0f);
static PRM_Range intensity_r(PRM_RANGE_RESTRICTED, 0.0f, PRM_RANGE_RESTRICTED, 10.0f);

static PRM_Name exposure("exposure", "Exposure");
static PRM_Default exposure_d(0.0f);
static PRM_Range exposure_r(PRM_RANGE_RESTRICTED, -5.0f, PRM_RANGE_RESTRICTED, 10.0f);

static PRM_Name object_selection("object_selection", "Object selection");

PRM_Template
OBJ_IncandescenceLight::templateList[] =
{
	PRM_Template(PRM_RGB, 3, &color, color_d),
	PRM_Template(PRM_FLT, 1, &intensity, &intensity_d, nullptr, &intensity_r),
	PRM_Template(PRM_FLT, 1, &exposure, &exposure_d, nullptr, &exposure_r),
	PRM_Template(PRM_SEPARATOR, 0, &separator),
	PRM_Template(PRM_STRING_OPLIST, PRM_TYPE_DYNAMIC_PATH_LIST, 1, &object_selection, nullptr, nullptr, nullptr, nullptr, &PRM_SpareData::objGeometryPath),
    PRM_Template()      // List terminator
};

OP_TemplatePair*
OBJ_IncandescenceLight::buildTemplatePair(OP_TemplatePair *prevstuff)
{
    OP_TemplatePair     *incandescence, *geo;
    // Here, we have to "inherit" template pairs from geometry and beyond. To
    // do this, we first need to instantiate our template list, then add the
    // base class templates.
    incandescence = new OP_TemplatePair(OBJ_IncandescenceLight::templateList, prevstuff);
    geo = new OP_TemplatePair(OBJ_Geometry::getTemplateList(OBJ_PARMS_PLAIN),
                              incandescence);
    return geo;
}

unsigned
OBJ_IncandescenceLight::minInputs() const
{
	return 0;
}

unsigned
OBJ_IncandescenceLight::getNumVisibleInputs() const
{
	return 0;
}

OBJ_IncandescenceLight::OBJ_IncandescenceLight(
	OP_Network* parent, const char* name, OP_IncandescenceLightOperator* entry)
    :	OBJ_Geometry(parent, name, entry)
{
}

OBJ_IncandescenceLight::~OBJ_IncandescenceLight()
{
}

OP_IncandescenceLightOperator::OP_IncandescenceLightOperator()
	:	OP_Operator(
			"3Delight::IncandescenceLight",
			"Incandescence Light",
			OBJ_IncandescenceLight::alloc,
			OBJ_IncandescenceLight::buildTemplatePair(0),
			OBJ_IncandescenceLight::theChildTableName,
			0, 0)
{
	setIconName("OBJ_3Delight-dlIncandescenceLight");
	setOpTabSubMenuPath("3Delight");
}

bool OP_IncandescenceLightOperator::getOpHelpURL(UT_String& url)
{
	std::string url_name = dl_system::delight_doc_url() + "Incandescence+Light";
	url.harden(url_name.c_str());
	return true;
}
