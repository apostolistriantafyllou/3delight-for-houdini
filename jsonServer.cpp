#include "jsonServer.h"

#include <HOM/HOM_Module.h>
#include <OBJ/OBJ_Light.h>
#include <OP/OP_Director.h>
#include <UT/UT_PackageUtils.h>

#include <assert.h>

//--------------DlNetInit---------------------

#ifdef _WIN32
#	include <winsock2.h>
#endif

#ifdef _WIN32

DlNetInit::DlNetInit( bool i_do_init )
:
	m_retCode(-1)
{
	if( i_do_init )
		Init();
}

DlNetInit::~DlNetInit()
{
	if( m_retCode == 0 )
	{
		::WSACleanup();
	}
}

int DlNetInit::Init()
{
	/* Only do it once to keep things balanced. */
	if( m_retCode != 0 )
	{
		WSADATA wsaData;
		m_retCode = ::WSAStartup(MAKEWORD(2, 2), &wsaData);
	}

	return m_retCode;
}

#else

DlNetInit::DlNetInit( bool )
:
	m_retCode(0)
{
}

DlNetInit::~DlNetInit()
{
}

int DlNetInit::Init()
{
	return 0;
}

#endif

//--------------DlNetError---------------------


#ifdef _WIN32
#	include <windows.h>
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>

DlNetError::DlNetError()
:
	m_error(0),
	m_message(0x0)
{
}

DlNetError::~DlNetError()
{
	FreeMessage();
}

DlNetError::DlNetError( const DlNetError &rhs )
:
	m_error(rhs.m_error),
	m_message(0x0)
{
}

void DlNetError::operator=( const DlNetError &rhs )
{
	m_error = rhs.m_error;
	FreeMessage();
}

void DlNetError::Record()
{
	FreeMessage();

#ifdef _WIN32
	m_error = WSAGetLastError();
#else
	m_error = errno;
#endif
}
#if 0
const char* DlNetError::Message()
{
	if( m_message )
		return m_message;

#ifdef _WIN32
	DWORD ret = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		m_error,
		0, /* any language */
		(LPTSTR)&m_message,
		0x0,
		NULL );

	if( ret == 0 )
		return "";
#else
	const int k_buf_size = 200;
	m_message = (char*) calloc( 200, 1 );

#	ifdef _GNU_SOURCE
	/* Can't avoid the nonstandard version with older gcc/libc. It usually will
	   not use the provided buffer so fix this. */
	char *e = strerror_r( m_error, m_message, k_buf_size );
	if( e != m_message )
	{
		strncpy( m_message, e, k_buf_size - 1 );
		m_message[k_buf_size - 1] = '\0';
	}
#	else
	/* 'dummy' is just to make sure we're not calling the gnu version. */
	int dummy = strerror_r( m_error, m_message, k_buf_size );
#	endif
#endif

	/* Srip any newline at the end of the message (windows has them). */
	size_t l = strlen( m_message );
	if( m_message[l - 1] == '\n' )
		m_message[l - 1] = '\0';
	if( m_message[l - 2] == '\r' )
		m_message[l - 2] = '\0';

	return m_message;
}
#endif
void DlNetError::FreeMessage()
{
#ifdef _WIN32
	LocalFree( m_message );
#else
	free( m_message );
#endif
	m_message = 0x0;
}

//--------------DlArchive---------------------

/*
	DlArchive

	This class is meant as a generic buffer to serialize data into. It can then
	be transmitted over the network or stored in a file for later use.

	It handles endianness conversion and its interface splits types explicitely
	instead of using overloading to reduce the risk of 32-bit and 64-bit
	systems behaving differently.

	The buffer may also choose to align the data however it wants so you should
	not rely on the data layout. This means the sequence of Read calls should
	be the exact same as the Write calls used to fill the buffer.
*/

typedef int    dl_int32;
typedef short  dl_int16;
typedef char   dl_int8;

typedef signed int    dl_sint32;
typedef signed short  dl_sint16;
typedef signed char   dl_sint8;

typedef unsigned int    dl_uint32;
typedef unsigned short  dl_uint16;
typedef unsigned char   dl_uint8;

#if defined(__x86_64__) || defined(__powerpc64__)
typedef long dl_int64;
typedef signed long dl_sint64;
typedef unsigned long dl_uint64;
#	define FORMAT_INT64 "l"
#else
typedef long long dl_int64;
typedef signed long long dl_sint64;
typedef unsigned long long dl_uint64;
/* Use to printf a dl_int64: printf( "value: %" FORMAT_INT64 "d\n" ); */
#	ifdef _WIN32
#		define FORMAT_INT64 "I64"
#	else
#		define FORMAT_INT64 "ll"
#	endif
#endif

/* Pointer sized integer types. Like C99's intptr_t and uintptr_t. */
#if defined(_WIN64) || defined(__x86_64__) || defined(__powerpc64__)
typedef dl_int64 dl_intptr;
typedef dl_uint64 dl_uintptr;
#	define FORMAT_INTPTR FORMAT_INT64
#else
typedef dl_int32 dl_intptr;
typedef dl_uint32 dl_uintptr;
#	define FORMAT_INTPTR ""
#endif

class DlArchive
{
	DlArchive( const DlArchive& );
	void operator=( const DlArchive& );

public:
	DlArchive();
	~DlArchive();

	/* Methods to serialize or unserialize data. */

	void WriteInt32( dl_int32 v );
	dl_int32 ReadInt32();

	void WriteUInt32( dl_uint32 v );
	dl_uint32 ReadUInt32();

	void WriteInt64( dl_int64 v );
	dl_int64 ReadInt64();

	void WriteUInt64( dl_uint64 v );
	dl_uint64 ReadUInt64();

	void WriteFloat( float v );
	float ReadFloat();

	void WriteDouble( double v );
	double ReadDouble();

	void WriteChar( char c );
	char ReadChar();


	void WriteFloats( unsigned i_num, const float *i_v );
	void ReadFloats( unsigned i_num, float *o_v );

	/*
		Returned string is owned by the DlArchive. It is only valid until the
		buffer is modified again.
	*/
	void WriteString( const char *s );
	const char* ReadString();

	void WriteArchive( const DlArchive &i_archive );
	DlArchive *ReadArchive();

	void clear();

	bool Eof() const { return m_eof; }

	/* Raw access. */
	unsigned size() const { return m_write_ptr; }
	const void *RawData() const { return m_buf; }

	void WriteBytes( unsigned i_num_bytes, const void *i_bytes );
	void ReadBytes( unsigned i_num_bytes, void *o_bytes );

	/* Read directly from buffer, without copy. */
	void* ReadBytes( unsigned i_num_bytes );

	/**
		\brief ReadRewind the *read* pointer to the begining of the archive.
	*/
	void ReadRewind( void );

private:
	unsigned m_allocated_size;
	char *m_buf;

	unsigned m_read_ptr;
	unsigned m_write_ptr;

	bool m_eof;
};

template<bool truth>
struct DlCompileAssert
{
};

template<>
struct DlCompileAssert<true>
{
	static void assertion_failed() {}
};

#define compile_assert(expr) DlCompileAssert<(expr)>::assertion_failed()
#if defined(DL_LITTLE_ENDIAN) || defined(DL_BIG_ENDIAN)
#error DL_xyz_ENDIAN should not be defined on the command line
#endif

/* gcc/icc on x86 linux and gcc on x86 OS X */
#if defined(__i386__) 
#define DL_LITTLE_ENDIAN 1

/* gcc on x86-64 linux */
#elif defined(__x86_64__)
#define DL_LITTLE_ENDIAN 1

/* cl/icl on x86 windows */
#elif defined(_M_IX86)
#define DL_LITTLE_ENDIAN 1

/* cl on x86-64 windows (msdn says _M_X64 but it's not defined) */
#elif defined(_M_AMD64) || defined(_M_X64)
#define DL_LITTLE_ENDIAN 1

/* gcc on ppc OS X (and likely other big endian) */
#elif defined(__ppc__) || defined(__BIG_ENDIAN__)
#define DL_BIG_ENDIAN 1

/* defined by us on IRIX, there's probably something better */
#elif defined(IRIX_MIPS4)
#define DL_BIG_ENDIAN 1

#else
#error unknown endianness
#endif


/* Somewhat optimized endian swap for 16, 32 and 64 bits. Assembly would be way
   better as some machines can do this in a single instruction but I don't think
   it would be significant for this project. This generates semi-decent code
   (many instructions but everything is done in registers).
*/
inline dl_uint16 DlSwapUInt16(dl_uint16 x)
{
	return (x >> 8u) | (x << 8u);
}

inline dl_uint32 DlSwapUInt32(dl_uint32 x)
{
	return
		(x >> 24u) |
		((x >>  8u) & 0x0000ff00u) |
		((x <<  8u) & 0x00ff0000u) |
		(x << 24u);
}

