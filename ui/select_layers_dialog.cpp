#include "select_layers_dialog.h"
#include <ROP/ROP_Node.h>

#include "aov.h"

#include <stdio.h>
// Used to wrap around callback functions responding to UI_Value changes
#define MYDIALOG_CB(method)     ((UI_EventMethod)&SelectLayersDialog::method)

static const char* k_aov = "aov";

SelectLayersDialog::SelectLayersDialog()
	: m_parsedDialog(false)
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
		m_foundItems.push_back(false);
	}
	m_node = io_node;

	int nb_aovs = m_node->evalInt(k_aov, 0, 0.0f);
	for (int i = 0; i < nb_aovs; i++)
	{
		std::string aov_name = aov::getAovStrToken(i);
		updateToggle(i, aov_name);
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
	m_foundItems.clear();
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
		if (!value && m_foundItems[i])
		{
			removeMultiParmItem(parm, m_labels[i]);
		}
	}
	for (unsigned i = 0; i < m_symbols.size(); i++)
	{
		bool value = (bool)*getValueSymbol(m_symbols[i].c_str());
		if (value && !m_foundItems[i])
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
SelectLayersDialog::updateToggle(int index, const std::string& i_aov_name)
{
	UT_String label;
	m_node->evalString(label, aov::getAovStrToken(index), 0, 0.0f);

	for (unsigned i = 0; i < m_labels.size(); i++)
	{
		if (label == m_labels[i])
		{
			updateValue(*m_values[i]);
			m_foundItems[i] = true;
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
#if 0
	PRM_Parm* lastParm = io_parm.getMultiParm(lastIndex);
	assert(lastParm);
	PRM_ParmList* pList = lastParm->getOwner();
	assert(pList);

	PRM_Parm* label = pList->getParmPtr(aov::getAovLabelToken(lastIndex));
	assert(label);
	PRM_Template* labelTemp = label->getTemplatePtr();
	assert(labelTemp);
	PRM_Name* labelName = labelTemp->getNamePtr();
	assert(labelName);
	fprintf(stderr, "addMultiParmItem - labelName->getToken(): %s\n", labelName->getToken());
	fprintf(stderr, "addMultiParmItem - i_label: %s\n", i_label.c_str());
	labelName->setLabel(i_label.c_str());
	label->setSaveLabelFlag(true);
	int index = m_node->getParmIndex(aov::getAovLabelToken(lastIndex));
	fprintf(stderr, "(1) m_node - index: %d\n", index);
	m_node->parmChanged(index);
	m_node->updatePending(0.0);
	m_node->updateParmsFlags();
#endif
	m_node->setString(
		i_label.c_str(), CH_STRING_LITERAL,
		aov::getAovStrToken(lastIndex), 0, 0.0);
}

void
SelectLayersDialog::removeMultiParmItem(
    PRM_Parm& io_parm, const std::string& i_label)
{
	int size = io_parm.getMultiParmNumItems();
	for (int i = size-1; i >= 0; i--)
	{
#if 0
		PRM_Parm* currParm = io_parm.getMultiParm(i);
		assert(currParm);
		PRM_ParmList* pList = currParm->getOwner();
		assert(pList);

		PRM_Parm* str = pList->getParmPtr(aov::getAovStrToken(i));
		assert(str);
#endif
		UT_String label;
		m_node->evalString(label, aov::getAovStrToken(i), 0, 0.0f);

		if (i_label == label.toStdString())
		{
#if 0
			PRM_Parm* label = pList->getParmPtr(aov::getAovLabelToken(i));
			assert(label);
			PRM_Template* labelTemp = label->getTemplatePtr();
			assert(labelTemp);
			PRM_Name* labelName = labelTemp->getNamePtr();
			assert(labelName);
			fprintf(stderr, "removeMultiParmItem - labelName->getToken(): %s\n",
					labelName->getToken());
			fprintf(stderr, "removeMultiParmItem - labelName->getLabel(): %s\n",
					labelName->getLabel());

			// PATCH: houdini don't update labels correctly, only tokens/values
			for (int j = i+1; j < size; j++)
			{
				PRM_Parm* currParm2 = io_parm.getMultiParm(j);
				assert(currParm2);
				PRM_ParmList* pList2 = currParm2->getOwner();
				assert(pList2);

				PRM_Parm* label2 = pList2->getParmPtr(aov::getAovLabelToken(j));
				assert(label2);
				PRM_Template* labelTemp2 = label2->getTemplatePtr();
				assert(labelTemp2);
				PRM_Name* labelName2 = labelTemp2->getNamePtr();
				assert(labelName2);
				labelName->setLabel(labelName2->getLabel());
				fprintf(stderr, "(2) removeMultiParmItem - labelName->getToken(): %s\n",
						labelName->getToken());
				fprintf(stderr, "(2) removeMultiParmItem - labelName->getLabel(): %s\n",
						labelName->getLabel());
				labelName = labelName2;
				fprintf(stderr, "removeMultiParmItem - labelName2->getToken(): %s\n",
						labelName2->getToken());
				fprintf(stderr, "removeMultiParmItem - labelName2->getLabel(): %s\n",
						labelName2->getLabel());
			}
#endif
			io_parm.removeMultiParmItem(i);
			break;
		}
	}
#if 0
	size = io_parm.getMultiParmNumItems();
	for (int i = 0; i < size; i++)
	{
		int index = m_node->getParmIndex(aov::getAovLabelToken(i));
		fprintf(stderr, "m_node - index: %d\n", index);
		m_node->parmChanged(index);
	}
	m_node->updatePending(0.0);
	m_node->updateParmsFlags();
#endif
}
