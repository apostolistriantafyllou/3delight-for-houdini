#pragma once

#include "exporter.h"
#include "vop.h"

#include "3Delight/ShaderQuery.h"
#include "osl_utilities.h"

#include <OP/OP_Value.h>

class OP_Node;
class OP_Parameters;

namespace NSI { class ArgumentList; }

class placement_matrix : public exporter
{

public:
	placement_matrix(const context&, VOP_Node *);

	void create(void) const override;

	void set_attributes(void) const override;
	void set_attributes_at_time(double i_time) const;

	void connect(void) const override;

	static void changed_cb(
		OP_Node* i_caller,
		void* i_callee,
		OP_EventType i_type,
		void* i_data);

};
