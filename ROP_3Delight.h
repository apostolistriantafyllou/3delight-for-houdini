#ifndef __ROP_3Delight_h__
#define __ROP_3Delight_h__

#include "ui/aov.h"

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
public:

	/// Registers the 3Delight ROP
	static void Register(OP_OperatorTable* io_table);
	static OP_Node* alloc(OP_Network* net, const char* name, OP_Operator* op);

	/** \brief Returns true if motion blur is enabled. */
	bool HasMotionBlur()const;
	virtual void onCreated();

	static int image_format_cb(void* data, int index, fpreal t,
								const PRM_Template* tplate);
	static int aov_clear_cb(void* data, int index, fpreal t,
							const PRM_Template* tplate);
	static int add_layer_cb(void* data, int index, fpreal t,
						   const PRM_Template* tplate);
	static int refresh_lights_cb(void* data, int index, fpreal t,
								const PRM_Template* tplate);

protected:

	ROP_3Delight(OP_Network* net, const char* name, OP_Operator* entry);
	virtual ~ROP_3Delight();

	virtual int startRender(int nframes, fpreal s, fpreal e);
	virtual ROP_RENDER_CODE renderFrame(fpreal time, UT_Interrupt* boss);
	virtual ROP_RENDER_CODE endRender();
	// Used to enable some buttons.
	virtual bool updateParmsFlags();
	/// Makes the "Render to MPlay" button visible.
	virtual bool isPreviewAllowed();
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

	void BuildImageJpegName(
		const UT_String& i_image_file_name,
		const std::string& i_light_name,
		const std::string& i_aov_token,
		UT_String& o_image_jpeg_name) const;

	// Update UI lights from scene lights.
	void UpdateLights();
	// Gets the light names from the selected one
	void GetSelectedLights(std::vector<std::string>& o_light_names) const;

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
};

#endif