inline dl_uint64 DlSwapUInt64(dl_uint64 x)
{
	return
		dl_uint64(DlSwapUInt32(dl_uint32(x >> 32u))) |
		dl_uint64(DlSwapUInt32(dl_uint32(x))) << 32u;
}

/*
	Generic endian swap macro which uses the optimized version when possible.

	IMPORTANT NOTE

	Do not use functions for conversion of floating point data or your data may
	be messed up by the float->double->float conversion done when passing the
	parameters. Try this code (tested on a P4):

	unsigned k = 0x7f89683f;
	float f = * (float*) &k;
	double f2 = f;
	float f3 = f2;
	printf("f: %x k: %x\n", *(unsigned*)&f3, k);

	All the ugly _3dl_ prefixes are to avoid name clashes. Wouldn't really hurt
	but triggers a lot of compiler warnings.

	All the nasty templates in there are so we can:
	1) Generate the best possible code for common cases (without assembly).
	2) Access the data through a union with the original type so the compiler's
	   alias analysis picks it up.
*/
#define ENDIAN_SWAP(_3dl_x) \
	_3dl_do_endian_swap( &(_3dl_x) );

template<unsigned size>
struct _3dl_endian_swap
{
	template <typename T>
	static inline
	void _do_swap( T *io_value )
	{
		union U
		{
			T value;
			unsigned char chiffes[size];
		};
		U *d = (U*)io_value;

		unsigned char buf[size];
		for (unsigned i = size; i; --i)
		{
			buf[i-1] = d->chiffes[size-i];
		}
		for (unsigned j = 0; j < size; ++j)
		{
			d->chiffes[j] = buf[j];
		}
	}
};

template<>
struct _3dl_endian_swap<2>
{
	template <typename T>
	static inline
	void _do_swap( T *io_value )
	{
		union U
		{
			T value;
			dl_uint16 chiffes;
		};
		U *d = (U*)io_value;

		d->chiffes = DlSwapUInt16( d->chiffes );
	}
};

template<>
struct _3dl_endian_swap<4>
{
	template <typename T>
	static inline
	void _do_swap( T *io_value )
	{
		union U
		{
			T value;
			dl_uint32 chiffes;
		};
		U *d = (U*)io_value;

		d->chiffes = DlSwapUInt32( d->chiffes );
	}
};

template<>
struct _3dl_endian_swap<8>
{
	template <typename T>
	static inline
	void _do_swap( T *io_value )
	{
		union U
		{
			T value;
			dl_uint64 chiffes;
		};
		U *d = (U*)io_value;

		d->chiffes = DlSwapUInt64( d->chiffes );
	}
};

template <typename T>
inline
void _3dl_do_endian_swap( T *io_value )
{
	_3dl_endian_swap<sizeof(T)>::_do_swap( io_value );
}

/* Endian-specific section */

#ifdef DL_BIG_ENDIAN

#define ENDIAN_TO_LSB(_3dl_x) ENDIAN_SWAP(_3dl_x)
#define ENDIAN_FROM_LSB(_3dl_x) ENDIAN_SWAP(_3dl_x)

#define ENDIAN_TO_MSB(_3dl_x)
#define ENDIAN_FROM_MSB(_3dl_x)

#else // DL_LITTLE_ENDIAN

#define ENDIAN_TO_LSB(_3dl_x)
#define ENDIAN_FROM_LSB(_3dl_x)

#define ENDIAN_TO_MSB(_3dl_x) ENDIAN_SWAP(_3dl_x)
#define ENDIAN_FROM_MSB(_3dl_x) ENDIAN_SWAP(_3dl_x)

#endif

DlArchive::DlArchive()
:
	m_allocated_size(0),
	m_buf(0x0),
	m_read_ptr(0),
	m_write_ptr(0),
	m_eof(false)
{
}

DlArchive::~DlArchive()
{
	free( m_buf );
}

void DlArchive::ReadRewind( void )
{
	m_read_ptr = 0;
	m_eof = false;
}

void DlArchive::WriteInt32( dl_int32 v )
{
	compile_assert( sizeof(v) == 4 );
	ENDIAN_TO_MSB( v );
	WriteBytes( sizeof(v), &v );
}

dl_int32 DlArchive::ReadInt32()
{
	dl_int32 v = 0;
	compile_assert( sizeof(v) == 4 );
	ReadBytes( sizeof(v), &v );
	ENDIAN_FROM_MSB( v );
	return v;
}


void DlArchive::WriteUInt32( dl_uint32 v )
{
	compile_assert( sizeof(v) == 4 );
	ENDIAN_TO_MSB( v );
	WriteBytes( sizeof(v), &v );
}

dl_uint32 DlArchive::ReadUInt32()
{
	dl_uint32 v = 0;
	compile_assert( sizeof(v) == 4 );
	ReadBytes( sizeof(v), &v );
	ENDIAN_FROM_MSB( v );
	return v;
}


void DlArchive::WriteInt64( dl_int64 v )
{
	compile_assert( sizeof(v) == 8 );
	ENDIAN_TO_MSB( v );
	WriteBytes( sizeof(v), &v );
}

dl_int64 DlArchive::ReadInt64()
{
	dl_int64 v = 0;
	compile_assert( sizeof(v) == 8 );
	ReadBytes( sizeof(v), &v );
	ENDIAN_FROM_MSB( v );
	return v;
}


void DlArchive::WriteUInt64( dl_uint64 v )
{
	compile_assert( sizeof(v) == 8 );
	ENDIAN_TO_MSB( v );
	WriteBytes( sizeof(v), &v );
}

dl_uint64 DlArchive::ReadUInt64()
{
	dl_uint64 v = 0;
	compile_assert( sizeof(v) == 8 );
	ReadBytes( sizeof(v), &v );
	ENDIAN_FROM_MSB( v );
	return v;
}


void DlArchive::WriteFloat( float v )
{
	compile_assert( sizeof(v) == 4 );
	ENDIAN_TO_MSB( v );
	WriteBytes( sizeof(v), &v );
}

float DlArchive::ReadFloat()
{
	float v = 0.0f;
	compile_assert( sizeof(v) == 4 );
	ReadBytes( sizeof(v), &v );
	ENDIAN_FROM_MSB( v );
	return v;
}

void DlArchive::WriteDouble( double v )
{
	compile_assert( sizeof(v) == 8 );
	ENDIAN_TO_MSB( v );
	WriteBytes( sizeof(v), &v );
}

double DlArchive::ReadDouble()
{
	double v = 0.0;
	compile_assert( sizeof(v) == 8 );
	ReadBytes( sizeof(v), &v );
	ENDIAN_FROM_MSB( v );
	return v;
}

void DlArchive::WriteChar( char c )
{
	WriteBytes( 1, &c );
}

char DlArchive::ReadChar()
{
	char c = 0;
	ReadBytes( 1, &c );
	return c;
}

/* TODO: optimize */
void DlArchive::WriteFloats( unsigned i_num, const float *i_v )
{
	for( unsigned i = 0; i < i_num; ++i )
	{
		WriteFloat( i_v[i] );
	}
}

/* TODO: optimize */
void DlArchive::ReadFloats( unsigned i_num, float *o_v )
{
	for( unsigned i = 0; i < i_num; ++i )
	{
		o_v[i] = ReadFloat();
	}
}

void DlArchive::WriteString( const char *s )
{
	unsigned l = strlen( s );
	WriteBytes( l + 1, s );
}

const char* DlArchive::ReadString()
{
	if( m_eof )
		return "";

	const char *str = m_buf + m_read_ptr;

	while( m_buf[m_read_ptr] != '\0' )
	{
		++m_read_ptr;
		if( m_read_ptr > m_write_ptr )
		{
			m_eof = true;
			return "";
		}
	}

	/* skips over the null byte */
	++m_read_ptr;

	return str;
}

void DlArchive::clear()
{
	m_read_ptr = 0;
	m_write_ptr = 0;
	m_eof = false;
}

void DlArchive::WriteBytes( unsigned i_num_bytes, const void *i_bytes )
{
	unsigned new_size = m_write_ptr + i_num_bytes;
	if( new_size > m_allocated_size )
	{
		m_allocated_size += m_allocated_size / 2 + 32;

		if( new_size > m_allocated_size )
			m_allocated_size = new_size;

		m_buf = (char*) realloc( m_buf, m_allocated_size );
	}

	memcpy( m_buf + m_write_ptr, i_bytes, i_num_bytes );
	m_write_ptr = new_size;
}

void DlArchive::ReadBytes( unsigned i_num_bytes, void *o_bytes )
{
	if( m_eof )
		return;

	if( m_read_ptr + i_num_bytes > m_write_ptr )
	{
		m_eof = true;
		return;
	}

	memcpy( o_bytes, m_buf + m_read_ptr, i_num_bytes );
	m_read_ptr += i_num_bytes;
}

