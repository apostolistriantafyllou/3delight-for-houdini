#pragma once

#include "exporter.h"

#include <vector>

class primitive;

/**
	\brief Exporter of geometry based on GT primitives.

	It manages a list of primitives obtained through recursive refinement of the
	object's main GT primitive. It simply forwards calls to the exporter's
	interface to each primitive in its list.
*/
class geometry : public exporter
{
public:
	/**
		\brief Constructor.

		\param i_context
			Current rendering context.
		\param i_object
			The object from which the GT primitives should be retrieved.
	*/
	geometry(const context& i_context, OBJ_Node* i_object);
	~geometry();

	void create()const override;
	void set_attributes()const override;
	void connect()const override;

private:

	/// List of refined primitives
	std::vector<primitive*> m_primitives;
};
