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

	// Retrieves a JSON array from a layer's "feedback data" JSON object
	UT_JSONValueArray*
	GetLayerArray(UT_JSONValueMap& i_map, const char* i_name)
	{
		UT_JSONValue* layer_obj = i_map.get("layer");
		assert(layer_obj);
		if(!layer_obj) return nullptr;

		UT_JSONValueMap* layer_map = layer_obj->getMap();
		assert(layer_map);
		if(!layer_map) return nullptr;

		UT_JSONValue* array_obj = layer_map->get(i_name);
		assert(array_obj);
		if(!array_obj) return nullptr;

		UT_JSONValueArray* array = array_obj->getArray();
		assert(array);
		return array;
	}

	/*
		Returns the node accessible through the full path at index i_index of
		array i_nodes.
	*/
	OBJ_Node*
	GetOBJNode(const UT_JSONValueArray& i_nodes, unsigned i_index)
	{
		assert(i_index < i_nodes.size());
		const UT_JSONValue* node_obj = i_nodes[i_index];
		assert(node_obj);
		assert(node_obj->getType() == UT_JSONValue::JSON_STRING);
		if(!node_obj || node_obj->getType() != UT_JSONValue::JSON_STRING)
		{
			return nullptr;
		}

		const char* node = node_obj->getS();
		assert(node);

		OBJ_Node* obj_node = OPgetDirector()->findOBJNode(node);
		assert(obj_node);
		return obj_node;
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
	CleanUp();
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
}


void idisplay_port::CleanUp()
{
	idisplay_port *idp = idisplay_port::get_instance();

	// Wait for client threads to terminate
	while (!idp->m_threads.empty())
	{
		idp->m_threads.back().join();
		idp->m_threads.pop_back();
	}
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
			Log( "insufficient data to start render" );
			return;
		}

		// true means we start a live render, otherwise an interactive one
		bool live = liveObj->getB();

		UT_JSONValue* fromObj = object_map->get("from");

		if( !fromObj )
		{
			Log( "insufficient data to select render node" );
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
			Log( "insufficient data to select render node" );
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
			rop_3dl->UpdateCrop(crop);
		}
	}
	else if (op == "select layer")
	{
		UT_JSONValueArray* nodes = GetLayerArray(*object_map, "nodes");
		assert(nodes);
		if(!nodes)
		{
			Log("insufficient data to select light nodes");
			return;
		}

		HOM_AutoLock hom_lock;

		OP_Director* director = OPgetDirector();
		director->clearPickedItems();

		for (unsigned i = 0; i < nodes->size(); i++)
		{
			OBJ_Node* obj_node = GetOBJNode(*nodes, i);
			if(obj_node)
			{
				obj_node->setEditPicked(1);
			}
		}
	}
	else if (op == "scale layer intensity")
	{
		UT_JSONValue* factor_obj = object_map->get("scale factor");
		assert(factor_obj);
		if(!factor_obj)
		{
			Log("insufficient data to adjust light intensities");
			return;
		}

		assert(factor_obj->getType() == UT_JSONValue::JSON_INT ||
			factor_obj->getType() == UT_JSONValue::JSON_REAL);
		double factor = factor_obj->getF();

		UT_JSONValueArray* nodes = GetLayerArray(*object_map, "nodes");
		assert(nodes);
		UT_JSONValueArray* intensities = GetLayerArray(*object_map, "intensities");
		assert(intensities);

		if(!nodes || !intensities)
		{
			Log("insufficient data to adjust light intensities");
			return;
		}

		assert(nodes->size() == intensities->size());

		HOM_AutoLock hom_lock;

		for(unsigned i = 0; i < nodes->size(); i++)
		{
			OBJ_Node* obj_node = GetOBJNode(*nodes, i);
			assert(obj_node);

			UT_JSONValue* intensity_obj = (*intensities)[i];
			assert(intensity_obj->getType() == UT_JSONValue::JSON_INT ||
				intensity_obj->getType() == UT_JSONValue::JSON_REAL);
			double intensity = intensity_obj->getF();

			obj_node->setParameterOrProperty(
				"light_intensity", 0, 0, factor*intensity);
		}
	}
	else if (op == "update layer filter")
	{
		UT_JSONValue* multiplier_obj = object_map->get("color multiplier");
		assert(multiplier_obj);
		assert(multiplier_obj->getType() == UT_JSONValue::JSON_ARRAY);
		if(!multiplier_obj ||
			multiplier_obj->getType() != UT_JSONValue::JSON_ARRAY)
		{
			Log("insufficient data to adjust light intensities");
			return;
		}

		UT_JSONValueArray* multiplier_array = multiplier_obj->getArray();
		assert(multiplier_array);
		assert(multiplier_array->size() == 3);

		double multiplier[3];
		for(unsigned j = 0; j < 3; j++)
		{
			UT_JSONValue* m = (*multiplier_array)[j];
			assert(m);
			assert(m->getType() == UT_JSONValue::JSON_INT ||
				m->getType() == UT_JSONValue::JSON_REAL);
			multiplier[j] = m->getF();
		}

		UT_JSONValueArray* nodes = GetLayerArray(*object_map, "nodes");
		assert(nodes);
		UT_JSONValueArray* colors = GetLayerArray(*object_map, "colors");
		assert(colors);

		if(!nodes || !colors)
		{
			Log("insufficient data to adjust light intensities");
			return;
		}

		assert(nodes->size() == colors->size());

		HOM_AutoLock hom_lock;

		for(unsigned i = 0; i < nodes->size(); i++)
		{
			OBJ_Node* obj_node = GetOBJNode(*nodes, i);
			assert(obj_node);

			UT_JSONValue* color_obj = (*colors)[i];
			assert(color_obj->getType() == UT_JSONValue::JSON_ARRAY);
			UT_JSONValueArray* color_array = color_obj->getArray();
			assert(color_array);
			assert(color_array->size() == 3);

			for(unsigned j = 0; j < 3; j++)
			{
				UT_JSONValue* c = (*color_array)[j];
				assert(c);
				assert(c->getType() == UT_JSONValue::JSON_INT ||
					c->getType() == UT_JSONValue::JSON_REAL);
				double component = c->getF();
				obj_node->setParameterOrProperty(
					"light_color", j, 0, multiplier[j]*component);
			}
		}
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

