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

private:
	/// Exports time-dependent attributes to NSI
	void set_attributes_at_time( double i_time ) const;
};
