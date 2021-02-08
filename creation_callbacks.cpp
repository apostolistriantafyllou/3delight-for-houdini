#include "creation_callbacks.h"

#include "ROP_3Delight.h"
#include "safe_interest.h"

#include "OBJ/OBJ_Node.h"
#include "OP/OP_Director.h"

#include <mutex>
#include <vector>

namespace
{
	UT_String k_obj_manager_path = "/obj";

	std::mutex rops_mutex;
	std::set<ROP_3Delight*> rops;
	
	std::mutex interests_mutex;
	std::vector<safe_interest> obj_node_interests;
	std::vector<safe_interest> manager_interests;

	bool scene_loading = false;
	std::mutex scene_nodes_mutex;
	std::vector<OBJ_Node*> scene_nodes;

	void register_manager_cb(OP_Node& i_manager);

	/// Clears out inactive interests from a list.
	void clean_interests(std::vector<safe_interest>& io_interests)
	{
		if(io_interests.empty())
		{
			return;
		}

		safe_interest* last = &io_interests.back();
		safe_interest* current = last;
		do
		{
			if(!current->active())
			{
				*current = *last;
				last--;
			}
		}
		while(current-- != &io_interests.front());

		io_interests.resize(last-current);
	}

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
			creation_callbacks::add_attributes_to_node(*node);
		}
		scene_nodes.clear();
		scene_nodes_mutex.unlock();

		// Avoid accumulating inactive interests
		interests_mutex.lock();
		clean_interests(obj_node_interests);
		clean_interests(manager_interests);
		interests_mutex.unlock();
	}

	/**
		\brief Called when a child of an obj manager changes.

		We only react to the creation of geometry nodes.
	*/
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

		OP_Node* node = (OP_Node*)i_data;
		OBJ_Node* obj_node = node->castToOBJNode();
		if(!obj_node)
		{
			return;
		}

		// Notify registered ROPs of the new OBJ node
		rops_mutex.lock();
		for(ROP_3Delight* rop : rops)
		{
			rop->NewOBJNode(*obj_node);
			
		}
		rops_mutex.unlock();

		/*
			Filter-out unsupported node types. The mere absence of a script for
			a particular node type is sufficient to avoid adding attribute to
			it, so this check is redundant. However, it avoids having to try
			calling scripts on lots of node and, more importantly, it avoids
			filling the scene_nodes list with useless data during scene loading.
		*/
		if(obj_node->getObjectType() != OBJ_GEOMETRY &&
			obj_node->getObjectType() != OBJ_CAMERA &&
			obj_node->getObjectType() != OBJ_STD_NULL &&
			obj_node->getObjectType() != OBJ_STD_LIGHT)
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
			creation_callbacks::add_attributes_to_node(*obj_node);
		}
	}

	/// Registers a callback on an obj manager to detect node creation.
	void register_obj_node_cb(OP_Node& i_obj_manager)
	{
		/*
			Connect the callback to the obj manager.  safe_interest's destructor
			and assignment operator will ensure that, en the end, only one
			instance of the callback remains connected.
		*/
		interests_mutex.lock();
		obj_node_interests.emplace_back(&i_obj_manager, nullptr, &obj_node_cb);
		interests_mutex.unlock();
	}

	/**
		\brief Called when a child of a manager changes.

		We only react to the creation of other managers.
	*/
	void manager_cb(
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

		// We're only interested in managers here (including subnetworks)
		if(!node->isManager() && !node->isManagementNode() &&
			!node->isEffectivelyAManagementNode() && !node->isSubNetwork(true))
		{
			return;
		}

		register_manager_cb(*node);

		if(node->getOpTypeID() == OBJ_OPTYPE_ID ||
			node->getFullPath() == k_obj_manager_path)
		{
			/*
				This is an OBJ manager : it could contain OBJ nodes, in addition
				to other managers.
			*/
			register_obj_node_cb(*node);
		}
	}

	/// Registers a callback on a manager to detect obj manager creation
	void register_manager_cb(OP_Node& i_manager)
	{
		/*
			Connect the callback to the obj manager. safe_interest's destructor
			and assignment operator will ensure that, en the end, only one
			instance of the callback remains connected.
		*/
		interests_mutex.lock();

		manager_interests.emplace_back(&i_manager, nullptr, &manager_cb);

		/*
			Strangely (but, sadly, not surprisingly), manager_cb is called twice
			for the creation of managers "/mat" and "/out" under "/". Let's
			avoid registering our callback twice on the same manager, otherwise
			the callbacks for all their subnets will be duplicated as well.
		*/
		size_t nb_cb = manager_interests.size();
		if(nb_cb >= 2 &&
			manager_interests[nb_cb-2] == manager_interests[nb_cb-1])
		{
			manager_interests.pop_back();
		}

		interests_mutex.unlock();
	}

}

void creation_callbacks::init()
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
		creation of any node manager. When this happens, we will be able to
		connect a callback that notifies of the creation of any node
		under an obj manager.

		Note that, unlike the creation scripts automatically called by Houdini,
		our callback is also called during the opening of a scene, which means
		that we don't need a 456.cmd script anymore.
	*/
	register_manager_cb(*OPgetDirector());
}

void creation_callbacks::add_attributes_to_node(OBJ_Node& io_node)
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

void creation_callbacks::register_ROP(ROP_3Delight* i_rop)
{
	rops_mutex.lock();
	rops.insert(i_rop);
	rops_mutex.unlock();
}

void creation_callbacks::unregister_ROP(ROP_3Delight* i_rop)
{
	rops_mutex.lock();
	rops.erase(i_rop);
	rops_mutex.unlock();
}

