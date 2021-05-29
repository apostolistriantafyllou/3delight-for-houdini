#ifndef __ROP_3Delight_h__
#define __ROP_3Delight_h__

#include "safe_interest.h"
#include "ui/aov.h"
#include "ui/settings.h"

#include <ROP/ROP_Node.h>
#include <OP/OP_Operator.h>

#include <nsi.hpp>

#include <deque>
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
class time_notifier;

class ROP_3Delight : public ROP_Node
{
	friend class settings; // UI related.
	friend class camera;

public:

	// Registers the 3Delight ROP
	static void Register(OP_OperatorTable* io_table);
	static OP_Node* alloc(OP_Network* net, const char* name, OP_Operator* op);
	static OP_Node* cloud_alloc(OP_Network* net, const char* name, OP_Operator* op);
	static OP_Node* standin_alloc(OP_Network* net, const char* name, OP_Operator* op);
	static OP_Node* viewport_alloc(OP_Network* net, const char* name, OP_Operator* op);

	/** \brief Returns true if motion blur is enabled. */
	bool HasMotionBlur( double i_time )const;

	/**
		\brief Manually restarts rendering at the request of i-display.

		\param i_time
			Time at which the scene should be sampled.
		\param i_ipr
			Indicates whether an editable/live render was requested.
		\param i_window
			Rectangular window to be used as crop or priority window for the new
			render.
	*/
	void StartRenderFromIDisplay(
		double i_time,
		bool i_ipr,
		const float* i_window);

	/**
		Updates the IPR's crop window from i-display. This happens when
		3Delight Display calls us with a crop window update.
	*/
	void UpdateCrop(const float* i_window);

	bool idisplay_rendering( void ) const { return m_idisplay_rendering; }
	const float *idisplay_crop_window( void ) const
	{
		return &m_idisplay_rendering_window[0];
	}

	/// Stops any current rendering session from this ROP
	void StopRender();

	virtual unsigned maxInputs() const;

	virtual unsigned maxOutputs() const;
	virtual unsigned getNumVisibleOutputs() const;

	/**
		\brief Returns the current time.
	*/
	double current_time( void ) const;

	/**
		\brief Returns the name of the file into which to export an NSI stream.

		It might also return "stdout", which is valid in NSI, or an empty
		string, which means to render instead of exporting.
	*/
	std::string GetNSIExportFilename( double i_time ) const;

	/// Called by creation_callbacks when a new OBJ is created in IPR
	void NewOBJNode(OBJ_Node& i_node);

	const settings &get_settings( void ) const
	{
		return m_settings;
	}

protected:

	ROP_3Delight(
		OP_Network* net,
		const char* name,
		OP_Operator* entry,
		rop_type i_rop_type);
	virtual ~ROP_3Delight();

	virtual int startRender(int nframes, fpreal s, fpreal e);
	virtual ROP_RENDER_CODE renderFrame(fpreal time, UT_Interrupt* boss);
	virtual ROP_RENDER_CODE endRender();

	// Used to enable some buttons.
	virtual bool updateParmsFlags();

	virtual void loadFinished();

    virtual void resolveObsoleteParms(PRM_ParmList*);

	virtual void opChanged(OP_EventType reason, void* data);

protected:
	/** Returns the resolution multiplier from Speed Boost */
	float GetResolutionFactor()const;

	/** Returns the pixel samples (AA oversampling) in this ROP */
	int GetPixelSamples()const;

private:

	/// Sets "render_mode" parameter based on "export_nsi" and "ipr"
	void resolve_obsolete_render_mode(PRM_ParmList* i_old_parms);

	/**
		\brief Called when the ROP changes in any way during an IPR render.

		This is used to detect changes that require updating the scene in IPR.
		An atmosphere shader assignment is an example of this.

		It could have been implemented by overriding virtual function
		OP_Node::opChanged instead, but this would have been less convenient,
		mostly because opChanged could be called at any time, even when not
		rendering and even when the ROP is being deleted. This could cause
		serious synchronization and validation problems.
	*/
	static void changed_cb(
		OP_Node* i_caller,
		void* i_callee,
		OP_EventType i_type,
		void* i_data);

