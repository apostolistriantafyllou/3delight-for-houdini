#include <OP/OP_OperatorTable.h>
#include <PRM/PRM_SpareData.h>
#include <OP/OP_Input.h>
#include <VOP/VOP_Operator.h>
#include "VOP_AOVGroup.h"

void
VOP_AOVGroup::Register(OP_OperatorTable* table)
{
    table->addOperator(new VOP_AOVGroupOperator());
}

OP_Node*
VOP_AOVGroup::alloc(OP_Network* net, const char* name, OP_Operator* entry)
{
    VOP_AOVGroupOperator* aov_entry =
        dynamic_cast<VOP_AOVGroupOperator*>(entry);
    assert(aov_entry);
    return new VOP_AOVGroup(net, name, aov_entry);
}

static PRM_Name AOVInputs("aovs", "AOVs");
static PRM_Name AOVInputName("input_aov#", "AOV Name #");
static PRM_Default AOVInputDefault(0, "aov#");

static std::vector<PRM_Template>
vopPlugInpTemplate =
{
    PRM_Template(PRM_ALPHASTRING, 1, &AOVInputName, &AOVInputDefault),
    PRM_Template()
};

static PRM_Name difuse_visibility("visible_in_diffuse", "Visible in Diffuse");
static PRM_Default difuse_visibility_d(true);
static PRM_Name reflection_visibility("visible_in_reflections", "Visible in Reflection");
static PRM_Default reflection_visibility_d(true);
static PRM_Name refraction_visibility("visible_in_refractions", "Visible in Refraction");
static PRM_Default refraction_visibility_d(true);

static std::vector<PRM_Template>
visibilityTab =
{
    PRM_Template(),
    PRM_Template(PRM_TOGGLE, 1, &difuse_visibility, &difuse_visibility_d),
    PRM_Template(PRM_TOGGLE, 1, &reflection_visibility, &reflection_visibility_d),
    PRM_Template(PRM_TOGGLE, 1, &refraction_visibility, &refraction_visibility_d),
};

static PRM_Name tabs_name("tabs");
static std::vector<PRM_Default> main_tabs =
{
    PRM_Default(vopPlugInpTemplate.size()-1, "Main"),
    PRM_Default(3, "Visibility"),
};

PRM_Template VOP_AOVGroup::myTemplateList[] =
{
    PRM_Template(PRM_SWITCHER, main_tabs.size(), &tabs_name, &main_tabs[0]),
    PRM_Template(PRM_MULTITYPE_LIST, &vopPlugInpTemplate[0], 0, &AOVInputs,
                 0, 0, &PRM_SpareData::multiStartOffsetZero),
    PRM_Template(PRM_TOGGLE, 1, &difuse_visibility, &difuse_visibility_d),
    PRM_Template(PRM_TOGGLE, 1, &reflection_visibility, &reflection_visibility_d),
    PRM_Template(PRM_TOGGLE, 1, &refraction_visibility, &refraction_visibility_d),
    PRM_Template()
};
VOP_AOVGroup::VOP_AOVGroup(OP_Network* parent, const char* name, VOP_AOVGroupOperator* entry)
    : VOP_Node(parent, name, entry)
{
}
VOP_AOVGroup::~VOP_AOVGroup()
{
}
bool
VOP_AOVGroup::updateParmsFlags()
{

    return true;
}