void* DlArchive::ReadBytes( unsigned i_num_bytes )
{
	if( m_eof )
		return 0x0;

	if( m_read_ptr + i_num_bytes > m_write_ptr )
	{
		m_eof = true;
		return 0x0;
	}

	void *bytes = m_buf + m_read_ptr;
	m_read_ptr += i_num_bytes;

	return bytes;
}

/**
	\brief Writes an archive into this archive.
	\sa ReadArchive
*/
void DlArchive::WriteArchive( const DlArchive &i_archive )
{
	assert( i_archive.size() );

	WriteUInt32( (dl_uint32)i_archive.size() );
	WriteBytes( i_archive.size(), i_archive.RawData() );
}

/**
	\brief Read an archive encoded using WriteArchive
	\sa WriteArchive
*/
DlArchive *DlArchive::ReadArchive()
{
	if( m_eof )
		return 0x0;

	dl_uint32 s = ReadUInt32();

	if( m_eof )
		return 0x0;

	assert( s );

	void *bytes = ReadBytes( s );

	if( !bytes )
	{
		assert( !"badly encoded DlArchive" );
		return 0x0;
	}

	DlArchive *a = new DlArchive;
	a->WriteBytes( s, bytes );

	return a;
}

//--------------DlSocketAddress---------------------


/*
	DlSocketAddress

	This class represents the complete address of a socket. This includes the
	IP address, port as well as the protocols involved (eg. IPv4 or IPv6, TCP
	or UDP, etc). Having all that stuff hidden behind a single class makes
	sockets much easier to use.
*/

class DlArchive;

struct sockaddr;

class DlSocketAddress
{
	friend class DlSocket;

public:
	DlSocketAddress();
	~DlSocketAddress();

	DlSocketAddress( const DlSocketAddress &i_addr );
	void operator=( const DlSocketAddress &i_addr );

	bool ForConnect(
		const char *i_addr, const char *i_port, bool i_stream );

	bool ForConnect(
		const char *i_addr, int i_port, bool i_stream );

	/*
		Initialize the address for a service. If the port is to be chosen by
		the OS, give an empty string or a null pointer.
	*/
	bool ForService(
		const char *i_port, bool i_stream );

	bool ForService(
		int i_port, bool i_stream );

	bool IsStream() const;

	void Read( DlArchive *i_archive );
	void Write( DlArchive *i_archive ) const;

	/* Recommended buffer sizes for GetHostAndPort are 1025 and 32. */
	enum { kHostSize = 1025, kPortSize = 32 };

	bool GetHostAndPort(
		char *o_host, unsigned i_host_len,
		char *o_port, unsigned i_port_len,
		bool i_resolve = false ) const;

	bool GetNumericPort( int *o_port ) const;

	bool Valid() const { return m_sockaddr != 0x0; }

	/* Convert a numeric address string to a host name. */
	static bool NumericToHostname(
		const char *i_numeric, char *o_host, unsigned i_host_len );

	/* Portable way to get some special addresses. */
	static const char* Loopback();
	static const char* Broadcast();

private:
	void FreeAddress();

private:
	bool m_is_server;

	int m_family;
	int m_socktype;
	int m_protocol;

	int m_sockaddr_len;
	struct sockaddr *m_sockaddr;
};

#ifdef _WIN32
#	include <winsock2.h>
#	include <ws2tcpip.h>
	/* This include is to have getnameinfo on legacy systems (win2k). */
#	include <wspiapi.h>
	/* So we can build with older SDKs (on origin4). Should be removed. */
#	ifndef IPV6_V6ONLY
#		define IPV6_V6ONLY 27
#	endif
#else
#	include <sys/types.h>
#	include <sys/socket.h>
#	include <netdb.h>
#	include <unistd.h>
#	define SOCKET int
#	define INVALID_SOCKET -1
#	define closesocket close
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

DlSocketAddress::DlSocketAddress()
:
	m_is_server(false),
	m_family(0),
	m_socktype(0),
	m_protocol(0),
	m_sockaddr_len(0),
	m_sockaddr(0x0)
{
}

DlSocketAddress::~DlSocketAddress()
{
	FreeAddress();
}

DlSocketAddress::DlSocketAddress( const DlSocketAddress &i_addr )
:
	m_is_server(false),
	m_family(0),
	m_socktype(0),
	m_protocol(0),
	m_sockaddr_len(0),
	m_sockaddr(0x0)
{
	*this = i_addr;
}

void DlSocketAddress::operator=( const DlSocketAddress &i_addr )
{
	m_is_server = i_addr.m_is_server;

	m_family = i_addr.m_family;
	m_socktype = i_addr.m_socktype;
	m_protocol = i_addr.m_protocol;

	m_sockaddr_len = i_addr.m_sockaddr_len;
	free( m_sockaddr );
	if( i_addr.m_sockaddr )
	{
		m_sockaddr = (struct sockaddr*) malloc( m_sockaddr_len );
		memcpy( m_sockaddr, i_addr.m_sockaddr, m_sockaddr_len );
	}
}

void DlSocketAddress::FreeAddress()
{
	m_is_server = false;
	m_sockaddr_len = 0;
	free( m_sockaddr );
	m_sockaddr = 0x0;
}

/*
	ForConnect

	Initializes an address for use in connecting to another machine.
*/
bool DlSocketAddress::ForConnect(
	const char *i_addr,
	const char *i_port,
	bool i_stream )
{
	FreeAddress();

	struct addrinfo hints, *res;

	memset( &hints, 0, sizeof(hints) );
	hints.ai_flags = 0;
	/*
		Try IPv4 first so we are able to connect to IPv4 only services. This
		will also work with a service listening for both protocols. For an
		IPv6 only service, it is expected that we'll get a IPv6 address or a
		domain name which doesn't have an IPv4 address. Otherwise there's no
		good reason for the service not to listen on IPv4 too. This is dealt
		with below.
	*/
	hints.ai_family = PF_INET;
	hints.ai_socktype = i_stream ? SOCK_STREAM : SOCK_DGRAM;

	if( 0 != getaddrinfo( i_addr, i_port, &hints, &res ) )
	{
		/* Couldn't resolve as IPv4. Try IPv6. */
		hints.ai_family = PF_INET6;
		if( 0 != getaddrinfo( i_addr, i_port, &hints, &res ) )
			return false;
	}

	m_family = res->ai_family;
	m_socktype = res->ai_socktype;
	m_protocol = res->ai_protocol;

	m_sockaddr_len = res->ai_addrlen;
	m_sockaddr = (struct sockaddr*) malloc( m_sockaddr_len );
	memcpy( m_sockaddr, res->ai_addr, m_sockaddr_len );

	freeaddrinfo( res );

	return true;
}

bool DlSocketAddress::ForConnect(
	const char *i_addr, int i_port, bool i_stream )
{
	char buf[64];
	sprintf( buf, "%d", i_port );

	return ForConnect( i_addr, buf, i_stream );
}

/*
	ForService

	Initializes an address for use as a service (ie. bind()).
*/
bool DlSocketAddress::ForService(
	const char *i_port,
	bool i_stream )
{
	FreeAddress();

	m_is_server = true;

	/*
		Deal with a request to get a port from the OS. Linux likes an empty
		string but OS X requires a 0.
	*/
	if( !i_port || !i_port[0] )
		i_port = "0";

	struct addrinfo hints, *res, *res_to_free;

	memset( &hints, 0, sizeof(hints) );
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = i_stream ? SOCK_STREAM : SOCK_DGRAM;
	//hints.ai_protocol = i_stream ? 6 : 17; /* 6 = TCP, 17 = UDP */

	if( 0 != getaddrinfo( 0x0, i_port, &hints, &res ) )
		return false;

	/*
		If several addresses are returned, it is likely there is one AF_INET6
		and one AF_INET. Because it's possible for some configurations to
		return an AF_INET6 address while IPv6 is not really supported (origin1
		does this), we try to create a socket with each returned address and
		use the first which succeeds.
	*/
	res_to_free = res;
	if( res->ai_next )
	{
		for( struct addrinfo *it = res; it != 0x0; it = it->ai_next )
		{
			SOCKET s =
				socket( it->ai_family, it->ai_socktype, it->ai_protocol );

			if( s == INVALID_SOCKET )
				continue;
#ifdef _WIN32
			/*
				If this is an IPv6 address, make sure this version of windows
				supports the IPV6_V6ONLY option or we'll be stuck with a
				v6-only socket. This isn't needed on other operating systems
				because they have had a proper dual stack from the start.
			*/
			if( it->ai_family == AF_INET6 )
			{
				int off = 0;
				int ret = setsockopt(
					s, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&off, sizeof(off) );
				if( ret != 0 )
					continue;
			}
#endif
			closesocket( s );
			res = it;
			break;
		}
	}

	m_family = res->ai_family;
	m_socktype = res->ai_socktype;
	m_protocol = res->ai_protocol;

	m_sockaddr_len = res->ai_addrlen;
	m_sockaddr = (struct sockaddr*) malloc( m_sockaddr_len );
	memcpy( m_sockaddr, res->ai_addr, m_sockaddr_len );

	freeaddrinfo( res_to_free );

	return true;
}

