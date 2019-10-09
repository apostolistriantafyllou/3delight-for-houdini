#include "safe_interest.h"

#include <OP/OP_Node.h>

safe_interest::safe_interest(
	OP_Node* i_node,
	void* i_callee,
	OP_EventMethod i_cb)
	:	m_node(i_node),
		m_callee(i_callee),
		m_cb(i_cb)
{
	assert(m_node);
	assert(m_callee);
	assert(m_cb);
	m_node->addOpInterest(this, &static_callback);
}


safe_interest::safe_interest(const safe_interest& i_source)
	:	m_node(i_source.m_node),
		m_callee(i_source.m_callee),
		m_cb(i_source.m_cb)
{
	assert(m_callee);
	assert(m_cb);
	if(m_node)
	{
		m_node->addOpInterest(this, &static_callback);
	}
}


safe_interest::~safe_interest()
{
	if(m_node)
	{
		assert(m_node->hasOpInterest(this, &static_callback));
		m_node->removeOpInterest(this, &static_callback);
	}
}


const safe_interest&
safe_interest::operator=(const safe_interest& i_source)
{
	if(&i_source == this)
	{
		return *this;
	}

	if(m_node)
	{
		assert(m_node->hasOpInterest(this, &static_callback));
		m_node->removeOpInterest(this, &static_callback);
	}

	m_node = i_source.m_node;
	m_callee = i_source.m_callee;
	m_cb = i_source.m_cb;

	if(m_node)
	{
		m_node->addOpInterest(this, &static_callback);
	}

	return *this;
}


void safe_interest::static_callback(
	OP_Node* i_caller,
	void* i_callee,
	OP_EventType i_type,
	void* i_data)
{
	safe_interest* interest = (safe_interest*)i_callee;
	interest->callback(i_caller, i_type, i_data);
}


