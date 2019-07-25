#include <UI/UI_Value.h>
#include <SI/AP_Interface.h>

#include <string>
#include <vector>

class PRM_Parm;
class ROP_Node;

/**
	This dialog appears when the user clicks "Add" in the image
	Layers tab.
*/
class SelectLayersDialog: public AP_Interface
{
public:
	SelectLayersDialog();
	/// Launch the UI dialog
	bool open(ROP_Node* io_node);
	/// Close the UI dialog
	void close();

private:
	/// Parses the supplied ui file
	bool parseDialog();
	/// Callback for OK button
	void handleOK(UI_Event* i_event);
	void updateToggle(int index, const std::string& i_aov_name);
	void updateValue(UI_Value& o_value);
	void addMultiParmItem(PRM_Parm& io_parm, const std::string& i_label);
	void removeMultiParmItem(PRM_Parm& io_parm, const std::string& i_label);

	/// Bound to value for dialog open/close
	UI_Value m_openValue;

	/// Flag to keep track if we've been parsed
	bool m_parsedDialog;

	/// Keep track if we're open
	bool m_isOpen;

	std::vector<UI_Value*> m_values;
	std::vector<std::string> m_labels;
	std::vector<std::string> m_symbols;
	std::vector<bool> m_foundItems;

	ROP_Node* m_node;
};
