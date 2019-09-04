#ifndef __jsonServer_h__
#define __jsonServer_h__

/*
	DlNetInit
	
	On linux this class does nothing.
	
	On windows, it calls WSAStartup on construction and WSACleanup on
	destruction. The return code from WSAStartup can be obtained with Status().

	It is possible to do the initialization later by giving false to the
	constructor and calling Init() manually.
*/

class DlNetInit
{
	DlNetInit(const DlNetInit&);
	void operator=(const DlNetInit&);

public:
	DlNetInit( bool i_do_init = true );
	~DlNetInit();

	int Init();
	int Status() const { return m_retCode; }

private:
	int m_retCode;
};

/*
	DlNetError

	This class records a network operation error code and formats it into a
	human readable message.

	The errors come from errno on unix and WSAGetLastError() on windows. Note
	that the semantics are slightly different too: a successful operation, such
	as closesocket(), will wipe the error from WSAGetLastError() but leave
	errno intact. This is why the error must be recorder into this class as
	soon as it happens.
*/
class DlNetError
{
public:
	DlNetError();
	~DlNetError();

	DlNetError( const DlNetError &rhs );
	void operator=( const DlNetError &rhs );

	void Record();
	void RecordIf( bool b ) { if ( b ) Record(); }

//	const char* Message();

private:
	void FreeMessage();

private:
	int m_error;
	char *m_message;
};

class DlArchive;
class DlSocketAddress;

class DlSocket
{
	DlSocket( const DlSocket& );
	void operator=( const DlSocket& );

	friend class DlFDSet;

public:
	DlSocket();
	~DlSocket();

	bool Init( const DlSocketAddress &i_addr );

	bool Bind();
	bool Listen();
	bool Accept( DlSocket *o_socket );

	bool Connect();

	/* Send/Receive a DlArchive. */
	bool Send( const DlArchive &i_data );
	bool Receive( DlArchive *o_data );

	/* Raw send/recv (still loops until all data is processed). */
	bool Send( const void *i_data, unsigned i_len );
	bool Receive( void *o_data, unsigned i_len );

	/*
		Receive without a loop which might only partially fill buffer.
		Returns the number of bytes read.
	*/
	unsigned ReceivePartial( void *o_data, unsigned i_len );

	/* Send/Reive to/from file descriptor. */
	bool Receive( int o_fd );
	bool Send( int i_fd );

	/* Raw sendto/recvfrom for datagram sockets. */
	bool SendTo( const void *i_data, unsigned i_len, DlSocketAddress &i_addr );
	bool ReceiveFrom( void *o_data, unsigned *io_len, DlSocketAddress *o_addr );

	void Shutdown();
	void Close();

	/* A socket is valid at any time between Init() and Close(). */
	bool Valid() const;

	/* Control over on/off socket options. k_xxx is SO_xxx. */
	enum eOption { k_REUSEADDR, k_BROADCAST, k_NODELAY, k_REUSEPORT };
	bool SetOption( eOption i_option, bool i_state );

	/*
		For bound socket, returns the local address.
		For a connected socket, returns the address of the remote end.
	*/
	const DlSocketAddress* Address() const { return m_addr; }

	/*
		Retrieves the local address of a connected socket (or a UDP socket from
		which data was sent). This will not work for a bound socket, use
		Address() instead.
	*/
	bool GetLocalAddress( DlSocketAddress *o_addr );

	const DlNetError& LastError() const { return m_error; }

private:
#if defined(_WIN64)
	typedef unsigned long long fd_type;
#elif defined(_WIN32)
	typedef unsigned fd_type;
#else
	typedef int fd_type;
#endif
	fd_type m_fd;

	DlSocketAddress *m_addr;

	DlNetError m_error;
};

#include <thread>

#include "pxr/base/js/json.h"

#include <assert.h>

class DlSocketAddress;

/*
	This class implements a JSON-RPC-like protocol.

	NOTES
	- The transport protocole uses our own "archives" for easy read and
	  and write to the sockets. So this is not a pure JSON RPC protocol.
	- Aim should be to simply adhere to JSON 2.0 protocole I think.
*/
class JSonServer
{
	enum { k_default_port = 0 };

public:
	struct ConnectionData
	{
		DlSocket *m_socket;
		JSonServer *m_server;
	};

	enum LogType
	{
		e_debug,
		e_log,
		e_warning,
		e_error
	};

public:
	JSonServer() :
		m_server_thread(0x0)
	{
	}

	virtual ~JSonServer()
	{
	}

	/*
		Starts a server thread and waits for connections.

		NOTES
		- Returns after starting the server loop in another thread.
	*/
	bool Start( int i_port = 0 );
	void Stop( void );

	/*
		Sends a reply to the specified client.
	*/
	virtual bool Reply( DlSocket &, const PXR_NS::JsObject& ) const;

	/*
		The parent class has to implement this.
	*/
    virtual void ExecuteJSonCommand(DlSocket &i_socket, const PXR_NS::JsObject& i_object)
	{
		assert( false );
	}

	virtual void Datagram( const DlSocketAddress &, char *data, int i_len ) {};

	/**
		\brief Connection has been closed

		\param i_socket
			The socket for this connection. Note that at this stage, this
			parameter can only be used for non IO operations.
	*/
	virtual void ConnectionClosed( const DlSocket * )
	{
	}

	/*
	*/
	virtual void Log( LogType i_type, const char *i_msg, ... ) const;
	virtual void LogString( LogType i_type, const char *i_msg ) const = 0;

	/*
		Returns the address of the server
	*/
	const DlSocketAddress &Address( void ) const
	{
		return *m_socket.Address();
	}

private:

	/*
		This loops starts in a thread and serves incoming connections.
	*/
	static void JSonServiceLoop( void *i_data );
	static void ClientConnection( void *i_data );

private:
	/** Main TCP and UDP sockets */
	DlSocket m_socket, m_socket_udp;

	/* */
	std::thread *m_server_thread;
	DlNetInit m_init;
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

    virtual void ExecuteJSonCommand(DlSocket &i_socket, const PXR_NS::JsObject& i_object);
	virtual void LogString( LogType i_type, const char *i_msg ) const;

private:

	// Not implemented
	jsonServer(const jsonServer&);
	const jsonServer& operator=(const jsonServer&);

	// Constructor
	jsonServer();

	static bool StartServer();

	static jsonServer* m_instance;
};

#endif
