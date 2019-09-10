#include "jsonServer.h"

#include <HOM/HOM_Module.h>
#include <OBJ/OBJ_Light.h>
#include <OP/OP_Director.h>
#include <UT/UT_JSONParser.h>
#include <UT/UT_JSONValue.h>
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

	UT_NetSocket* server = getSocket();
	assert(server);
	assert(server->isValid());

	server->getHostName(host, kHostSize);

	return host;
}

void JSonServer::ExecuteJSonCommand(const UT_JSONValue& i_object)
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
			if (wb.isEmpty()) return;

			assert(wb.buffer()[0] == '{');
			assert(wb.isNullTerminated());

			UT_IStream in(wb);
			UT_JSONParser parser;
			UT_JSONValue js;
			if( js.parseValue(parser, &in) &&
				js.getType() == UT_JSONValue::JSON_MAP )
			{
				ExecuteJSonCommand(js);
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
jsonServer::ExecuteJSonCommand(const UT_JSONValue& i_object)
{
	UT_JSONValueMap* object_map = i_object.getMap();

	UT_JSONValue* opObj = object_map->get("operation");
	if (!opObj)
	{
		LogString(JSonServer::e_debug, "Key operation not found\n");
		return;
	}
	if (opObj->getType() != UT_JSONValue::JSON_STRING)
	{
		LogString(JSonServer::e_debug, "Bad type for operation\n");
		return;
	}

	std::string op = opObj->getS();
	if (op == "select layer")
	{
		UT_JSONValue* layerObj = object_map->get("layer");
		if (!layerObj)
		{
			LogString(JSonServer::e_debug, "Key layer not found\n");
			return;
		}
		if (layerObj->getType() != UT_JSONValue::JSON_MAP)
		{
			LogString(JSonServer::e_debug, "Bad type for layer\n");
			return;
		}

		UT_JSONValueMap* layer_map = layerObj->getMap();
		UT_JSONValue* nameObj = layer_map->get("name");
		if (!nameObj)
		{
			LogString(JSonServer::e_debug, "Key name not found\n");
			return;
		}
		if (nameObj->getType() != UT_JSONValue::JSON_STRING)
		{
			LogString(JSonServer::e_debug, "Bad type for name\n");
			return;
		}
		
		HOM_AutoLock hom_lock;

		OBJ_Node* obj_node = OPgetDirector()->findOBJNode(nameObj->getS());
		if(!obj_node)
		{
			LogString(JSonServer::e_debug, "Node not found\n");
			return;
		}

		obj_node->setEditPicked(1);
	}
	else if (op == "scale layer intensity")
	{
		UT_JSONValue* layerObj = object_map->get("layer");
		if (!layerObj)
		{
			LogString(JSonServer::e_debug, "Key layer not found\n");
			return;
		}
		if (layerObj->getType() != UT_JSONValue::JSON_MAP)
		{
			LogString(JSonServer::e_debug, "Bad type for layer\n");
			return;
		}

		UT_JSONValue* factorObj = object_map->get("scale factor");
		if (!factorObj)
		{
			LogString(JSonServer::e_debug, "Key scale factor not found\n");
			return;
		}
		if (factorObj->getType() != UT_JSONValue::JSON_INT &&
			factorObj->getType() != UT_JSONValue::JSON_REAL)
		{
			LogString(JSonServer::e_debug, "Bad type for scale factor\n");
			return;
		}
		fpreal factor =
			factorObj->getType() == UT_JSONValue::JSON_INT
			? factorObj->getI()
			: factorObj->getF();

		UT_JSONValueMap* layer_map = layerObj->getMap();
		UT_JSONValue* valuesObj = layer_map->get("values");
		if (!valuesObj)
		{
			LogString(JSonServer::e_debug, "Key values not found\n");
			return;
		}
		if (valuesObj->getType() != UT_JSONValue::JSON_ARRAY)
		{
			LogString(JSonServer::e_debug, "Bad type for values\n");
			return;
		}
		UT_JSONValueArray* values = valuesObj->getArray();
		assert(values);

		fpreal newValue;
		for (unsigned i = 0; i < values->size(); i++)
		{
			UT_JSONValue* value = (*values)[i];
			assert(value);
			if (value->getType() != UT_JSONValue::JSON_INT &&
				value->getType() != UT_JSONValue::JSON_REAL)
			{
				LogString(JSonServer::e_debug, "Bad value in values\n");
				return;
			}
			newValue =
				value->getType() == UT_JSONValue::JSON_INT
				? value->getI() * factor
				: value->getF() * factor;
		}

		UT_JSONValue* nameObj = layer_map->get("name");
		if (!nameObj)
		{
			LogString(JSonServer::e_debug, "Key name not found\n");
			return;
		}
		if (nameObj->getType() != UT_JSONValue::JSON_STRING)
		{
			LogString(JSonServer::e_debug, "Bad type for name\n");
			return;
		}

		HOM_AutoLock hom_lock;

		OBJ_Node* obj_node = OPgetDirector()->findOBJNode(nameObj->getS());
		if(!obj_node)
		{
			LogString(JSonServer::e_debug, "Node not found\n");
			return;
		}

		obj_node->setParameterOrProperty("light_intensity", 0, 0, newValue);
	}
	else if (op == "update layer filter")
	{
		UT_JSONValue* layerObj = object_map->get("layer");
		if (!layerObj)
		{
			LogString(JSonServer::e_debug, "Key layer not found\n");
			return;
		}
		if (layerObj->getType() != UT_JSONValue::JSON_MAP)
		{
			LogString(JSonServer::e_debug, "Bad type for layer\n");
			return;
		}

		UT_JSONValue* colorMultObj = object_map->get("color multiplier");
		if (!colorMultObj)
		{
			LogString(JSonServer::e_debug, "Key color multiplier not found\n");
			return;
		}
		if (colorMultObj->getType() != UT_JSONValue::JSON_ARRAY)
		{
			LogString(JSonServer::e_debug, "Bad type for color multiplier\n");
			return;
		}
		UT_JSONValueArray* colMult = colorMultObj->getArray();
		assert(colMult);

		fpreal mult[3];
		for (unsigned i = 0; i < colMult->size(); i++)
		{
			UT_JSONValue* value = (*colMult)[i];
			assert(value);
			if (value->getType() != UT_JSONValue::JSON_INT &&
				value->getType() != UT_JSONValue::JSON_REAL)
			{
				LogString(JSonServer::e_debug, "Bad value in color multiplier\n");
				return;
			}
			mult[i] =
				value->getType() == UT_JSONValue::JSON_INT
				? value->getI()
				: value->getF();
		}

		UT_JSONValueMap* layer_map = layerObj->getMap();
		UT_JSONValue* colorValuesObj = layer_map->get("color_values");
		if (!colorValuesObj)
		{
			LogString(JSonServer::e_debug, "Key color_values not found\n");
			return;
		}
		if (colorValuesObj->getType() != UT_JSONValue::JSON_ARRAY)
		{
			LogString(JSonServer::e_debug, "Bad type for color_values\n");
			return;
		}
		UT_JSONValueArray* colorValues = colorValuesObj->getArray();

		fpreal newValues[3];
		for (unsigned i = 0; i < colorValues->size(); i++)
		{
			UT_JSONValue* value = (*colorValues)[i];
			assert(value);
			if (value->getType() != UT_JSONValue::JSON_INT &&
				value->getType() != UT_JSONValue::JSON_REAL)
			{
				LogString(JSonServer::e_debug, "Bad value in color_values\n");
				return;
			}
			newValues[i] =
				value->getType() == UT_JSONValue::JSON_INT
				? value->getI() * mult[i]
				: value->getF() * mult[i];
		}

		UT_JSONValue* nameObj = layer_map->get("name");
		if (!nameObj)
		{
			LogString(JSonServer::e_debug, "Key name not found\n");
			return;
		}
		if (nameObj->getType() != UT_JSONValue::JSON_STRING)
		{
			LogString(JSonServer::e_debug, "Bad type for name\n");
			return;
		}
		
		HOM_AutoLock hom_lock;

		OBJ_Node* obj_node = OPgetDirector()->findOBJNode(nameObj->getS());
		if(!obj_node)
		{
			LogString(JSonServer::e_debug, "Node not found\n");
			return;
		}

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
