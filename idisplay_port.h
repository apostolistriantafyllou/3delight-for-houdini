#pragma once

#include <UT/UT_SocketListener.h>

#include <string>
#include <vector>
#include <thread>

class UT_JSONValue;
class UT_NetSocket;

/**
	\brief This class is a TCP/IP communication port to receive
	i-display commands.

	Commands could be multi-light commands or re-render commands.

	Operates as a singleton.
*/
class idisplay_port: public UT_SocketListener
{

public:
	static idisplay_port *get_instance();

	idisplay_port();
	virtual ~idisplay_port();

	/** Stops the server and destroys its resources */
	static void CleanUp();

	/** Returns the server's port number. */
	int GetServerPort();

	/** Returns the server's host name. */
	std::string GetServerHost();

    void ExecuteJSonCommand( const UT_JSONValue & );

	/** A simple logging utility for varying args */
	void Log( const char *i_msg, ... ) const;

protected:
	virtual void onSocketEvent(int event_types);

private:
	static bool StartServer();

	std::vector<UT_NetSocket*> m_clients;
	std::vector<std::thread> m_threads;
};
