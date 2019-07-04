#ifndef __ROP_3Delight_h__
#define __ROP_3Delight_h__

#include <ROP/ROP_Node.h>

namespace NSI { class Context; }

class ROP_3Delight : public ROP_Node
{
public:

	static OP_Node* alloc(OP_Network* net, const char* name, OP_Operator* op);

	bool HasMotionBlur()const;

	void ExportGlobals(NSI::Context& io_nsi)const;

protected:

	ROP_3Delight(OP_Network* net, const char* name, OP_Operator* entry);
	virtual ~ROP_3Delight();

	virtual int startRender(int nframes, fpreal s, fpreal e);
	virtual ROP_RENDER_CODE renderFrame(fpreal time, UT_Interrupt* boss);
	virtual ROP_RENDER_CODE endRender();

	void export_scene( void );

	void process_obj(
		OP_Node *node, std::vector<OBJ_Node*> &o_to_export );
	void export_obj( OBJ_Node * );
	void scan_obj( OP_Network *i_network );

private:

	bool HasSpeedBoost()const;
	float GetResolutionFactor()const;
	float GetSamplingFactor()const;

	fpreal m_end_time;
};

#endif
