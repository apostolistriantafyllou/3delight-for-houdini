#include "select_layers_dialog.h"
#include <ROP/ROP_Node.h>

#include <stdio.h>
// Used to wrap around callback functions responding to UI_Value changes
#define MYDIALOG_CB(method)     ((UI_EventMethod)&SelectLayersDialog::method)

static const char* k_aov = "aov";

SelectLayersDialog::SelectLayersDialog()
	: m_parsedDialog(false)
	, m_isOpen(false)
{
	m_values.push_back(new UI_Value()); // ci.val
	m_values.push_back(new UI_Value()); // diffuse.val
	m_values.push_back(new UI_Value()); // subsurface.val
	m_values.push_back(new UI_Value()); // reflection.val
	m_values.push_back(new UI_Value()); // refraction.val
	m_values.push_back(new UI_Value()); // volume.val
	m_values.push_back(new UI_Value()); // incandescence.val
	m_values.push_back(new UI_Value()); // zdepth.val
	m_values.push_back(new UI_Value()); // cam_space_position.val
	m_values.push_back(new UI_Value()); // cam_space_normal.val
	m_values.push_back(new UI_Value()); // uv.val
	m_values.push_back(new UI_Value()); // geometry.val

	m_labels.push_back("Ci");
	m_labels.push_back("Diffuse");
	m_labels.push_back("Subsurface scattering");
	m_labels.push_back("Reflection");
	m_labels.push_back("Refraction");
	m_labels.push_back("Volume scattering");
	m_labels.push_back("Incandescence");
	m_labels.push_back("Z (depth)");
	m_labels.push_back("Camera space position");
	m_labels.push_back("Camera space normal");
	m_labels.push_back("UV");
	m_labels.push_back("Geometry ID");

	m_symbols.push_back("ci.val");
	m_symbols.push_back("diffuse.val");
	m_symbols.push_back("subsurface.val");
	m_symbols.push_back("reflection.val");
	m_symbols.push_back("refraction.val");
	m_symbols.push_back("volume.val");
	m_symbols.push_back("incandescence.val");
	m_symbols.push_back("zdepth.val");
	m_symbols.push_back("cam_space_position.val");
	m_symbols.push_back("cam_space_normal.val");
	m_symbols.push_back("uv.val");
	m_symbols.push_back("geometry.val");

	assert(m_values.size() == m_labels.size());
	assert(m_labels.size() == m_symbols.size());
}

bool
SelectLayersDialog::parseDialog()
{
	// These bind the named UI values with the given objects.
	// It's not strictly necessary but is more readable than using
	// getValueSymbol() after calling parseUI().
	setValueSymbol("dlg.val", &m_openValue);
	for (unsigned i = 0; i < m_symbols.size(); i++)
	{
		setValueSymbol(m_symbols[i].c_str(), m_values[i]);
	}
	// The search path used will be defined by HOUDINI_UI_APP_PATH environment
	// variable. The default is $HFS/houdini/config/Applications
	if (!readUIFile("select_layers_ui.ui"))
		return false;   // error parsing
	// We can also directly add interest on an unbound but named UI_Value
	getValueSymbol("ok.val")->addInterest(this, MYDIALOG_CB(handleOK));
	return true;
}

bool
SelectLayersDialog::open(ROP_Node* io_node)
{
	// Only parse the dialog once
	if (!m_parsedDialog)
	{
		if (!parseDialog())
			return false;
		m_parsedDialog = true;
	}
	for (unsigned i = 0; i < m_labels.size(); i++)
	{
		m_nbItems.push_back(0);
	}
	m_node = io_node;

	int nb_aovs = m_node->evalInt(k_aov, 0, 0.0f);
	std::string aov_prefix = "aov_name_";

	for (int i = 0; i < nb_aovs; i++)
	{
		char suffix[12] = "";
		::sprintf(suffix, "%d", i+1);
		std::string aov_name = aov_prefix + suffix;
		updateToggle(aov_name);
	}
	m_openValue = true;
	m_openValue.changed(this);      // notify window to open
	return true;
}

void
SelectLayersDialog::close()
{
	for (unsigned i = 0; i < m_values.size(); i++)
	{
		*m_values[i] = false;
		m_values[i]->changed(this);
	}
	m_nbItems.clear();
	m_openValue = false;
	m_openValue.changed(this);      // notify window to close
}

void
SelectLayersDialog::handleOK(UI_Event* event)
{
	PRM_Parm& parm = m_node->getParm(k_aov);
	for (unsigned i = 0; i < m_symbols.size(); i++)
	{
		bool value = (bool)*getValueSymbol(m_symbols[i].c_str());
		if (!value && m_nbItems[i] > 0)
		{
			removeMultiParmItem(parm, m_labels[i], m_nbItems[i]);
		}
	}
	for (unsigned i = 0; i < m_symbols.size(); i++)
	{
		bool value = (bool)*getValueSymbol(m_symbols[i].c_str());
		if (value && m_nbItems[i] == 0)
		{
			addMultiParmItem(parm, m_labels[i]);
		}
	}
	int nb_aovs = m_node->evalInt(k_aov, 0, 0.0f);
	if (nb_aovs == 0)
	{
		// Don't accept empty list, add Ci by default
		addMultiParmItem(parm, m_labels[0]);
	}
	close();
}

void
SelectLayersDialog::updateToggle(const std::string& i_aov_name)
{
	const PRM_Parm& parm = m_node->getParm(i_aov_name.c_str());
	std::string label = parm.getLabel();
	for (unsigned i = 0; i < m_labels.size(); i++)
	{
		if (label == m_labels[i])
		{
			updateValue(*m_values[i]);
			m_nbItems[i]++;
			break;
		}
	}
}

void
SelectLayersDialog::updateValue(UI_Value& o_value)
{
	o_value = true;
	o_value.changed(this);
}

void
SelectLayersDialog::addMultiParmItem(
	PRM_Parm& io_parm, const std::string& i_label)
{
	io_parm.insertMultiParmItem(io_parm.getMultiParmNumItems());

	int lastIndex = io_parm.getMultiParmNumItems() - 1;
	PRM_Parm* lastParm = io_parm.getMultiParm(lastIndex);
	lastParm->setValue(0.0, 0);

	PRM_Template* lastTemp = io_parm.getMultiParmTemplate(lastIndex);
	PRM_Name* oldName = lastTemp->getNamePtr();

	PRM_Name* newName = new PRM_Name(oldName->getToken(), i_label.c_str());
	lastTemp->setNamePtr(newName);
}

void
SelectLayersDialog::removeMultiParmItem(
    PRM_Parm& io_parm, const std::string& i_label, unsigned i_nbItems)
{
	int size = io_parm.getMultiParmNumItems();
	assert(i_nbItems > 0);
	unsigned nbItems = 0;

	for (int i = size-1; i >= 0; i--)
	{
		PRM_Template* temp = io_parm.getMultiParmTemplate(i);
		PRM_Name* name = temp->getNamePtr();
		if (i_label == name->getLabel())
		{
			// PATCH: houdini don't update labels correctly, only tokens
			for (int j = i+1; j < size; j++)
			{
				PRM_Template* temp2 = io_parm.getMultiParmTemplate(j);
				PRM_Name* name2 = temp2->getNamePtr();
				name->setLabel(name2->getLabel());
				name = name2;
			}
			io_parm.removeMultiParmItem(i);
			// Because of Duplicate, they're may be more than one to remove...
			nbItems++;
			if (nbItems == i_nbItems)
			{
				break;
			}
		}
	}
}
