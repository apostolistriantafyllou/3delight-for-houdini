#pragma once

#include <nsi_dynamic.hpp>
#include <string>

class UT_JSONValue;

/**
	\brief This class is a TCP/IP communication port to receive
	i-display commands.

	Commands could be multi-light commands or re-render commands.

	Operates as a singleton.
*/
class idisplay_port
{

public:
	static idisplay_port *get_instance();

	idisplay_port();
	virtual ~idisplay_port();

	/// Returns the server's identification string for the display driver.
	const std::string& GetServerID()const { return m_server_id; }

private:

	/// Executes the command in the JSON object
	void ExecuteJSonCommand(const UT_JSONValue& i_command)const;

	/// Prints and log an error message from this command server
	void Log(const std::string& i_message)const;

	/// Parses i_message and sends a JSON object to ExecuteJSonCommand
	static void feedback_cb(void* i_userdata, const char* i_message);

	NSI::DynamicAPI m_api;
	std::string m_server_id;
	int m_main_thread;
};
