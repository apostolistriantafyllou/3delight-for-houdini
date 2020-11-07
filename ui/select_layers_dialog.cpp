#include "select_layers_dialog.h"
#include <ROP/ROP_Node.h>
#include <VOP/VOP_Node.h>
#include <FS/UT_PathFile.h>
#include <UT/UT_TempFileManager.h>

#include "aov.h"

#include <stdio.h>
// Used to wrap around callback functions responding to UI_Value changes
#define MYDIALOG_CB(method)     ((UI_EventMethod)&SelectLayersDialog::method)

static const char* k_aov = "aov";

SelectLayersDialog::SelectLayersDialog()
	: m_parsedDialog(false)
{
	for(unsigned a = 0; a < aov::nbPredefined(); a++)
	{
		const aov::description& desc = aov::getDescription(a);

		m_values.push_back(new UI_Value());
		m_labels.push_back(desc.m_ui_name);
		m_symbols.push_back(desc.m_variable_name + ".val");
	}
}

bool
SelectLayersDialog::parseDialog(const std::vector<VOP_Node*>& i_custom_aovs)
{
	// Builds dialog last column with custom aovs
	std::vector<std::string> aov_names;
	for (unsigned i = 0; i < i_custom_aovs.size(); i++)
	{
		//UT_String aov_name = "Test";
		for (int j = 0; j < i_custom_aovs[i]->getNumVisibleInputs(); j++)
		{
			UT_String aov_name = i_custom_aovs[i]->inputLabel(j);
			if (!findAovName(aov_names, aov_name.c_str()))
			{
				aov_names.push_back(aov_name.toStdString());
				m_values.push_back(new UI_Value());
				m_labels.push_back(aov_names.back());
				m_symbols.push_back(aov_names.back() + ".val");
			}
		}
	}
	// These bind the named UI values with the given objects.
	// It's not strictly necessary but is more readable than using
	// getValueSymbol() after calling parseUI().
	setValueSymbol("dlg.val", &m_openValue);
	for (unsigned i = 0; i < m_symbols.size(); i++)
	{
		setValueSymbol(m_symbols[i].c_str(), m_values[i]);
	}

	// Find our dialog description file
	UT_String pathName;
	if(!UT_PathSearch::getInstance(UT_HOUDINI_UI_APP_PATH)->
		findFile(pathName, "select_layers_ui.ui"))
	{
		assert(false);
		return false;
	}

	const std::string separator = 
#ifdef _WIN32
		"\\";
#else
		"/";
#endif

	// Builds a temp name for temp file "select_layers_ui.ui"
	UT_String temp(UT_TempFileManager::getTempFilename());
	UT_String tempName, fileName;
	temp.splitPath(tempName, fileName);
	tempName += separator;
	tempName += "select_layers_ui.ui";

	// Builds the temp file "select_layers_ui.ui" from the existing one
	FILE* input = fopen(pathName.c_str(), "r");
	assert(input);
	FILE* output = fopen(tempName.c_str(), "w");
	assert(output);

	// Make a copy until the insertion mark
	int c;
	while ((c = fgetc(input)) != '>') fputc(c, output);
	fputc(c, output);
	fputc('\n', output);
	// Insert our custom variables
	for (unsigned i = 0; i < aov_names.size(); i++)
	{
		fprintf(output, "TOGGLE_BUTTON ");
		fprintf(output, "\"%s\"", aov_names[i].c_str());
		fprintf(output, ":LABEL_WIDTH VALUE(");
		fprintf(output, "%s);\n", (aov_names[i]+".val").c_str());
	}

	// Finish the copy
	while ((c = fgetc(input)) != EOF) fputc(c, output);
	fclose(output);
	fclose(input);

	// Parse the temp file
	if (!readUIFile(tempName.c_str()))
		return false;   // error parsing
	// We can also directly add interest on an unbound but named UI_Value
	getValueSymbol("ok.val")->addInterest(this, MYDIALOG_CB(handleOK));
	return true;
}

bool
SelectLayersDialog::findAovName(
	const std::vector<std::string>& i_aov_names,
	const std::string& i_aov_name) const
{
	for (unsigned i = 0; i < i_aov_names.size(); i++)
	{
		if (i_aov_names[i] == i_aov_name)
		{
			return true;
		}
	}

	return false;
}

bool
SelectLayersDialog::open(
	ROP_Node* io_node,
	const std::vector<VOP_Node*>& i_custom_aovs)
{
	// Only parse the dialog once
	if (!m_parsedDialog)
	{
		if (!parseDialog(i_custom_aovs))
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
