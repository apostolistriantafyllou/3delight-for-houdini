#ifndef __ROP_3Delight_h__
#define __ROP_3Delight_h__

#include <ROP/ROP_Node.h>

namespace NSI { class Context; }

class context;
class OBJ_Camera;
class exporter;

class ROP_3Delight : public ROP_Node
{
public:

	static OP_Node* alloc(OP_Network* net, const char* name, OP_Operator* op);

	/** \brief Returns true if motion blur is enabled. */
	bool HasMotionBlur()const;

protected:

	ROP_3Delight(OP_Network* net, const char* name, OP_Operator* entry);
	virtual ~ROP_3Delight();

	virtual int startRender(int nframes, fpreal s, fpreal e);
	virtual ROP_RENDER_CODE renderFrame(fpreal time, UT_Interrupt* boss);
	virtual ROP_RENDER_CODE endRender();

private:

	void ExportGlobals(const context& i_ctx)const;

	bool HasSpeedBoost()const;
	float GetResolutionFactor()const;
	float GetSamplingFactor()const;
	OBJ_Camera* GetCamera()const;
	float GetShutterInterval(float i_time)const;

	fpreal m_end_time;
};

#endif
