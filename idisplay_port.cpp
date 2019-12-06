#include "idisplay_port.h"

#include "ROP_3Delight.h"

#include <HOM/HOM_Module.h>
#include <OBJ/OBJ_Light.h>
#include <OP/OP_Director.h>
#include <ROP/ROP_Node.h>
#include <UT/UT_JSONParser.h>
#include <UT/UT_JSONValue.h>
#include <UT/UT_NetSocket.h>
#include <UT/UT_PackageUtils.h>
#include <UT/UT_WorkBuffer.h>

#include <assert.h>
#include <stdarg.h>

#include <memory>


namespace
{
	ROP_3Delight*
	Get3DelightROP(const char* i_path)
	{
		if(!i_path)
		{
			return nullptr;
		}

		ROP_Node* rop_node = OPgetDirector()->findROPNode(i_path);
		return dynamic_cast<ROP_3Delight*>(rop_node);
	}

	bool
	RetrieveCrop(float* o_crop, const UT_JSONValueMap& i_object_map)
	{
		// Render with a crop region if possible
		const UT_JSONValue* useCropObj = i_object_map.get("usecrop");
		if(!useCropObj || useCropObj->getType() != UT_JSONValue::JSON_INT ||
			!useCropObj->getB())
		{
			return false;
		}

		const UT_JSONValue* cropObj = i_object_map.get("cropregion");
		if(cropObj->getType() != UT_JSONValue::JSON_ARRAY)
		{
			return false;
		}

		const UT_JSONValueArray* cropArray = cropObj->getArray();
		if(!cropArray || cropArray->size() != 4 ||
			(cropArray->get(0)->getType() != UT_JSONValue::JSON_INT &&
			cropArray->get(0)->getType() != UT_JSONValue::JSON_REAL))
		{
			return false;
		}

		for(unsigned i = 0; i < 4; i++)
		{
			o_crop[i] = float(cropArray->get(i)->getF());
		}

		return true;
	}

}


idisplay_port *idisplay_port::get_instance()
{
	static std::unique_ptr<idisplay_port> sm_instance;
	if( !sm_instance )
	{
		sm_instance = std::make_unique<idisplay_port>();
	}

	return sm_instance.get();
}

idisplay_port::idisplay_port()
	:	UT_SocketListener(UT_NetSocket::newSocket(0)),
		m_main_thread(SYSgetSTID())
{
	start();
}

idisplay_port::~idisplay_port()
{
	stop();
}

/**
	Keep reading until socket is closed from 3Delight Display.
*/
void clientConnection(UT_NetSocket* client, idisplay_port* dp)
{
	while (1)
	{
		UT_WorkBuffer wb;
		int res2 = client->read(wb);

		if (res2 != 0 || wb.isEmpty())
			break;

		// Skip the number of bytes at the beginning
		wb.eraseHead(sizeof(unsigned));

		if (wb.isEmpty())
			break;

		assert(wb.buffer()[0] == '{');
		assert(wb.isNullTerminated());

		UT_IStream in(wb);
		UT_JSONParser parser;
		UT_JSONValue js;

		if( js.parseValue(parser, &in) &&
			js.getType() == UT_JSONValue::JSON_MAP )
		{
			dp->ExecuteJSonCommand(js);
		}
		else
		{
			dp->Log( "invalid read operation from socket" );
		}
	}
}

/**
	As soon as we accept a read socket, we start a thread for reading.
	This is needed to be able to accept other client connection.
*/
void idisplay_port::onSocketEvent(int event_types)
{
	if (event_types != UT_SOCKET_READ_READY)
	{
		assert( false );
		return;
	}

	UT_NetSocket* server = getSocket();

	if( !server )
	{
		assert( false );
		return;
	}

	int res = server->dataAvailable();

	if (res <= 0)
		return;

	int condition;
	UT_NetSocket *client = server->accept(true, condition);

	if( !client || !client->isConnected() )
		return;

	m_clients.push_back(client);	
	m_threads.push_back(std::thread(clientConnection, client, this));
	// To avoid "terminate called without an active exception"
	m_threads.back().detach();
}


void idisplay_port::CleanUp()
{
	idisplay_port *idp = idisplay_port::get_instance();
	// Terminate all client threads
	while (!idp->m_threads.empty()) idp->m_threads.pop_back();
	// Delete all client sockets
	while (!idp->m_clients.empty())
	{
		delete idp->m_clients.back();
		idp->m_clients.pop_back();
	}
}

