#pragma once

#include "exporter.h"

#include <OP/OP_Value.h>

class OP_Node;

/**
	\brief Poly and poly soupe exporter.
*/
class null : public exporter
{
public:
	null( const context&, OBJ_Node * );

	void create( void ) const override;
	void set_attributes( void ) const override;
	void connect( void ) const override;

	static void changed_cb(
		OP_Node* i_caller,
		void* i_callee,
		OP_EventType i_type,
		void* i_data);

	/**
		\brief Returns true if the index refers to a transform parameter.

		Since Houdini's object contain their own transform, but NSI geometry
		nodes don't, we need a quick way to differentiate between transform and
		geometry edits. This should do for now.
	*/
	static bool is_transform_parameter_index(int i_parm_index)
	{
		/*
			The other parameters seem to have no effect on the transform : they
			are also displayed in other tabs of the object's parameters sheet.
		*/
		return i_parm_index <= 13;
	}

	/// Deletes the NSI nodes associated to Houdini node i_node.
	static void Delete(OBJ_Node& i_node, const context& i_context);
	
private:
	/// Exports time-dependent attributes to NSI
	void set_attributes_at_time( double i_time ) const;
};
