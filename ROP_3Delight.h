#ifndef __ROP_3Delight_h__
#define __ROP_3Delight_h__

#include "ui/aov.h"
#include "ui/settings.h"

#include <ROP/ROP_Node.h>

#include <vector>

namespace NSI { class Context; class DynamicAPI; }

class context;
class OBJ_Camera;
class OBJ_Node;
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

	/** \brief Returns true if motion blur is enabled. */
	bool HasMotionBlur()const;
	virtual void onCreated();

protected:

	ROP_3Delight(OP_Network* net, const char* name, OP_Operator* entry);
	virtual ~ROP_3Delight();

	virtual int startRender(int nframes, fpreal s, fpreal e);
	virtual ROP_RENDER_CODE renderFrame(fpreal time, UT_Interrupt* boss);
	virtual ROP_RENDER_CODE endRender();

	// Used to enable some buttons.
	virtual bool updateParmsFlags();

	virtual void loadFinished();

private:
	static void register_mplay_driver( NSI::DynamicAPI &i_api );

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

	fpreal m_end_time;
	std::vector<OBJ_Node*> m_lights;

	/* The UI part of the ROP */
	settings m_settings;
};

#endif
