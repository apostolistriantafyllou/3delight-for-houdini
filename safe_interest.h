#pragma once

#include <OP/OP_Value.h>
#include <assert.h>

class OP_Node;


/**
	Manages an interest in an OP_Node.

	Ensures that the interest will be removed when the callback is not needed,
	but also that we won't try to remove an interest on a deleted node.
*/
class safe_interest
{
public:

	/// Constructor with callback details
	safe_interest(OP_Node* i_node, void* i_callee, OP_EventMethod i_cb);
	/**
		\brief Copy-constructor.

		The callback details are duplicated so, if the interest is ever
		activated. the callback would be called both by the source and the
		newly-constructed object. This is unlikely to be a problem because it's
		most likely a temporary state.
	*/
	safe_interest(const safe_interest& i_source);
	/// Destructor
	~safe_interest();

	/**
		\brief Assignment operator.

		The callback details are duplicated so, if the interest is ever
		activated. the callback would be called both by the source and the
		newly-assigned object. This is unlikely to be a problem because it's
		most likely a temporary state.
	*/
	const safe_interest& operator=(const safe_interest& i_source);

private:

	/*
		Callback called by Houdini's interest system, simply calls callback() on
		a safe_interest object.
	*/
	static void static_callback(
		OP_Node* i_caller,
		void* i_callee,
		OP_EventType i_type,
		void* i_data);

	// Calls the client callback, handling deletion event when they occur.
	void callback(
		OP_Node* i_caller,
		OP_EventType i_type,
		void* i_data)
	{
		// Call the actual client callback
		m_cb(i_caller, m_callee, i_type, i_data);

		if(i_type == OP_NODE_DELETED)
		{
			// Prevent removeOpInterest from being called in the destructor
			assert(m_node);
			m_node = nullptr;
		}
	}

	OP_Node* m_node;
	void* m_callee;
	OP_EventMethod m_cb;
};


