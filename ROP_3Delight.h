#ifndef __ROP_3Delight_h__
#define __ROP_3Delight_h__

#include "safe_interest.h"
#include "ui/aov.h"
#include "ui/settings.h"

#include <ROP/ROP_Node.h>

#include <nsi.hpp>

#include <mutex>
#include <thread>
#include <vector>

namespace NSI { class Context; class DynamicAPI; }

class context;
class OBJ_Camera;
class OBJ_Node;
class UT_ReadWritePipe;
class UT_String;
class exporter;

class ROP_3Delight : public ROP_Node
{
	friend class settings; // UI related.
public:

	/**
		Image layer (convoluted) file output modes.
	*/
	enum e_fileOutputMode
	{
		e_disabled,
		e_useToggleStates,
		e_allFilesAndSelectedJpeg,
		e_useToggleAndFramebufferStates
	};

	/// Registers the 3Delight ROP
	static void Register(OP_OperatorTable* io_table);
	static OP_Node* alloc(OP_Network* net, const char* name, OP_Operator* op);
	static OP_Node* cloud_alloc(OP_Network* net, const char* name, OP_Operator* op);

	/** \brief Returns true if motion blur is enabled. */
	bool HasMotionBlur()const;
	virtual void onCreated();

protected:

	ROP_3Delight(
		OP_Network* net,
		const char* name,
		OP_Operator* entry,
		bool i_cloud);
	virtual ~ROP_3Delight();

	virtual int startRender(int nframes, fpreal s, fpreal e);
	virtual ROP_RENDER_CODE renderFrame(fpreal time, UT_Interrupt* boss);
	virtual ROP_RENDER_CODE endRender();

	// Used to enable some buttons.
	virtual bool updateParmsFlags();

	virtual void loadFinished();

private:
	static void register_mplay_driver( NSI::DynamicAPI &i_api );

	std::string AtmosphereAttributesHandle() const;
	void ExportAtmosphere(const context& i_ctx)const;
	void ExportOutputs(const context& i_ctx)const;
	void ExportOneOutputLayer(
		const context& i_ctx,
		const std::string& i_layer_handle,
		const aov::description& i_desc,
		const UT_String& i_scalar_format,
		const UT_String& i_filter,
		double i_filter_width,
		const std::string& i_screen_handle,
		const std::string& i_light_handle,
		const std::string& i_driver_handle,
		unsigned& io_sort_key) const;

	void ExportLayerFeedbackData(
		const context& i_ctx,
		const std::string& i_layer_handle,
		const std::string& i_light_handle) const;

	void ExportGlobals(const context& i_ctx)const;
	void ExportDefaultMaterial( const context &i_context ) const;

	/**
		\brief Builds a unique image name for image format which don't support
		multi-layers (png and jpeg)
	*/
	void BuildImageUniqueName(
		const UT_String& i_image_file_name,
		const std::string& i_light_name,
		const std::string& i_aov_token,
		const char* i_extension,
		UT_String& o_image_unique_name) const;

	/**
		\brief Returns the light's token for the specified index
	*/
	const char* GetLightToken(int index) const;

	const char* GetUseLightToken(int index) const;

	bool HasSpeedBoost()const;
	float GetResolutionFactor()const;
	float GetSamplingFactor()const;
	int GetPixelSamples()const;
	OBJ_Camera* GetCamera()const;
	float GetShutterInterval(float i_time)const;
	bool HasDepthOfField()const;
	UT_String GetObjectsToRender()const;
	UT_String GetLightsToRender()const;

	std::vector<OBJ_Node*> m_lights;
	bool m_cloud;

	context* m_current_render;
	/*
		NSI context used to export NSI scenes (and render single frames in a
		background thread).
	*/
	NSI::Context m_nsi;

	/*
		List of interests (callbacks) created in IPR mode.
		We don't use a vector because there might be a lot of items (1 or 2 per
		node) and the re-allocation pattern of std::vector implies copying its
		into a bigger array. In the case of our safe_interest class, this would
		mean additional connections (in copy-constructor) and disconnections (in
		destructor) from nodes, so the callbacks point to the right
		safe_interest object.
	*/
	std::deque<safe_interest, std::allocator<safe_interest> > m_interests;

	// renderdl process rendering a list of NSI files being read from stdin
	UT_ReadWritePipe* m_renderdl;
	/*
		A thread that waits for the m_renderdl process to finish so we could be
		notified of it.
	*/
	std::thread m_renderdl_waiter;

	/*
		True while rendering is in progress.
		When a render is interrupted explicitly, it's set to false immediately
		in order to prevent m_nsi to be invalidated by the stopper callback.
	*/
	bool m_rendering;

	/*
		Mutex controlling access to m_rendering and m_renderdl, specifically
		when handling various end-of-render situations.
	*/
	std::mutex m_render_end_mutex;

	/* The UI part of the ROP */
	settings m_settings;
};

#endif
