#pragma once

#include <DM/DM_SceneHook.h>

#include <nsi.hpp>

class dm_SetupTask;
/// Scene hook that allows 3Delight rendering directly into Houdini viewports
class viewport_hook_builder : public DM_SceneHook
{
public:

	/// Destructor
    virtual ~viewport_hook_builder();

	/// Single instance accessor
	static viewport_hook_builder& instance();

    /// Called when a viewport needs to create a new render hook.
    DM_SceneRenderHook* newSceneRender(
		DM_VPortAgent& i_viewport,
		DM_SceneHookType i_type,
		DM_SceneHookPolicy i_policy) override;

    /// Called when a viewport no longer requires the render hook.
    virtual void retireSceneRender(
		DM_VPortAgent& i_viewport,
		DM_SceneRenderHook* io_hook) override;

	/**
		\brief Adds relevant camera, layer, screen and driver nodes to NSI scene.
		
		Also keeps a pointer to the NSI context so the camera node could be
		updated when necessary.
		
		Any previously connected context will be disconnected and its render,
		terminated.
	*/
	void connect(NSI::Context* io_nsi);

	/**
		\brief Disconnects from the currently connected NSI context. 
		
		It also disconnects all known render hooks from their display driver.
		
		This will eventually terminate the associated renders since the drivers
		will close.
	*/
	void disconnect();

private:

	/// Constructor
	viewport_hook_builder();

	// Not implemented
	viewport_hook_builder(const viewport_hook_builder&);
	const viewport_hook_builder& operator=(const viewport_hook_builder&);

	// List of active render hooks
	std::vector<dm_SetupTask*> m_hooks;
	// Current rendering context
	NSI::Context* m_nsi{nullptr};
};
