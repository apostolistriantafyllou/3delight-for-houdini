#pragma once

#include <OH/OH_EventHandler.h>

#include <functional>

/// Calls a callback when the application's current time changes.
class time_notifier : public OH_EventHandler
{
public:

	/// Callback type, receives the updated time
	typedef std::function<void(double)> cb;
	
	/**
		\brief Constructor.

		\param i_cb
			Callback to be called when the the current time changes.
		\param i_drag
			True to be notified of drag events (ie : quick successive changes in
			value), or false to ignore them and just get the final time value.
	*/
	time_notifier(const cb& i_cb, bool i_drag);
	/// Destructor
	virtual ~time_notifier();

	/// Called when current time changes
	void ohHandleTimeChange(UI_Event* i_event) override;

private:

	OH_TriggerObject m_trigger;
	cb m_cb;
	bool m_drag;
};
