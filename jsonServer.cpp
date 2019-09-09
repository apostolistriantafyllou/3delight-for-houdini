#include "jsonServer.h"

#include <HOM/HOM_Module.h>
#include <OBJ/OBJ_Light.h>
#include <OP/OP_Director.h>
#include <UT/UT_NetSocket.h>
#include <UT/UT_PackageUtils.h>
#include <UT/UT_WorkBuffer.h>

#include <assert.h>
#include <stdarg.h>

JSonServer::JSonServer(UT_NetSocket* i_socket)
	: UT_SocketListener(i_socket),
	  m_client(0)
{
	start();
}

JSonServer::~JSonServer()
{
	stop();
}

int JSonServer::getPort() const
{
	UT_NetSocket* server = getSocket();
	assert(server);
	assert(server->isValid());
	return server->getPort();
}

std::string JSonServer::getServerHost() const
{
	enum { kHostSize = 1025 };
	char host[kHostSize];
	if( 0 != gethostname( host, sizeof(host) ) )
		return "";

	return host;
}

void JSonServer::ExecuteJSonCommand(const PXR_NS::JsObject& i_object)
{
	assert(false);
}

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

void JSonServer::onSocketEvent(int event_types)
{
	if (event_types == UT_SOCKET_READ_READY)
	{
		UT_NetSocket* server = getSocket();
		assert(server);
		assert(server->isValid());
		assert(!server->isBlocking());
		int res = server->dataAvailable();
		if (res <= 0) return;
		if (!m_client)
		{
			int condition;
			m_client = server->accept(true, condition);
		}
		assert(m_client);
		assert(m_client->isValid());
		assert(m_client->isConnected());
		assert(m_client->isBlocking());
		while (1)
		{
			UT_WorkBuffer wb;
			int res2 = m_client->read(wb);

			if (res2 != 0 || wb.isEmpty()) return;
			// Skip the number of bytes at the beginning
			wb.eraseHead(sizeof(unsigned));
			assert(wb.buffer()[0] == '{');
			assert(wb.isNullTerminated());

			std::string jsonString = wb.buffer();
			if (jsonString.empty()) return;

			PXR_NS::JsValue js_val = PXR_NS::JsParseString(jsonString);

			if( js_val.IsObject() )
			{
				const PXR_NS::JsObject& js_obj = js_val.GetJsObject();
				ExecuteJSonCommand(js_obj);
			}
			else
			{
				Log( e_debug, "received invalid JSON object.\n" );
			}
		}
	}
}


jsonServer* jsonServer::m_instance = 0;

void
jsonServer::CleanUp()
{
	if(m_instance)
	{
		m_instance->stop();
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
		return m_instance->getPort();
	}

	return -1;
}

std::string
jsonServer::GetServerHost()
{
	if(m_instance || StartServer())
	{
		assert(m_instance);
		return m_instance->getServerHost();
	}

	return "";
}

void
jsonServer::ExecuteJSonCommand(const PXR_NS::JsObject& i_object)
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
	m_instance = new jsonServer(UT_NetSocket::newSocket(0));

	if(!m_instance->isListening())
	{
		delete m_instance;
		m_instance = 0;
	}

	return m_instance != 0;
}

jsonServer::jsonServer(UT_NetSocket* i_socket)
	: JSonServer(i_socket)
{
}

jsonServer::~jsonServer()
{
}
