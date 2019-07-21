#ifndef __ROP_3Delight_h__
#define __ROP_3Delight_h__

#include <ROP/ROP_Node.h>

namespace NSI { class Context; class DynamicAPI; }

class context;
class OBJ_Camera;
class exporter;

class ROP_3Delight : public ROP_Node
{
public:

	/// Registers the 3Delight ROP
	static void Register(OP_OperatorTable* io_table);
	static OP_Node* alloc(OP_Network* net, const char* name, OP_Operator* op);

	/** \brief Returns true if motion blur is enabled. */
	bool HasMotionBlur()const;

	static int add_layer_cb(void* data, int index, fpreal t,
						   const PRM_Template* tplate);
	static int remove_layer_cb(void* data, int index, fpreal t,
								const PRM_Template* tplate);
	static int duplicate_layer_cb(void* data, int index, fpreal t,
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

private:
	static void register_mplay_driver( NSI::DynamicAPI &i_api );

	void ExportOutputs(const context& i_ctx)const;
	void ExportGlobals(const context& i_ctx)const;
	void ExportDefaultMaterial( const context &i_context ) const;

	bool HasSpeedBoost()const;
	float GetResolutionFactor()const;
	float GetSamplingFactor()const;
	int GetPixelSamples()const;
	OBJ_Camera* GetCamera()const;
	float GetShutterInterval(float i_time)const;
	bool HasDepthOfField()const;

	fpreal m_end_time;
};

#endif
