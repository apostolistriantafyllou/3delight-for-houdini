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

static PRM_Name     vopPlugInputs("aovs", "AOVs");
static PRM_Name     vopPlugInpName("inpplug#", "AOV Name #");
static PRM_Default  vopPlugInpDefault(0, "aov#");
static PRM_Template
vopPlugInpTemplate[] =
{
    PRM_Template(PRM_ALPHASTRING, 1, &vopPlugInpName, &vopPlugInpDefault),
    PRM_Template()
};

PRM_Template dlAOVGroup::myTemplateList[] =
{
    PRM_Template(PRM_MULTITYPE_LIST, vopPlugInpTemplate, 0, &vopPlugInputs,
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

    // Evaluate our label from the corresponding parameter.
    evalStringInst(vopPlugInpName.getToken(), &i, label, 0, t);
    if (label.isstring())
        theLabel.strcpy(label);
    else
        theLabel.strcpy("<unnamed>");
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