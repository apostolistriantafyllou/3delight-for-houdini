#ifndef __ROP_3Delight_h__
#define __ROP_3Delight_h__

#include <ROP/ROP_Node.h>

class OP_TemplatePair;
class OP_VariablePair;


class ROP_3Delight : public ROP_Node
{
public:

	static OP_Node* alloc(OP_Network* net, const char* name, OP_Operator* op);

protected:

	ROP_3Delight(OP_Network* net, const char* name, OP_Operator* entry);
	virtual ~ROP_3Delight();

	virtual int startRender(int nframes, fpreal s, fpreal e);
	virtual ROP_RENDER_CODE renderFrame(fpreal time, UT_Interrupt* boss);
	virtual ROP_RENDER_CODE endRender();

private:

	fpreal end_time;
};

#endif