bool DlSocketAddress::ForService( int i_port, bool i_stream )
{
	char buf[64];
	sprintf( buf, "%d", i_port );

	return ForService( i_port == 0 ? 0x0 : buf, i_stream );
}

bool DlSocketAddress::IsStream() const
{
	return m_socktype == SOCK_STREAM;
}

/*
	Read

	Reads the address from an archive.
*/
void DlSocketAddress::Read( DlArchive *i_archive )
{
	bool stream = i_archive->ReadChar();
	const char *host = i_archive->ReadString();
	const char *service = i_archive->ReadString();

	ForConnect( host, service, stream );
}

/*
	Write

	Writes this address to an archive.

	NOTES
	- For a 'service' address, we write the hostname instead of the unbound
	address which is what this class really represents.
*/
void DlSocketAddress::Write( DlArchive *i_archive ) const
{
	char host[kHostSize], service[kPortSize];
	host[0] = 0;
	service[0] = 0;

	GetHostAndPort( host, sizeof(host), service, sizeof(service) );

	i_archive->WriteChar( IsStream() ? 1 : 0 );
	i_archive->WriteString( host );
	i_archive->WriteString( service );
}

bool DlSocketAddress::GetHostAndPort(
	char *o_host, unsigned i_host_len,
	char *o_port, unsigned i_port_len,
	bool i_resolve ) const
{
	bool status = false;
	if( o_host != 0x0 && i_host_len != 0 )
		o_host[0] = 0;
	if( o_port != 0x0 && i_port_len != 0 )
		o_port[0] = 0;

	if( m_sockaddr )
	{
		int numeric_flag =
			i_resolve ? NI_NAMEREQD : (NI_NUMERICHOST | NI_NUMERICSERV);

		status = 0 == getnameinfo(
			m_sockaddr, m_sockaddr_len,
			o_host, i_host_len,
			o_port, i_port_len,
			numeric_flag | (IsStream() ? 0 : NI_DGRAM) );
	}

	if( m_is_server )
	{
		if( 0 != gethostname( o_host, i_host_len ) )
			status = false;
	}

	return status;
}

bool DlSocketAddress::GetNumericPort( int *o_port ) const
{
	char host[kHostSize], service[kPortSize];
	host[0] = 0;
	service[0] = 0;

	if( !GetHostAndPort( host, sizeof(host), service, sizeof(service) ) )
		return false;

	*o_port = atoi( service );
	return true;
}

/*
	NumericToHostname

	Converts a numeric address string (eg. "10.10.10.10") to a hostname (eg.
	"crappyserver"). Includes ugly hack to deal with IPv6 mapped IPv4 addresses
	which don't have the proper records to find the hostname the 'natural' way.
*/
bool DlSocketAddress::NumericToHostname(
	const char *i_numeric, char *o_host, unsigned i_host_len )
{
	DlSocketAddress addr;
	if( !addr.ForConnect( i_numeric, 1111 /*unused*/, true ) )
		return false;

	char dummy[kHostSize];

	if( addr.GetHostAndPort( o_host, i_host_len, dummy, sizeof(dummy), true ) )
		return true;

	const char *mapped = "::ffff:";
	if( 0 == strncmp( i_numeric, mapped, strlen(mapped) ) )
	{
		return NumericToHostname(
				i_numeric + strlen(mapped), o_host, i_host_len );
	}

	return false;
}

const char* DlSocketAddress::Loopback()
{
	/* Will be "::1" with IPv6. */
	return "127.0.0.1";
}

const char* DlSocketAddress::Broadcast()
{
	/* Will be "ff02::1" with IPv6. */
	return "255.255.255.255";
}

//--------------DlSocket---------------------

#ifdef _WIN32
#	include <winsock2.h>
#	include <ws2tcpip.h>
#	include <stdio.h>
#	include <io.h>
typedef int socklen_t;
	/* So we can build with older SDKs (on origin4). Should be removed. */
#	ifndef IPV6_V6ONLY
#		define IPV6_V6ONLY 27
#	endif
#	define write _write
#else
#	include <unistd.h>
#	include <sys/types.h>
#	include <sys/socket.h>
#	include <netinet/tcp.h>
#	include <netinet/in.h>
#	define SOCKET int
#	define INVALID_SOCKET -1
#	define closesocket close
#endif

#include <stdlib.h>
#include <string.h>
#include <stdint.h>


/*
	TODO:
	- getpeername()
	- windows ioctlsocket() for non blocking I/O (maybe)
*/

DlSocket::DlSocket()
:
	m_fd(INVALID_SOCKET),
	m_addr(0x0)
{
}

DlSocket::~DlSocket()
{
	Close();
}

bool DlSocket::Init( const DlSocketAddress &i_addr )
{
	assert( m_fd == INVALID_SOCKET );
	assert( !m_addr );

	SOCKET fd = socket( i_addr.m_family, i_addr.m_socktype, i_addr.m_protocol );

	if( fd == INVALID_SOCKET )
	{
		m_error.Record();
		return false;
	}
#ifdef _WIN32
	/* Read the comment about IPV6_V6ONLY in dlSocketAddress.cpp */
	if( i_addr.m_is_server && i_addr.m_family == AF_INET6 )
	{
		int off = 0;
		setsockopt( fd, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&off, sizeof(off) );
	}
#endif
	m_fd = fd;
	m_addr = new DlSocketAddress( i_addr );

	return true;
}

bool DlSocket::Bind()
{
	if( !Valid() )
		return false;

	int ret = bind( m_fd, m_addr->m_sockaddr, m_addr->m_sockaddr_len );
	m_error.RecordIf( ret != 0 );

	/*
		Retrieve bound address and port.

		NOTE: This may not return a valid address on a multi-homed host. In
		that case, we should probably use the canonical name from
		getaddrinfo...
	*/
	getsockname(
		m_fd, m_addr->m_sockaddr, (socklen_t*)&m_addr->m_sockaddr_len );

	compile_assert( sizeof(socklen_t) == sizeof(m_addr->m_sockaddr_len) );

	return ret == 0;
}

bool DlSocket::Listen()
{
	if( !Valid() )
		return false;

	int ret = listen( m_fd, SOMAXCONN );

	return ret == 0;
}

bool DlSocket::Accept( DlSocket *o_socket )
{
	if( !Valid() || !o_socket || o_socket->Valid() )
		return false;

	DlSocketAddress *new_addr = new DlSocketAddress( *m_addr );

	SOCKET fd = accept(
		m_fd, new_addr->m_sockaddr, (socklen_t*)&new_addr->m_sockaddr_len );

	compile_assert( sizeof(socklen_t) == sizeof(m_addr->m_sockaddr_len) );

	if( fd == INVALID_SOCKET )
	{
		m_error.Record();
		delete new_addr;
		return false;
	}

#ifdef __APPLE__
	/* Turn off SIGPIPE on OS X. Done in send() on linux. */
	int on = 1;
	setsockopt( fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on) );
#endif

	o_socket->m_fd = fd;
	o_socket->m_addr = new_addr;
	o_socket->m_addr->m_is_server = false;
	return true;
}

bool DlSocket::Connect()
{
	if( !Valid() )
		return false;

	int ret = connect( m_fd, m_addr->m_sockaddr, m_addr->m_sockaddr_len );
	m_error.RecordIf( ret != 0 );

#ifdef __APPLE__
	/* Turn off SIGPIPE on OS X. Done in send() on linux. */
	int on = 1;
	if( ret == 0 )
		setsockopt( m_fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on) );
#endif

	return ret == 0;
}

#if defined(__linux__)
#if !defined(__INTEL_COMPILER)
#	include <stdlib.h>
#else
#	include <alloca.h>
#endif
#endif

#ifdef __APPLE__
#include <stdlib.h>
#endif

#ifdef _WIN32
#include <malloc.h>
#endif

/*
	A macro to make alloca usage new-like by hiding sizeof and the cast.
*/
#define DlAlloca(type, size) ((type *) alloca((size) * sizeof(type)))