const char*
VOP_AOVGroup::inputLabel(unsigned index) const
{
    static UT_WorkBuffer theLabel;
    fpreal t = CHgetEvalTime();
    int i = index;
    UT_String label;
    UT_WorkBuffer input_parameter;
    input_parameter.sprintf("input_aov%d", index);
    if (hasParm(input_parameter.buffer()))
    {
        evalStringInst(AOVInputName.getToken(), &i, label, 0, t);
        if (label.isstring())
            theLabel.strcpy(label);
        else
            theLabel.strcpy("<unnamed>");
    }
    return theLabel.buffer();
}
const char*
VOP_AOVGroup::outputLabel(unsigned index) const
{
    assert(index == 0);
    return "Out Color";
}
void
VOP_AOVGroup::getInputNameSubclass(UT_String& name, int index) const
{
    name.harden(inputLabel(index));
}
int
VOP_AOVGroup::getInputFromNameSubclass(const UT_String& name) const
{    
    // Use the numeric suffix on the input name to determine the input index.
    for (int i = 0; i < getNumVisibleInputs(); i++)
    {
        if (name == inputLabel(i))
            return i;
    }
    return -1;
}
void
VOP_AOVGroup::getInputTypeInfoSubclass(VOP_TypeInfo& type_info, int idx)
{
    type_info.setType(VOP_TYPE_COLOR);
}
void
VOP_AOVGroup::getAllowedInputTypeInfosSubclass(unsigned idx,
    VOP_VopTypeInfoArray& type_infos)
{
    //Allow only Color data types as input Connections
    type_infos.append(VOP_TypeInfo(VOP_TYPE_COLOR));
}
void
VOP_AOVGroup::getOutputNameSubclass(UT_String& name, int idx) const
{
    name.harden(outputLabel(idx));
}
void
VOP_AOVGroup::getOutputTypeInfoSubclass(VOP_TypeInfo& type_info, int idx)
{
    //Set Output connection as a surface shader to connect it to the 
    //aov group of 3delight surfaces.
    type_info.setType(VOP_SURFACE_SHADER);
}
unsigned
VOP_AOVGroup::getNumVisibleInputs() const
{
    return evalInt("aovs", 0, CHgetEvalTime());
}

void VOP_AOVGroup::ReplaceDuplicateLabel(std::vector<std::string> inputs, std::string updated_label, int index)
{
    /*
        If the label already exist we check if the existing one has any number
        in the end or no, so we only take the first part without the numbers.
    */
    if (std::find(inputs.begin(), inputs.end(), updated_label) != inputs.end())
    {
        /*
            Search the string for the last character that does not match any
            of the characters specified in its arguments.
        */
        size_t last_index = updated_label.find_last_not_of("0123456789");
        updated_label = updated_label.substr(0, last_index + 1) == "" ? "aov" : updated_label.substr(0, last_index + 1);
        /*
            Check for possible names of our new parameter by adding a number
            in the end of it.
        */
        int input_number = getNumVisibleInputs();
        for (int i = 0; i < input_number; i++)
        {
            std::string new_name = updated_label + std::to_string(i);
            if (std::find(inputs.begin(), inputs.end(), new_name) == inputs.end())
            {
                setStringInst(new_name, CH_STRING_LITERAL, AOVInputName.getToken(), &index, 0, 0);
                break;
            }
        }
    }
}

/*
    Updates label naming by not allowing an empty or duplicate label name.
*/
void VOP_AOVGroup::UpdateLabelNaming(void *data)
{
    std::vector<std::string> inputs;
    int parm_index = reinterpret_cast<intptr_t>(data) - 1;
    UT_String changed_label;
    UT_WorkBuffer input_parameter;

    /*
        check if the parameter exists. This will fix error messages about
        evaluating non existant parameters upon their creation.
    */
    input_parameter.sprintf("input_aov%d", parm_index);
    if (hasParm(input_parameter.buffer()))
    {
        evalStringInst(AOVInputName.getToken(), &parm_index, changed_label, 0, 0);

        /*
            Insert all the inputs label name on a vector except the one that
            we already changed and check if the parameter we changed exist on
            the vector or not.
        */
        int inputNumber = getNumVisibleInputs();
        for (int i = 0; i < inputNumber; i++)
        {
            UT_String label;
            evalStringInst(AOVInputName.getToken(), &i, label, 0, 0);
            if (i == parm_index)
                inputs.push_back("");
            else
                inputs.push_back(label.toStdString());
        }
        ReplaceDuplicateLabel(inputs, changed_label.toStdString(), parm_index);
    }
}

void
VOP_AOVGroup::opChanged(OP_EventType reason, void* data)
{
    VOP_Node::opChanged(reason, data);
    if (reason != OP_PARM_CHANGED)
    {
        return;
    }
    UpdateLabelNaming(data);
}

VOP_AOVGroupOperator::VOP_AOVGroupOperator()
    :   VOP_Operator(
            "dlAOVGroup",
            "AOV Group",
            VOP_AOVGroup::alloc,
            VOP_AOVGroup::myTemplateList,
            VOP_AOVGroup::theChildTableName,
            0,
            VOP_VARIABLE_INOUT_MAX,
            "nsi"
        )
{
    setIconName("ROP_3Delight");
    setOpTabSubMenuPath("3Delight");
}