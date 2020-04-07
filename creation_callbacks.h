#pragma once

class OBJ_Node;
class ROP_3Delight;

/**
	This ensures that our scripts that try to add custom attributes on node
	creation are actually called.

	Houdini's built-in system does call the scripts automatically, but it only
	calls the first appropriately-named one it finds in its search path. This
	means that we have no guarantee that our scripts would be called since other
	plugins could also define such scripts.

	So, instead of relying on this flaky system, we install C++ callbacks that
	call the appropriate scripts manually *each time* an object is created or a
	scene is opened.
	
	This is also used to notify a ROP of new nodes to be added to an IPR render.
*/
namespace creation_callbacks
{
	/// Initializes the callbacks that add custom attributes.
	void init();

	/// Utility function that adds our custom attributes to a node.
	void add_attributes_to_node(OBJ_Node& io_node);
	
	/// Register a ROP to be notified when an OBJ node is created
	void register_ROP(ROP_3Delight* i_rop);
	/// Unregisters a ROP previously registered with register_ROP
	void unregister_ROP(ROP_3Delight* i_rop);
}
