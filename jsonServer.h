#ifndef __jsonServer_h__
#define __jsonServer_h__

#include "pxr/base/js/json.h"

#include <UT/UT_SocketListener.h>

class UT_NetSocket;

class JSonServer: public UT_SocketListener
{
	enum { k_default_port = 0 };

public:

	enum LogType
	{
		e_debug,
		e_log,
		e_warning,
		e_error
	};

	JSonServer(UT_NetSocket* i_socket);
	virtual ~JSonServer();

	int getPort() const;
	std::string getServerHost() const;
	/*
		The parent class has to implement this.
	*/
    virtual void ExecuteJSonCommand(const PXR_NS::JsObject& i_object) = 0;

	virtual void Log( LogType i_type, const char *i_msg, ... ) const;
	/*
		The parent class has to implement this.
	*/
	virtual void LogString( LogType i_type, const char *i_msg ) const = 0;

protected:
	virtual void onSocketEvent(int event_types);

private:
	UT_NetSocket* m_client;
};


#include <string>

class jsonServer: public JSonServer
{
public:

	// Destructor
	virtual ~jsonServer();

	// Stops the server and destroys its resources
	static void CleanUp();
	// Returns the server's port number.  Start the server if necessary.
	static int GetServerPort();
	// Returns the server's host name.  Start the server if necessary.
	static std::string GetServerHost();

    virtual void ExecuteJSonCommand(const PXR_NS::JsObject& i_object);
	virtual void LogString( LogType i_type, const char *i_msg ) const;

private:

	// Not implemented
	jsonServer(const jsonServer&);
	const jsonServer& operator=(const jsonServer&);

	// Constructor
	jsonServer(UT_NetSocket* i_socket);

	static bool StartServer();

	static jsonServer* m_instance;
};

#endif
