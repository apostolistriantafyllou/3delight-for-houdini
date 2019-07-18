#include <UI/UI_Value.h>
#include <SI/AP_Interface.h>

#include <string>
#include <vector>

class PRM_Parm;
class ROP_3Delight;

class SelectLayersDialog: public AP_Interface
{
public:
	SelectLayersDialog();
	// Launch the UI dialog
	bool open(ROP_3Delight* io_node);
	// Close the UI dialog
	void close();

private:
	// Parses the supplied ui file
	bool parseDialog();
	// Callback for OK button
	void handleOK(UI_Event* i_event);
	void updateToggle(const std::string& i_aov_name);
	void updateValue(UI_Value& o_value);
	void addMultiParmItem(PRM_Parm& io_parm, const std::string& i_label);
	void removeMultiParmItem(
		PRM_Parm& io_parm, const std::string& i_label, unsigned i_nbItems);

	UI_Value m_openValue;    // Bound to value for dialog open/close
	bool m_parsedDialog;     // Flag to keep track if we've been parsed
	bool m_isOpen;           // Keep track if we're open
	std::vector<UI_Value*> m_values;
	std::vector<std::string> m_labels;
	std::vector<std::string> m_symbols;
	std::vector<unsigned> m_nbItems;
	ROP_3Delight* m_node;
};
