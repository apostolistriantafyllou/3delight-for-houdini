#pragma once

#include "exporter.h"

#include <OP/OP_Value.h>
#include <vector>

class OP_Node;

/**
	\brief Incandescence light exporter.
*/
class incandescence_light : public exporter
{
public:
	incandescence_light( const context&, OBJ_Node *);

	void create() const override;
	void set_attributes() const override;
	void connect() const override;

	static void changed_cb(
		OP_Node* i_caller,
		void* i_callee,
		OP_EventType i_type,
		void* i_data);

private:

	/// Disconnects the light from the scene.
	void disconnect()const;
	/// Computes output color from incandescence parameters
	void compute_output_color(float o_color[]) const;

	/// Returns the handle of the incandescence's NSI attributes node
	std::string attributes_handle()const
	{
		return m_handle + "|attributes";
	}
	/// Returns the handle of the incandescence's NSI set node
	std::string set_handle()const
	{
		return m_handle + "|set";
	}

	/**
		We have to keep track of the currently "managed" materials as we have
		to reset their multipliers to 1 during live operation. When
		SetAttributes is called the object_selection parameter already
		contains the new value and we need to access the old state.

		\see disconnect
	*/
	mutable std::vector<std::string> m_current_multipliers;
};