bool DlSocket::Send( const DlArchive &i_data )
{
	if( !Valid() )
		return false;

	if( m_addr->IsStream() )
	{
		dl_uint32 size = i_data.size();
		ENDIAN_TO_MSB( size );

#if 1
		/*
			Send a single buffer with size + data to avoid the round-trip
			delay induced by the Nagle algorithm.
		*/
		char *heap_buf = 0x0, *buf;
		unsigned total_size = i_data.size() + sizeof(size);

		if( total_size > 0*1024 )
		{
			heap_buf = buf = (char*) malloc( total_size );
		}
		else
		{
			buf = DlAlloca( char, total_size );
		}

		memcpy( buf, &size, sizeof(size) );
		memcpy( buf + sizeof(size), i_data.RawData(), i_data.size() );

		bool ret = Send( buf, total_size );

		free( heap_buf );
		return ret;
#else
		/* Could improve performance on Linux with MSG_MORE. On OS X, there is
		   MSG_HOLD and MSG_SEND. But on Windows there isn't much. */
		return
			Send( &size, sizeof(size) ) &&
			Send( i_data.RawData(), i_data.size() );
#endif
	}
	else
	{
		// We don't bother sending the size with UDP packets. It's useless.
		return Send( i_data.RawData(), i_data.size() );
	}
}

bool DlSocket::Receive( DlArchive *o_data )
{
	if( !Valid() )
		return false;

	if( m_addr->IsStream() )
	{
		dl_uint32 size;
		if( !Receive( &size, sizeof(size) ) )
			return false;

		ENDIAN_FROM_MSB( size );
		void *buf = malloc(size);

		if( !Receive( buf, size ) )
		{
			free( buf );
			return false;
		}

		o_data->WriteBytes( size, buf );
		free( buf );
	}
	else
	{
		// TODO: recvfrom
		return false;
	}

	return true;
}

/**
	\brief reads the socket and writes resul into o_fd file. Does this until
	nothing to read from the socket.
*/
bool DlSocket::Receive( int o_fd )
{
	if( !Valid() )
		return false;

	if( !m_addr->IsStream() )
		return false;

	const size_t k_buffer_size = 1024;
	char buffer[k_buffer_size];

	intptr_t r;
	while( (r=recv(m_fd, buffer, k_buffer_size, 0)) > 0 )
	{
		intptr_t ret = write( o_fd, buffer, r );
		if( ret != r )
			return false;
	}

	return true;
}

/**
	\brief writes into the socket from a given file descriptor.
*/
bool DlSocket::Send( int i_fd )
{
	if( !Valid() )
		return false;

	if( !m_addr->IsStream() )
		return false;

	const size_t k_buffer_size = 4096;
	char buffer[k_buffer_size];

	intptr_t r;
	while( (r=read(i_fd, buffer, k_buffer_size)) > 0 )
	{
		int flags = 0;
#if !defined(_WIN32) && !defined(__APPLE__)
		/* Request that there be no SIGPIPE. OS X uses a socket option. */
		flags = MSG_NOSIGNAL;
#endif
		int ret	= send( m_fd, buffer, r, flags );

		if( ret == -1 )
		{
			m_error.Record();
			return false;
		}
	}

	return true;
}


bool DlSocket::Send( const void *i_data, unsigned i_len )
{
	if( !Valid() )
		return false;

	const char *data = (const char*)i_data;
	if( m_addr->IsStream() )
	{
		while( i_len != 0 )
		{
			int flags = 0;
#if !defined(_WIN32) && !defined(__APPLE__)
			/* Request that there be no SIGPIPE. OS X uses a socket option. */
			flags = MSG_NOSIGNAL;
#endif
			int ret = send( m_fd, data, i_len, flags );

			if( ret == -1 )
			{
				m_error.Record();
				return false;
			}

			i_len -= ret;
			data += ret;
		}
	}
	else
	{
		return SendTo( i_data, i_len, *m_addr );
	}

	return true;
}

bool DlSocket::Receive( void *o_data, unsigned i_len )
{
	if( !Valid() )
		return false;

	int to_read = i_len;
	char *b = (char*)o_data;
	while( to_read != 0 )
	{
		int ret = recv( m_fd, b, to_read, 0 );

		if( ret == -1 || ret == 0 )
		{
			m_error.Record();
			return false;
		}

		to_read -= ret;
		b += ret;
	}

	return true;
}

unsigned DlSocket::ReceivePartial( void *o_data, unsigned i_len )
{
	if( !Valid() )
		return 0;

	if( i_len == 0 )
		return 0;

	int ret = recv( m_fd, (char*)o_data, i_len, 0 );

	if( ret == -1 || ret == 0 )
	{
		m_error.Record();
		return 0;
	}

	return ret;
}

/*
	ReceiveFrom

	Thin wrapper around the recvfrom syscall.

	PARAMETERS
	o_data
		A buffer of length *io_len to put the received data in.
	io_len
		On entry, *io_len is the size of o_data. On exit, it is set the number
		of bytes received if the reception was successful.
	o_addr
		Will be set to the address the data came from. May be modified even if
		there is an error.

	RETURNS
		true if some data was received, false on error.
*/
bool DlSocket::ReceiveFrom(
	void *o_data,
	unsigned *io_len,
	DlSocketAddress *o_addr )
{
	if( !Valid() )
		return false;

	*o_addr = *m_addr;

	int ret = recvfrom(
		m_fd, (char*)o_data, *io_len, 0, 
		o_addr->m_sockaddr, (socklen_t*)&o_addr->m_sockaddr_len );

	if( ret <= 0 )
	{
		m_error.Record();
		return false;
	}

	o_addr->m_is_server = false;

	*io_len = ret;
	return true;
}

/*
	Thin wrapper around the sendto() syscall.

	If the parameters aren't obvious, you shouldn't be writing network code.
*/
bool DlSocket::SendTo(
	const void *i_data,
	unsigned i_len,
	DlSocketAddress &i_addr )
{
	int ret = sendto(
		m_fd, (const char*)i_data, i_len, 0, 
		i_addr.m_sockaddr, i_addr.m_sockaddr_len );

	if( ret < 0 || ret != int(i_len) )
	{
		m_error.Record();
		return false;
	}

	return true;
}

/*
	Close() will normally shutdown a connection correctly, as far as I can
	tell. Unless the fd was duplicated (dup(), fork(), etc), in which case the
	connection will remain open. Shutdown() will force it to close even if
	there are still open fds to the socket.
*/
void DlSocket::Shutdown()
{
	if( !Valid() )
		return;

	/*
		This seems necessary to wake up any stalling recv/read  that might
		be waiting for data. Without this, the recv/read might stay stuck
		untel the other end closes the connection.
	*/
#ifdef _WIN32
	::shutdown( m_fd, SD_BOTH );
#else
	::shutdown( m_fd, SHUT_RDWR );
#endif
}

void DlSocket::Close()
{
	if( !Valid() )
		return;

	closesocket( m_fd );

	m_fd = INVALID_SOCKET;

	delete m_addr;
	m_addr = 0x0;
}

bool DlSocket::Valid() const
{
	return m_fd != INVALID_SOCKET;
}

bool DlSocket::SetOption( eOption i_option, bool i_state )
{
	int state = i_state ? 1 : 0;
	const char *opt = (const char*)&state;
	socklen_t optlen = sizeof(state);

	bool ok = false;

	switch( i_option )
	{
		case k_REUSEADDR:
			ok = 0 == setsockopt( m_fd, SOL_SOCKET, SO_REUSEADDR, opt, optlen );
		break;
		case k_REUSEPORT:
#ifdef _WIN32
			ok = 0 == setsockopt( m_fd, SOL_SOCKET, SO_REUSEADDR, opt, optlen );
#else
			ok = 0 == setsockopt( m_fd, SOL_SOCKET, SO_REUSEPORT, opt, optlen );
#endif
		break;
		case k_BROADCAST:
			ok = 0 == setsockopt( m_fd, SOL_SOCKET, SO_BROADCAST, opt, optlen );
		break;
		case k_NODELAY:
			ok = 0 == setsockopt( m_fd, IPPROTO_TCP, TCP_NODELAY, opt, optlen );
		break;
	}

	m_error.RecordIf( !ok );
	return ok;
}

bool DlSocket::GetLocalAddress( DlSocketAddress *o_addr )
{
	if( !Valid() || m_addr->m_is_server )
		return false;
	
	*o_addr = *m_addr;

	int ret = getsockname(
		m_fd, o_addr->m_sockaddr, (socklen_t*)&o_addr->m_sockaddr_len );

	if( ret != 0 )
	{
		m_error.Record();
		*o_addr = DlSocketAddress();
		return false;
	}

	return true;
}

//--------------DlFDSet---------------------
/*
	Wrapper around select() and fd sets to hide the minor differences between
	operating systems.

	The unix implementation actually uses poll() to avoid the 1024 fd per
	process limit. We've kept the select()-like interface because it's
	convenient and we have lots of code using it.

	Note that windows has no such limit because its fd_set implementation is an
	array of file descriptors. You can't give it more than 1024 fds *at once*
	but it doesn't matter how many are open in the process. This is much more
	sane for once.
*/
class DlFDSet
{
	/* Not implemented but there's no reason it couldn't be. */
	DlFDSet( const DlFDSet& );
	void operator=( const DlFDSet& );

public:
	DlFDSet();
	~DlFDSet();