int idisplay_port::GetServerPort()
{
	UT_NetSocket* server = getSocket();
	if( !server || !server->isValid() )
	{
		assert( false );
		return -1;
	}

	return server->getPort();
}

std::string idisplay_port::GetServerHost()
{
	unsigned char hostAddress[4];
	char hostName[4*3+4];

	UT_NetSocket* server = getSocket();
	if( !server || !server->isValid() )
	{
		assert( false );
		return {};
	}

	// First try with host name (in server->getAddress)
	int res = UT_NetSocket::getHostAddress(hostAddress, server->getAddress());
	if (res == 0)
	{
		// If fails, try with the name localhost
		res = UT_NetSocket::getHostAddress(hostAddress, "localhost");
		// Last resort, return local host address
		if (res == 0) return "127.0.0.1";
	}
	assert(res == 1);
	int nc = sprintf(hostName, "%hhu.%hhu.%hhu.%hhu",
					hostAddress[0], hostAddress[1],
					hostAddress[2], hostAddress[3]);
	hostName[nc] = '\0';
	return hostName;
}

void idisplay_port::ExecuteJSonCommand(const UT_JSONValue& i_object)
{
	UT_JSONValueMap* object_map = i_object.getMap();
	assert( object_map );

	UT_JSONValue* opObj = object_map->get("operation");
	if( !opObj )
	{
		Log( "operation missing from 3Delight Display data");
		return;
	}

	std::string op = opObj->getS();
	if (op == "start render")
	{
		UT_JSONValue* liveObj = object_map->get("live");

		if( !liveObj || liveObj->getType() != UT_JSONValue::JSON_INT )
		{
			Log( "unsufficient data to start render" );
			return;
		}

		// true means we start a live render, otherwise an interactive one
		bool live = liveObj->getB();

		UT_JSONValue* fromObj = object_map->get("from");

		if( !fromObj )
		{
			Log( "unsufficient data to select render node" );
			return;
		}

		HOM_AutoLock hom_lock;
		ROP_3Delight* rop_3dl = Get3DelightROP(fromObj->getS());
		if(!rop_3dl)
		{
			Log( "render node `%s` hasn't been found, render impossible",
				fromObj->getS() );
			return;
		}

		double time =
			OPgetDirector()->getChannelManager()->getEvaluateTime(m_main_thread);

		float crop[4];
		if(RetrieveCrop(crop, *object_map))
		{
			rop_3dl->StartRenderFromIDisplay(time, live, crop);
		}
		else
		{
			rop_3dl->StartRenderFromIDisplay(time, live, nullptr);
		}
	}
	else if (op == "update crop")
	{
		UT_JSONValue* fromObj = object_map->get("from");

		if( !fromObj )
		{
			Log( "unsufficient data to select render node" );
			return;
		}

		HOM_AutoLock hom_lock;
		ROP_3Delight* rop_3dl = Get3DelightROP(fromObj->getS());
		if(!rop_3dl)
		{
			Log( "render node `%s` hasn't been found, render impossible",
				fromObj->getS() );
			return;
		}

		float crop[4];
		if(RetrieveCrop(crop, *object_map))
		{
			rop_3dl->UpdateIDisplayPriorityWindow(crop);
		}
	}
	else if (op == "select layer")
	{
		UT_JSONValue* layerObj = object_map->get("layer");

		if( !layerObj )
		{
			Log( "unsufficient data to select light sources" );
			return;
		}

		UT_JSONValueMap* layer_map = layerObj->getMap();
		UT_JSONValue* nameObj = layer_map ? layer_map->get("name") : nullptr;

		if( !nameObj )
		{
			Log( "unsufficient data to select light sources (2)" );
			return;
		}

		const char *light = nameObj->getS();
		if( !light )
			light = "";

		HOM_AutoLock hom_lock;
		OBJ_Node* obj_node = OPgetDirector()->findOBJNode( light );
		if(!obj_node)
		{
			Log( "light source `%s` hasn't been found, selection impossible",
				light );
			return;
		}

		obj_node->setEditPicked(1);
	}
	else if (op == "scale layer intensity")
	{
		UT_JSONValue* layerObj = object_map->get("layer");
		UT_JSONValue* factorObj = object_map->get("scale factor");

		if( !factorObj ||
			layerObj->getType() != UT_JSONValue::JSON_MAP ||
		   (factorObj->getType() != UT_JSONValue::JSON_INT &&
			factorObj->getType() != UT_JSONValue::JSON_REAL) )
		{
			Log( "data missing, no light adjustemnt is possible");
			return;
		}

		fpreal factor = factorObj->getF();
		UT_JSONValueMap* layer_map = layerObj->getMap();
		UT_JSONValue* valuesObj = layer_map ? layer_map->get("values") : nullptr;
		UT_JSONValueArray* values = valuesObj ? valuesObj->getArray() : nullptr;

		if( !values )
		{
			Log( "values not found" );
			return;
		}

		fpreal newValue;
		for (unsigned i = 0; i < values->size(); i++)
		{
			UT_JSONValue* value = (*values)[i];
			assert(value);
			if (value->getType() != UT_JSONValue::JSON_INT &&
				value->getType() != UT_JSONValue::JSON_REAL)
			{
				Log( "bad type for 'value' in values array" );
				return;
			}
			newValue =
				value->getType() == UT_JSONValue::JSON_INT
				? value->getI() * factor
				: value->getF() * factor;
		}

		UT_JSONValue* nameObj = layer_map->get("name");
		const char *light = nameObj ? nameObj->getS() : nullptr;
		if( !light )
		{
			Log( "light name not provided for intensity scale");
			return;
		}

		HOM_AutoLock hom_lock;

		OBJ_Node* obj_node = OPgetDirector()->findOBJNode( light );
		if(!obj_node)
		{
			Log( "light '%s' not found", light);
			return;
		}

		obj_node->setParameterOrProperty("light_intensity", 0, 0, newValue);
	}
	else if (op == "update layer filter")
	{
		UT_JSONValue* layerObj = object_map->get("layer");
		UT_JSONValue* colorMultObj = object_map->get("color multiplier");

		if( !layerObj || layerObj->getType() != UT_JSONValue::JSON_MAP  ||
			!colorMultObj || colorMultObj->getType() != UT_JSONValue::JSON_ARRAY)
		{
			Log( "invalid color multiplier operation");
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
				Log( "bad value in color multiplier");
				return;
			}
			mult[i] = value->getF();
		}

		UT_JSONValueMap* layer_map = layerObj->getMap();
		UT_JSONValue* colorValuesObj = layer_map->get("color_values");

		if( !colorValuesObj ||
			colorValuesObj->getType() != UT_JSONValue::JSON_ARRAY )
		{
			Log( "key color_values not found");
			return;
		}

		UT_JSONValueArray* colorValues = colorValuesObj->getArray();
		assert( colorValues );

		fpreal newValues[3];
		for (unsigned i = 0; i < colorValues->size(); i++)
		{
			UT_JSONValue* value = (*colorValues)[i];
			assert(value);
			if (value->getType() != UT_JSONValue::JSON_INT &&
				value->getType() != UT_JSONValue::JSON_REAL)
			{
				Log( "bad value in color_values");
				return;
			}
			newValues[i] = value->getF() * mult[i];
		}

		UT_JSONValue* nameObj = layer_map->get("name");
		const char *light = nameObj ? nameObj->getS() : nullptr;

		if (!light)
		{
			Log( "light not provided for color multiplier");
			return;
		}

		HOM_AutoLock hom_lock;

		OBJ_Node* obj_node = OPgetDirector()->findOBJNode( light );
		if(!obj_node)
		{
			Log( "light '%s' not found", light);
			return;
		}

		obj_node->setParameterOrProperty("light_color", 0, 0, newValues[0]);
		obj_node->setParameterOrProperty("light_color", 1, 0, newValues[1]);
		obj_node->setParameterOrProperty("light_color", 2, 0, newValues[2]);
	}
	else
	{
		Log("unknown operation");
	}
}

void idisplay_port::Log( const char *i_msg, ... ) const
{
	char theMessage[ 2048 ];

	va_list ap;
	va_start(ap, i_msg);

	vsnprintf( theMessage, sizeof(theMessage)-1, i_msg, ap );

	va_end( ap );

	std::string error("3Delight for Houdini: 3Delight Display port - " );
	error += theMessage;

	static UT_Package::utils::Logger logger;
	logger.error( error.c_str() );

	error += "\n";
	::fprintf( stderr, "%s", error.c_str() );
}

