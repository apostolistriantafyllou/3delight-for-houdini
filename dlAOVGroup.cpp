#include <OP/OP_OperatorTable.h>
#include <PRM/PRM_SpareData.h>
#include <OP/OP_Input.h>
#include <VOP/VOP_Operator.h>
#include "dlAOVGroup.h"

void
dlAOVGroup::Register(OP_OperatorTable* table)
{
    table->addOperator(new dlAOVGroupOperator());
}

OP_Node*
dlAOVGroup::alloc(OP_Network* net, const char* name, OP_Operator* entry)
{
    dlAOVGroupOperator* aov_entry =
        dynamic_cast<dlAOVGroupOperator*>(entry);
    assert(aov_entry);
    return new dlAOVGroup(net, name, aov_entry);
}

static PRM_Name AOVInputs("aovs", "AOVs");
static PRM_Name AOVInputName("input_aov#", "AOV Name #");
static PRM_Default AOVInputDefault(0, "aov#");
static PRM_Template
vopPlugInpTemplate[] =
{
    PRM_Template(PRM_ALPHASTRING, 1, &AOVInputName, &AOVInputDefault),
    PRM_Template()
};

PRM_Template dlAOVGroup::myTemplateList[] =
{
    PRM_Template(PRM_MULTITYPE_LIST, vopPlugInpTemplate, 0, &AOVInputs,
                 0, 0, &PRM_SpareData::multiStartOffsetZero),
    PRM_Template()
};
dlAOVGroup::dlAOVGroup(OP_Network* parent, const char* name, dlAOVGroupOperator* entry)
    : VOP_Node(parent, name, entry)
{
}
dlAOVGroup::~dlAOVGroup()
{
}
bool
dlAOVGroup::updateParmsFlags()
{

    return true;
}

const char*
dlAOVGroup::inputLabel(unsigned index) const
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
dlAOVGroup::outputLabel(unsigned index) const
{
    assert(index == 0);
    return "Out Color";
}
void
dlAOVGroup::getInputNameSubclass(UT_String& name, int index) const
{
    name.harden(inputLabel(index));
}
int
dlAOVGroup::getInputFromNameSubclass(const UT_String& name) const
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
dlAOVGroup::getInputTypeInfoSubclass(VOP_TypeInfo& type_info, int idx)
{
    type_info.setType(VOP_TYPE_COLOR);
}
void
dlAOVGroup::getAllowedInputTypeInfosSubclass(unsigned idx,
    VOP_VopTypeInfoArray& type_infos)
{
    //Allow only Color data types as input Connections
    type_infos.append(VOP_TypeInfo(VOP_TYPE_COLOR));
}
void
dlAOVGroup::getOutputNameSubclass(UT_String& name, int idx) const
{
    name.harden(outputLabel(idx));
}
void
dlAOVGroup::getOutputTypeInfoSubclass(VOP_TypeInfo& type_info, int idx)
{
    //Set Output connection as a surface shader to connect it to the 
    //aov group of 3delight surfaces.
    type_info.setType(VOP_SURFACE_SHADER);
}
unsigned
dlAOVGroup::getNumVisibleInputs() const
{
    return evalInt("aovs", 0, CHgetEvalTime());
}

/*
    Updates label naming by not allowing an empty or duplicate label name.
*/
void dlAOVGroup::updateLabelNaming(void *data)
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
            Do not allow a parameter's value to be empty.
        */
        if (!changed_label.isstring())
        {
            UT_WorkBuffer label_value;
            label_value.sprintf("aov%d", parm_index);
            setStringInst(label_value.buffer(), CH_STRING_LITERAL, AOVInputName.getToken(), &parm_index, 0, 0);
        }

        /*
            Insert all the inputs label name on a vector except the one that
            we already changed and check if the parameter we changed exist on
            the vector or not.
        */
        for (int i = 0; i < getNumVisibleInputs(); i++)
        {
            UT_String label;
            evalStringInst(AOVInputName.getToken(), &i, label, 0, 0);
            if (i == parm_index)
                inputs.push_back("");
            else
                inputs.push_back(label.toStdString());
        }

        /*
            If the label already exist we check if the existing one has any number
            in the end or no, so we only take the first part without the numbers.
        */
        if (std::find(inputs.begin(), inputs.end(), changed_label.toStdString()) != inputs.end())
        {
            int number = 0;
            std::string str = changed_label.toStdString();
            /*
                Search the string for the last character that does not match any
                of the characters specified in its arguments.
            */
            size_t last_index = str.find_last_not_of("0123456789");
            std::string result = str.substr(0, last_index + 1);

            /*
                Check for possible names of our new parameter by adding a number
                in the end of it.
            */
            while (true)
            {
                std::string new_name = result + std::to_string(number++);
                if (std::find(inputs.begin(), inputs.end(), new_name) == inputs.end())
                {
                    setStringInst(new_name, CH_STRING_LITERAL, AOVInputName.getToken(), &parm_index, 0, 0);
                    break;
                }
            }
        }
        else
        {
            /*
                if the parameter is unique we place it on our vector
                in the corresponing position.
            */
            inputs[parm_index] = changed_label.toStdString();
        }
    }
}

void
dlAOVGroup::opChanged(OP_EventType reason, void* data)
{
    VOP_Node::opChanged(reason, data);
    if (reason != OP_PARM_CHANGED)
    {
        return;
    }
    updateLabelNaming(data);
}

dlAOVGroupOperator::dlAOVGroupOperator()
    :   VOP_Operator(
            "dlAOVGroup",
            "Aov Group",
            dlAOVGroup::alloc,
            dlAOVGroup::myTemplateList,
            dlAOVGroup::theChildTableName,
            0,
            VOP_VARIABLE_INOUT_MAX,
            "nsi"
        )
{
    setIconName("ROP_3Delight");
    setOpTabSubMenuPath("3Delight");
}