	bool Set( int i_fd );
	void Unset( int i_fd );
	bool IsSet( int i_fd );
	void clear();

	bool Set( const DlSocket *i_socket );
	void Unset( const DlSocket *i_socket );
	bool IsSet( const DlSocket *i_socket );

	/* Might return false if not all fds could fit. */
	bool Overflow() const { return m_overflow; }

	/*
		Wrapper around OS select().

		PARAMETERS
		i_read, i_write and i_except are like select.
		i_timeout_seconds : number of seconds before timeout (-1 for infinite)
		i_timeout_microseconds : number of microseconds added to timeout

		RETURNS
		-1 on error or signal. 0 if we timed out. Otherwise the number of FDs
		on which something happened.
	*/
	static int Select(
		DlFDSet *i_read,
		DlFDSet *i_write,
		DlFDSet *i_except,
		long i_timeout_seconds = -1,
		long i_timeout_microseconds = 0 );

private:
	void GrowSet( int i_fd );

private:
	/* Max fd in the set on unix, number of fds on windows. */
	int m_max_fd;

	/* Number of allocated words for the set on unix. */
	unsigned m_allocated;

	/* Set to true if we exceed the number of allowed fds. */
	bool m_overflow;

	/* The fd_set structure. */
	void *m_fd_set;
};


#ifdef _WIN32
#	define FD_SETSIZE 1024
#	include <winsock2.h>
#else
#	include <poll.h>
#endif

#include <stdlib.h>
#include <string.h>

/* Bit set manipulation macros. */

#define WORD_BITS (sizeof(dl_uintptr) * 8u)

#define MASK( __idx ) (dl_uintptr(1) << (unsigned(__idx) % WORD_BITS))

#define BIT_SET( _ptr, _idx ) do { \
	((dl_uintptr*)(_ptr))[unsigned(_idx)/WORD_BITS] |= MASK(_idx); \
	} while( false )

#define BIT_RESET( _ptr, _idx ) do { \
	((dl_uintptr*)(_ptr))[unsigned(_idx)/WORD_BITS] &= ~MASK(_idx); \
	} while( false )

#define BIT_TEST( _ptr, _idx ) \
	((((dl_uintptr*)(_ptr))[unsigned(_idx)/WORD_BITS] & MASK(_idx)) != 0 )

DlFDSet::DlFDSet()
:
	m_max_fd(0),
	m_allocated(0)
{
#ifdef _WIN32
	m_fd_set = malloc( sizeof(fd_set) );
#else
	m_fd_set = calloc( 1, sizeof(dl_uintptr) * 4 );
	m_allocated = 4;
#endif
	clear();
}

DlFDSet::~DlFDSet()
{
	free( m_fd_set );
}

bool DlFDSet::Set( int i_fd )
{
	if( i_fd < 0 )
	{
		assert( false );
		return false;
	}

#ifdef _WIN32
	/*
		The FD_SET macro does not report success/error so we check if the set
		is full beforehand. If it is, we report an error unless i_fd is already
		in the set.
	*/
	if( ((fd_set*)m_fd_set)->fd_count >= FD_SETSIZE )
	{
		if( IsSet( i_fd ) )
			return true;

		m_overflow = true;
		return false;
	}

	FD_SET( i_fd, (fd_set*)m_fd_set );
#else
	if( i_fd > m_max_fd )
	{
		m_max_fd = i_fd;
		GrowSet( m_max_fd );
	}

	BIT_SET( m_fd_set, i_fd );
#endif

	return true;
}

void DlFDSet::Unset( int i_fd )
{
	if( i_fd < 0 )
	{
		assert( false );
		return;
	}

#ifdef _WIN32
	FD_CLR( i_fd, (fd_set*)m_fd_set );
#else
	if( i_fd <= m_max_fd )
	{
		BIT_RESET( m_fd_set, i_fd );
	}
#endif
}

bool DlFDSet::IsSet( int i_fd )
{
	if( i_fd < 0 )
		return false;

#ifdef _WIN32
	return FD_ISSET( i_fd, (fd_set*)m_fd_set );
#else
	if( i_fd > m_max_fd )
		return false;

	return BIT_TEST( m_fd_set, i_fd );
#endif
}

void DlFDSet::clear()
{
	m_overflow = false;
#ifdef _WIN32
	FD_ZERO( (fd_set*)m_fd_set );
#else
	memset( m_fd_set, 0, sizeof(dl_uintptr) * m_allocated );
#endif
}

bool DlFDSet::Set( const DlSocket *i_socket )
{
	if( !i_socket->Valid() )
		return false;

	return Set( i_socket->m_fd );
}

void DlFDSet::Unset( const DlSocket *i_socket )
{
	if( !i_socket->Valid() )
		return;

	Unset( i_socket->m_fd );
}

bool DlFDSet::IsSet( const DlSocket *i_socket )
{
	if( !i_socket->Valid() )
		return false;

	return IsSet( i_socket->m_fd );
}

void DlFDSet::GrowSet( int i_fd )
{
	if( i_fd < 0 )
		return;

	unsigned needed = (unsigned(i_fd + 1) + WORD_BITS - 1u) / WORD_BITS;
	if( needed <= m_allocated )
		return;

	if( needed < m_allocated + m_allocated / 2 )
		needed = m_allocated + m_allocated / 2;

	m_fd_set = realloc( m_fd_set, sizeof(dl_uintptr) * needed );

	memset(
		(char*)m_fd_set + sizeof(dl_uintptr) * m_allocated,
		0, sizeof(dl_uintptr) * (needed - m_allocated) );

	m_allocated = needed;
}

int DlFDSet::Select(
	DlFDSet *i_read,
	DlFDSet *i_write,
	DlFDSet *i_except,
	long i_timeout_seconds,
	long i_timeout_microseconds )
{
#ifdef _WIN32
	struct timeval tv;
	tv.tv_sec = i_timeout_seconds;
	tv.tv_usec = i_timeout_microseconds;

	int ret = select(
			0, /* ignored */
			i_read ? (fd_set*)i_read->m_fd_set : 0x0,
			i_write ? (fd_set*)i_write->m_fd_set : 0x0,
			i_except ? (fd_set*)i_except->m_fd_set : 0x0,
			i_timeout_seconds >= 0 ? &tv : 0x0 );

	if( ret == SOCKET_ERROR )
	{
		/* Error (or signal). Clear the sets to make use easier (the linux man
		   page says their content is now undefined). */
		if( i_read )
			i_read->clear();
		if( i_write )
			i_write->clear();
		if( i_except )
			i_except->clear();

		/* Make sure we return '-1' on all systems. */
		ret = -1;
	}

	return ret;
#else
	std::vector<pollfd> fds;

	dl_uintptr *b_read = i_read ? (dl_uintptr*)i_read->m_fd_set : 0x0;
	dl_uintptr *b_write = i_write ? (dl_uintptr*)i_write->m_fd_set : 0x0;
	dl_uintptr *b_except = i_except ? (dl_uintptr*)i_except->m_fd_set : 0x0;

	unsigned len_read = i_read ? i_read->m_allocated : 0;
	unsigned len_write = i_write ? i_write->m_allocated : 0;
	unsigned len_except = i_except ? i_except->m_allocated : 0;

	unsigned max_len = len_read;
	if( len_write > max_len )
		max_len = len_write;
	if( len_except > max_len )
		max_len = len_except;

	for( unsigned j = 0; j < max_len; ++j )
	{
		dl_uintptr w_read = 0, w_write = 0, w_except = 0;
		if( j < len_read )
			w_read = b_read[j];
		if( j < len_write )
			w_write = b_write[j];
		if( j < len_except )
			w_except = b_except[j];

		if( (w_read | w_write | w_except) == 0 )
			continue;

		int fd = j * WORD_BITS;
		for( unsigned i = 0; i < WORD_BITS; ++i, ++fd )
		{
			if( ((w_read | w_write | w_except) & 1) != 0 )
			{
				pollfd pfd;
				pfd.fd = fd;
				pfd.events = 0;
				pfd.revents = 0;

				if( (w_read & 1) != 0 )
					pfd.events |= POLLIN;
				if( (w_write & 1) != 0 )
					pfd.events |= POLLOUT;

				fds.push_back( pfd );
			}

			w_read >>= 1;
			w_write >>= 1;
			w_except >>= 1;
		}
	}

	int timeout = -1;
	if( i_timeout_seconds >= 0 )
	{
		timeout = i_timeout_seconds * 1000 + i_timeout_microseconds / 1000;
	}

	int ret = poll( &fds[0], fds.size(), timeout );

	if( i_read )
		i_read->clear();
	if( i_write )
		i_write->clear();

	if( ret == -1 )
	{
		if( i_except )
			i_except->clear();

		return -1;
	}

	int bitcount = 0;
	for( unsigned i = 0; i < fds.size(); ++i )
	{
		int fd = fds[i].fd;
		short revent = fds[i].revents;

		if( i_read && (revent & POLLIN) != 0 )
		{
			++bitcount;
			i_read->Set( fd );
		}

		if( i_write && (revent & POLLOUT) != 0 )
		{
			++bitcount;
			i_write->Set( fd );
		}

		/*
			except is handled differently because poll reports errors for all
			fds while select allows us to specify for which we want the errors.
		*/
		if( i_except )
		{
			if( i_except->IsSet( fd ) )
			{
				if( (revent & POLLERR) != 0 )
				{
					++bitcount;
				}
				else
				{
					i_except->Unset( fd );
				}
			}
		}
	}

	return bitcount;
#endif
}


