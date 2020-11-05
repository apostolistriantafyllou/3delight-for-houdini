#pragma once

#include <VOP/VOP_Node.h>

struct dlAOVGroupOperator;

class dlAOVGroup : public VOP_Node
{
public:

    /// Registers the dlAOVGroup
    static void Register(OP_OperatorTable* table);

    /// Creates an instance of dlAOVGroup to a network.
    static OP_Node* alloc(OP_Network* net, const char* name, OP_Operator* entry);

    /// Returns the templates for the shader's parameters
    static PRM_Template myTemplateList[];

    /* 
        Disable our parameters based on which inputs are connected. 
        For now we are doing nothing but if we want to use color 
        values as well for the AOVs, we might disable them on 
        connectiong its corresponding node.
    */
    bool updateParmsFlags() override;
      
    /// Provides the labels to appear on input and output buttons.
    const char* inputLabel(unsigned idx) const override;
    const char* outputLabel(unsigned idx) const override;

    /// Returns the number input ports that should be visible
    unsigned getNumVisibleInputs() const override;
    virtual void opChanged(OP_EventType reason, void* data) override;

protected:

    ///Constructor
    dlAOVGroup(OP_Network* net, const char* name, dlAOVGroupOperator* entry);
    ~dlAOVGroup() override;

    /// Returns the internal name of the current input (index value = idx).
    void getInputNameSubclass(UT_String& in, int idx) const override;

    /// Reverse mapping of internal input names to an input index.
    int getInputFromNameSubclass(const UT_String& in) const override;

    /// Fills in the info about the vop type connected to the idx-th input.
    void getInputTypeInfoSubclass(VOP_TypeInfo& type_info,
        int idx) override;

    /// Returns the internal name of the supplied output.
    void getOutputNameSubclass(UT_String& out, int idx) const override;

    /// Determines the data type of the output connector.
    void getOutputTypeInfoSubclass(VOP_TypeInfo& type_info,
        int idx) override;

    /// Determines which vop types are allowed to be connected
    /// to this node at the input with index idx
    void getAllowedInputTypeInfosSubclass(unsigned idx,
        VOP_VopTypeInfoArray& type_infos) override;
};

struct dlAOVGroupOperator : public VOP_Operator
{
    /// Constructor.
    dlAOVGroupOperator();
};
