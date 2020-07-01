#include "time_notifier.h"

#include <UI/UI_Event.h>


time_notifier::time_notifier(const cb& i_cb, bool i_drag)
	/*
		OH_EventHandler's constructor uses an uninitialized m_trigger and
		m_trigger's constructor uses a still incomplete *this...  It's not
		pretty, but this seems to be the standard way to do this, so we'll
		leave it like this until it actually causes problems.
	*/
	:	OH_EventHandler(m_trigger),
		m_trigger(*this),
		m_cb(i_cb),
		m_drag(i_drag)
{
	ohAddTimeInterest();
}

time_notifier::~time_notifier()
{
	ohRemoveTimeInterest();
}

void time_notifier::ohHandleTimeChange(UI_Event* i_event)
{
	assert(i_event);
	
	if((!m_drag && i_event->reason == UI_VALUE_ACTIVE) ||
		i_event->reason == UI_VALUE_START ||
		i_event->reason == UI_VALUE_LOCATED)
	{
		return;
	}

	UI_Value* value = i_event->value;
	assert(value);
	assert(value->getType() == UI_VALUE_FLOAT);

	m_cb(double(*value));
}