//--------------JSonServer---------------------

#include "pxr/base/js/json.h"

#include <stdarg.h>

/*
	This is the network loop that accepts and processes connections

	We spawn a thread for each incoming connection.
*/
void JSonServer::JSonServiceLoop( void *i_data )
{
	if( !i_data )
	{
		assert( false );
		return;
	}

	JSonServer &server = *(JSonServer *)i_data;

	/* Buffers to retrieve printable addresses .*/
	char client_host[DlSocketAddress::kHostSize];
	char client_port[DlSocketAddress::kPortSize];

	server.Log( e_debug, "Entering service loop ...\n" );

	/* Enter the message processing loop */
	for( ;; )
	{
		DlFDSet fd_vars;
		fd_vars.Set( &server.m_socket );
		fd_vars.Set( &server.m_socket_udp );

		server.Log( e_debug, "Entering select.\n" );

		/* Set a timeout on windows so the service can be stopped. */
		int bitcount = DlFDSet::Select( &fd_vars, 0x0, 0x0
#ifdef _WIN32
			, 5
#endif
			);

		server.Log(
			e_debug,
			"Returned from select with a bitcount of %d.\n",
			bitcount );

		if( fd_vars.IsSet( &server.m_socket_udp) )
		{
			unsigned len = 1500;
			char data[1500];
			DlSocketAddress address;
			if( server.m_socket_udp.ReceiveFrom(data, &len, &address) )
			{
				assert( len != 1500 ); /* too close for comfort!! */
				if( len<1500 )
					server.Datagram( address, data, len );
			}
			/* NOTE: Don't "continue" in here, as we can have TCP socket too */
		}

		/* Check if we have a new TCP connection. */
		if( fd_vars.IsSet( &server.m_socket ) )
		{
			/* We have a new client, accept() it. */
			DlSocket *client_sock = new DlSocket;
			if( server.m_socket.Accept( client_sock ) )
			{
				client_sock->Address()->GetHostAndPort(
						client_host, sizeof(client_host),
						client_port, sizeof(client_port) );

				server.Log( e_debug, "accepted connection from %s\n", client_host );

#ifdef JSONSERVER_ENABLE_BROADCAST
				server.m_mutex.Lock();
				server.m_accepted_clients.push_back( client_sock );
				server.m_mutex.Unlock();
#endif
			}
			else
			{
				/*
					Can happen if client dies before binding.
				*/
				server.Log( e_log,
					"unable to establish connection with client (%s).\n",
					strerror(errno) );

				delete client_sock;
				continue;
			}

//			std::thread *new_client = new std::thread;
//			new_client->detach();

			ConnectionData * cd = new ConnectionData;
			cd->m_server = &server;
			cd->m_socket = client_sock;

			std::thread *new_client = new std::thread( ClientConnection, cd );
			new_client->detach();

//			new_client->Start( ClientConnection, cd );
		}
	}
}

bool JSonServer::Start( int i_port )
{
	if( m_init.Status() != 0 )
	{
		return false;
	}

	DlSocketAddress tcp_serv_addr;
	DlSocketAddress udp_serv_addr;

    if( !tcp_serv_addr.ForService( i_port, true /* stream */ ) ||
    	!udp_serv_addr.ForService( i_port, false /* datagram */ ) ||
        !m_socket.Init( tcp_serv_addr ) ||
        !m_socket_udp.Init( udp_serv_addr ) )
	{
		return false;
	}

 	/*
		Make the socket reusable to the server can be restarted quickly.
		FOR UDP, it is useful for multicast support. So put it just in case.
	*/
    if( !m_socket.SetOption( DlSocket::k_REUSEADDR, true ) ||
    	!m_socket_udp.SetOption( DlSocket::k_REUSEADDR, true ) )
    {
        Log( e_debug,
           "Unable to set socket parameters (%s).\n", strerror(errno) );
    }

	if( !m_socket.Bind() || !m_socket_udp.Bind() )
	{
		return false;
	}

    /*
		Listen for incoming connections on the TCP port. This is not needed
		for our UDP port.
	*/
    if( !m_socket.Listen() )
    {
		return false;
    }

//	m_server_thread = new DlThread;
//	m_server_thread->Start( JSonServiceLoop, this );
	m_server_thread = new std::thread( JSonServiceLoop, this );

	return true;
}

/*
	FIXME: I think we need a cleaner implementation
*/
void JSonServer::Stop( void )
{
//	m_server_thread->Cancel();
	delete m_server_thread;
}

/*
	Log
*/
void JSonServer::Log( LogType i_log_type, const char *i_msg, ... ) const
{
#ifndef VERBOSE
	if( i_log_type == e_debug )
		return;
#endif

	char theMessage[ 2048 ];

	va_list ap;
	va_start(ap, i_msg);

	vsnprintf( theMessage, sizeof(theMessage)-1, i_msg, ap );

	va_end( ap );

	LogString( i_log_type, theMessage );
}

/**
	\brief Sends a reply to the client.  It's important we always reply
	something to the client, if not, it could hang!
*/
bool JSonServer::Reply( DlSocket &i_client, const PXR_NS::JsObject& i_message ) const
{
	const char *empty = "{}";
//	const char *str = empty;
	std::string str = empty;

	PXR_NS::JsValue js_val(i_message);

//	if( !i_message )
	if( js_val.IsNull() )
	{
		Log( e_error, "Server::Reply received an empty message\n" );
		//DlDebug::StackTrace();
	}
	else
	{
//		str = json_dumps( i_message, JSON_COMPACT );
		str = PXR_NS::JsWriteToString(js_val);

	}

//	if( !str )
	if( str.empty() )
	{
		Log( e_error, "Server::Reply received an invalid message\n" );
		//DlDebug::StackTrace();
		str = empty;
	}

	DlArchive message;
//	message.WriteString( str );
	message.WriteString( str.c_str() );

//	if( str != empty )
//		free( (void *) str );

	return i_client.Send( message );
}

/*
	Starting point an incoming connection.
*/
void JSonServer::ClientConnection( void *i_data )
{
	const ConnectionData *cd =
		static_cast<const ConnectionData *>(i_data);

	DlSocket *socket = cd->m_socket;
	assert( socket );

	JSonServer &server = *cd->m_server;

	delete cd;

	/*
		Since we communicat mostly in messages, disable buffering.
	*/
	socket->SetOption( DlSocket::k_NODELAY, true );

	DlArchive archive;
	while(socket->Receive( &archive ))
	{
		const char *json_string = archive.ReadString();

//		json_error_t jerror;
//		json_t *json_object =
//			json_loads( json_string, JSON_REJECT_DUPLICATES, &jerror );
		std::string jsonString = json_string;
		PXR_NS::JsValue js_val = PXR_NS::JsParseString(jsonString);

//		bool exit = false;
//		if( json_object )
		if( js_val.IsObject() )
		{
			const PXR_NS::JsObject& js_obj = js_val.GetJsObject();
//			exit = server.ExecuteJSonCommand( socket, json_object);
			server.ExecuteJSonCommand(*socket, js_obj);
		}
		else
		{
#ifdef INVALID_JSON_TRACES
			char *tmpfile = DlSystem::NamedTemporaryFile( "studio_sync_" );

			server.Log( e_debug, "received invalid JSON object, dumping data to %s.\n", tmpfile );

			FILE *fh = ::fopen( tmpfile, "wb" );
			::fwrite( archive.RawData(), archive.size(), 1, fh );
			::fclose( fh );
#else
			server.Log( e_debug, "received invalid JSON object.\n" );
#endif
		}

//		json_decref( json_object );
#if 0
		if( exit )
		{
			/* User wants to handle this connection by itself. */
			return;
		}
#endif
	}

	server.ConnectionClosed( socket );

	socket->Close();
	delete socket;

	return;
}


jsonServer* jsonServer::m_instance = 0;

void
jsonServer::CleanUp()
{
	if(m_instance)
	{
		m_instance->Stop();
		delete m_instance;
		m_instance = 0;
	}
}

int
jsonServer::GetServerPort()
{
	if(m_instance || StartServer())
	{
		assert(m_instance);
		int port = -1;
		if(m_instance->Address().GetNumericPort(&port))
		{
			return port;
		}
	}

	return -1;
}