	void export_render_notes( const context &i_context ) const;

	void ExportTransparentSurface(const context& i_ctx) const;
	void ExportAtmosphere(const context& i_ctx, bool ipr_update = false);
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
		const std::string& i_driver_name,
		unsigned& io_sort_key) const;

	void ExportLayerFeedbackData(
		const context& i_ctx,
		const std::string& i_layer_handle,
		const std::string& i_light_category,
		const std::vector<OBJ_Node*>& i_lights) const;

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
		\brief Exports light categories, either bundles or single lights.

		Each category name is returned with the list of light nodes.
	*/
	void ExportLightCategories(
		const context& i_ctx,
		std::map<std::string, std::vector<OBJ_Node*>> &,
		fpreal t) const;

	bool HasSpeedBoost( double i_time )const;

	/** volumesm can be disabled in speed boost */
	bool atmosphere_enabled( double t ) const;
	double multi_scatter_multipier( double t ) const;

	/// Retrieves the image resolution, scaled by the Speed Boost res factor
	bool GetScaledResolution(int& o_x, int& o_y)const;

	/** Returns the sampling multiplier from Speed Boost */
	float GetSamplingFactor()const;

	/** Returns camera used to Render. */
	OBJ_Camera* GetCamera( double i_time )const;

	double GetShutterInterval(double i_time)const;

	bool HasDepthOfField( double i_time )const;

	/// Called when the application's current time has changed.
	void time_change_cb(double i_time);

private:
	std::vector<OBJ_Node*> m_lights;
	rop_type m_rop_type;

	context* m_current_render{nullptr};

	/// "end time" parameter of the last call to startRender()
	double m_end_time;

	/*
		NSI context used to export NSI scenes (and render single frames in a
		background thread).
	*/
	NSI::Context m_nsi;
	/*
		NSI context used to export non-animated attributes. It might use the
		same internal context handle as m_nsi when static attributes don't need
		to be exported separately. It might also be invalid when non-animated
		simply don't need to be exported again.
	*/
	NSI::Context m_static_nsi;
	/*
		Name of the file where m_static_nsi outputs its NSI stream, when it has
		a different context handle than m_nsi.
	*/
	std::string m_static_nsi_file;

	// renderdl process rendering a list of NSI files being read from stdin
	UT_ReadWritePipe* m_renderdl;
	/*
		A thread that waits for the m_renderdl process to finish so we could be
		notified of it.
	*/
	std::thread m_renderdl_waiter;

	/*
		Notifies the ROP of time changes during an IPR render, so that it can
		re-export the time-dependent components of the scene.
	*/
	time_notifier* m_time_notifier;

	/*
		True while rendering is in progress.
		When a render is interrupted explicitly, it's set to false immediately
		in order to prevent m_nsi to be invalidated by the stopper callback.
	*/
	bool m_rendering;

	// Indicates that rendering was requested by i-display
	bool m_idisplay_rendering;

	// Indicates that i-display is requesting an IPR render
	bool m_idisplay_ipr;

	// Crop or priority window used only when rendering from i-display
	float m_idisplay_rendering_window[4];

	/*
		Mutex controlling access to m_rendering and m_renderdl, specifically
		when handling various end-of-render situations.
	*/
	std::mutex m_render_end_mutex;

	/* The UI part of the ROP */
	settings m_settings;

	// Indicates that the crop has been made from i-display
	bool m_is_cropped_on_iDisplay = false;

};

struct ROP_3DelightOperator : public OP_Operator
{
	/// Constructor.
	ROP_3DelightOperator(rop_type i_rop_type);

	// overriding function which is responsible for help URL
	virtual bool getOpHelpURL(UT_String& url);

};

#endif
