#include "attributes_callbacks.h"

#include "safe_interest.h"

#include "OBJ/OBJ_Node.h"
#include "OP/OP_Director.h"

#include <mutex>
#include <vector>

namespace
{
	const char* k_obj_manager_path = "/obj";

	std::mutex interests_mutex;
	safe_interest obj_node_interest;
	safe_interest obj_manager_interest;

	bool scene_loading = false;
	std::mutex scene_nodes_mutex;
	std::vector<OBJ_Node*> scene_nodes;

	/**
		\brief Called when a scene is loaded (opened or merged).

		Disables direct calling of attributes scripts.
	*/
	void begin_load_scene_cb(OP_Director::EventType, void*)
	{
		scene_loading = true;
	}

	/**
		\brief Called when a scene has finished loading (opening or merging).

		Calls the appropriate attribute scripts on all nodes created during
		loading.
	*/
	void end_load_scene_cb(OP_Director::EventType, void*)
	{
		scene_loading = false;

		scene_nodes_mutex.lock();
		for(OBJ_Node* node : scene_nodes)
		{
			attributes_callbacks::add_attributes_to_node(*node);
		}
		scene_nodes.clear();
		scene_nodes_mutex.unlock();
	}

	/// Called when a child of /obj changes (we only react to their creation)
	void obj_node_cb(
		OP_Node* i_caller,
		void*,
		OP_EventType i_type,
		void* i_data)
	{
		if(i_type != OP_CHILD_CREATED)
		{
			return;
		}

		/*
			Filter-out unsupported node types. The mere absence of a script for
			a particular node type is sufficient to avoid adding attribute to
			it, so this check is redundant. However, it avoids having to try
			calling scripts on lots of node and, more importantly, it avoids
			filling the scene_nodes list with useless data during scene loading.
		*/
		OP_Node* node = (OP_Node*)i_data;
		OBJ_Node* obj_node = node->castToOBJNode();
		if(!obj_node ||
			(obj_node->getObjectType() != OBJ_GEOMETRY &&
			obj_node->getObjectType() != OBJ_CAMERA &&
			obj_node->getObjectType() != OBJ_STD_LIGHT))
		{
			return;
		}

		if(scene_loading)
		{
			// Wait until after scene loading to add our attributes to the node
			scene_nodes_mutex.lock();
			scene_nodes.push_back(obj_node);
			scene_nodes_mutex.unlock();
		}
		else
		{
			// Add our attributes to the node now. 
			attributes_callbacks::add_attributes_to_node(*obj_node);
		}
	}

	/// Registers a callback on /obj to detect node creation.
	void register_obj_node_cb()
	{
		// Find the /obj node
		OP_Node* obj_manager = OPgetDirector()->findNode(k_obj_manager_path);
		if(!obj_manager)
		{
			return;
		}

		/*
			Connect the callback to /obj.  safe_interest's destructor and
			assignment operator will ensure that, en the end, only one instance
			of the callback remains connected.
		*/
		interests_mutex.lock();
		obj_node_interest =
			safe_interest(
					obj_manager,
					nullptr,
					&obj_node_cb);
		interests_mutex.unlock();
	}

	/// Called when a child of / changes (we only react to the creation of /obj)
	void obj_manager_cb(
		OP_Node* i_caller,
		void*,
		OP_EventType i_type,
		void* i_data)
	{
		if(i_type != OP_CHILD_CREATED)
		{
			return;
		}

		OP_Node* node = (OP_Node*)i_data;
		UT_StringRef path = node->getFullPath();
		if(path != k_obj_manager_path)
		{
			return;
		}

		register_obj_node_cb();
	}

}

void attributes_callbacks::init()
{
	/*
		Connect callbacks to the director in order to be aware of scene loading
		events. This allows us to delay the addition of attributes until after
		the scene has finished loading.  Otherwise, we get uneven results : even
		though all attributes scripts are called, 3Delight-specific attributes
		are missing on many nodes.
	*/
	OPgetDirector()->addEventCallback(
		OP_Director::BEGIN_LOAD_NETWORK, &begin_load_scene_cb, nullptr);
	OPgetDirector()->addEventCallback(
		OP_Director::BEGIN_MERGE_NETWORK, &begin_load_scene_cb, nullptr);
	OPgetDirector()->addEventCallback(
		OP_Director::END_LOAD_NETWORK, &end_load_scene_cb, nullptr);
	OPgetDirector()->addEventCallback(
		OP_Director::END_MERGE_NETWORK, &end_load_scene_cb, nullptr);

	/*
		Here, we connect a callback on the root node which will notify us of the
		creation of the /obj network's manager node. When this happens, we will
		be able to connect a callback that notifies of the creation of any node
		under /obj.

		Note that, unlike the creation scripts automatically called by Houdini,
		our callback is also called during the opening of a scene, which means
		that we don't need a 456.cmd script anymore.
	*/
	interests_mutex.lock();
	obj_manager_interest = 
		safe_interest(OPgetDirector(), nullptr, &obj_manager_cb);
	interests_mutex.unlock();

	/*
		Try to connect the child node creation callback immediately, in case
		/obj already exists.
	*/
	register_obj_node_cb();
}

void attributes_callbacks::add_attributes_to_node(OBJ_Node& io_node)
{
	// Retrieve the node's operator type
	std::string node_type = io_node.getOperator()->getName().toStdString();

	/*
		Replace colons with dashes to get the name of the file. This is slightly
		different than Houdini's default behavior, which is to replace each
		*pair* of colons ("::") with a single dash ("-"). But we don't really
		care : we just want a file name that contains "regular" characters.
	*/
	std::replace(node_type.begin(), node_type.end(), ':', '-');

	// Execute the script with the node's full path as an argument
	std::string init_cmd =
		"private/" + node_type + "_OnCreated.cmd " +
		io_node.getFullPath().toStdString();
	OPgetDirector()->getCommandManager()->execute(init_cmd.c_str());
}