std::string
jsonServer::GetServerHost()
{
	if(m_instance || StartServer())
	{
		assert(m_instance);
		char host[DlSocketAddress::kHostSize] = "";
		char port[DlSocketAddress::kPortSize] = "";

		if(m_instance->Address().GetHostAndPort(host, sizeof(host), port, sizeof(port)))
		{
			return host;
		}
	}

	return "";
}

void
jsonServer::ExecuteJSonCommand(DlSocket &i_socket, const PXR_NS::JsObject& i_object)
{
	auto opObj = i_object.find("operation");
	if (opObj == i_object.end())
	{
		LogString(JSonServer::e_debug, "Key operation not found\n");
		return;
	}
	PXR_NS::JsValue js_val = opObj->second;
	if (!js_val.IsString())
	{
		LogString(JSonServer::e_debug, "Bad key operation\n");
		return;
	}

	std::string op = js_val.GetString();
	if (op == "select layer")
	{
		auto layerObj = i_object.find("layer");
		if (layerObj == i_object.end())
		{
			LogString(JSonServer::e_debug, "Key layer not found\n");
			return;
		}
		PXR_NS::JsValue layerVal = layerObj->second;
		if (!layerVal.IsObject())
		{
			LogString(JSonServer::e_debug, "Bad key layer\n");
			return;
		}

		const PXR_NS::JsObject fbObj = layerVal.GetJsObject();
		auto nameObj = fbObj.find("name");
		if (nameObj == fbObj.end())
		{
			LogString(JSonServer::e_debug, "Key name not found\n");
			return;
		}
		PXR_NS::JsValue nameVal = nameObj->second;
		if (!nameVal.IsString())
		{
			LogString(JSonServer::e_debug, "Bad key name\n");
			return;
		}
		
		OBJ_Node* obj_node =
			OPgetDirector()->findOBJNode(nameVal.GetString().c_str());
		assert(obj_node);

		obj_node->setEditPicked(1);
	}
	else if (op == "scale layer intensity")
	{
		auto layerObj = i_object.find("layer");
		if (layerObj == i_object.end())
		{
			LogString(JSonServer::e_debug, "Key layer not found\n");
			return;
		}
		PXR_NS::JsValue layerVal = layerObj->second;
		if (!layerVal.IsObject())
		{
			LogString(JSonServer::e_debug, "Bad key layer\n");
			return;
		}

		auto factorObj = i_object.find("scale factor");
		if (factorObj == i_object.end())
		{
			LogString(JSonServer::e_debug, "Key scale factor not found\n");
			return;
		}
		PXR_NS::JsValue factorVal = factorObj->second;
		if (!factorVal.IsInt() && !factorVal.IsReal())
		{
			LogString(JSonServer::e_debug, "Bad key scale factor\n");
			return;
		}
		fpreal factor;
		if (factorVal.IsInt()) factor = factorVal.GetInt();
		else factor = factorVal.GetReal();

		const PXR_NS::JsObject fbObj = layerVal.GetJsObject();
		auto valuesObj = fbObj.find("values");
		if (valuesObj == fbObj.end())
		{
			LogString(JSonServer::e_debug, "Key values not found\n");
			return;
		}
		PXR_NS::JsValue valuesVal = valuesObj->second;
		if (!valuesVal.IsArray())
		{
			LogString(JSonServer::e_debug, "Bad key values\n");
			return;
		}
		PXR_NS::JsArray values = valuesVal.GetJsArray();

		fpreal newValue;
		for (unsigned i = 0; i < values.size(); i++)
		{
			PXR_NS::JsValue value = values[i];
			assert(!value.IsNull());
			if (!value.IsInt() && !value.IsReal())
			{
				LogString(JSonServer::e_debug, "Bad value in values\n");
				return;
			}
			if (value.IsInt())
			{
				newValue = value.GetInt() * factor;
			}
			else
			{
				newValue = value.GetReal() * factor;
			}
		}

		auto nameObj = fbObj.find("name");
		if (nameObj == fbObj.end())
		{
			LogString(JSonServer::e_debug, "Key name not found\n");
			return;
		}
		PXR_NS::JsValue nameVal = nameObj->second;
		if (!nameVal.IsString())
		{
			LogString(JSonServer::e_debug, "Bad key name\n");
			return;
		}
		
		OBJ_Node* obj_node =
			OPgetDirector()->findOBJNode(nameVal.GetString().c_str());
		assert(obj_node);

		HOM_AutoLock hom_lock;
		obj_node->setParameterOrProperty("light_intensity", 0, 0, newValue);
	}
	else if (op == "update layer filter")
	{
		auto layerObj = i_object.find("layer");
		if (layerObj == i_object.end())
		{
			LogString(JSonServer::e_debug, "Key layer not found\n");
			return;
		}
		PXR_NS::JsValue layerVal = layerObj->second;
		if (!layerVal.IsObject())
		{
			LogString(JSonServer::e_debug, "Bad key layer\n");
			return;
		}

		auto colorMultObj = i_object.find("color multiplier");
		if (colorMultObj == i_object.end())
		{
			LogString(JSonServer::e_debug, "Key color multiplier not found\n");
			return;
		}
		PXR_NS::JsValue colorMultVal = colorMultObj->second;
		if (!colorMultVal.IsArray())
		{
			LogString(JSonServer::e_debug, "Bad key color multiplier\n");
			return;
		}
		PXR_NS::JsArray colMult = colorMultVal.GetJsArray();

		fpreal mult[3];
		for (unsigned i = 0; i < colMult.size(); i++)
		{
			PXR_NS::JsValue value = colMult[i];
			assert(!value.IsNull());
			if (!value.IsInt() && !value.IsReal())
			{
				LogString(JSonServer::e_debug, "Bad value in color multiplier\n");
				return;
			}
			if (value.IsInt())
			{
				mult[i] = value.GetInt();
			}
			else
			{
				mult[i] = value.GetReal();
			}
		}

		const PXR_NS::JsObject fbObj = layerVal.GetJsObject();
		auto colorValuesObj = fbObj.find("color_values");
		if (colorValuesObj == fbObj.end())
		{
			LogString(JSonServer::e_debug, "Key color_values not found\n");
			return;
		}
		PXR_NS::JsValue colorValuesVal = colorValuesObj->second;
		if (!colorValuesVal.IsArray())
		{
			LogString(JSonServer::e_debug, "Bad key color_values\n");
			return;
		}
		PXR_NS::JsArray colorValues = colorValuesVal.GetJsArray();

		fpreal newValues[3];
		for (unsigned i = 0; i < colorValues.size(); i++)
		{
			PXR_NS::JsValue value = colorValues[i];
			assert(!value.IsNull());
			if (!value.IsInt() && !value.IsReal())
			{
				LogString(JSonServer::e_debug, "Bad value in color_values\n");
				return;
			}
			if (value.IsInt())
			{
				newValues[i] = value.GetInt() * mult[i];
			}
			else
			{
				newValues[i] = value.GetReal() * mult[i];
			}
		}

		auto nameObj = fbObj.find("name");
		if (nameObj == fbObj.end())
		{
			LogString(JSonServer::e_debug, "Key name not found\n");
			return;
		}
		PXR_NS::JsValue nameVal = nameObj->second;
		if (!nameVal.IsString())
		{
			LogString(JSonServer::e_debug, "Bad key name\n");
			return;
		}
		
		OBJ_Node* obj_node =
			OPgetDirector()->findOBJNode(nameVal.GetString().c_str());
		assert(obj_node);

		HOM_AutoLock hom_lock;
		obj_node->setParameterOrProperty("light_color", 0, 0, newValues[0]);
		obj_node->setParameterOrProperty("light_color", 1, 0, newValues[1]);
		obj_node->setParameterOrProperty("light_color", 2, 0, newValues[2]);
	}
	else
	{
		LogString(JSonServer::e_debug, "Unknown operation\n");
	}
}

void
jsonServer::LogString(LogType i_type, const char *i_msg) const
{
	static UT_Package::utils::Logger logger;
	switch(i_type)
	{
		case JSonServer::e_debug :
#ifndef NDEBUG
			fprintf(stderr, "%s", i_msg);
#endif
			break;
		case JSonServer::e_log :
			logger.info(i_msg);
			break;
		case JSonServer::e_warning :
			logger.warning(i_msg);
			break;
		case JSonServer::e_error :
			logger.error(i_msg);
			break;
		default :
			fprintf(stderr, "%s", i_msg);
	}
}

bool
jsonServer::StartServer()
{
	assert(!m_instance);
	m_instance = new jsonServer();
	if(!m_instance->Start())
	{
		delete m_instance;
		m_instance = 0;
	}

	return m_instance != 0;
}

jsonServer::jsonServer()
{
}

jsonServer::~jsonServer()
{
}
