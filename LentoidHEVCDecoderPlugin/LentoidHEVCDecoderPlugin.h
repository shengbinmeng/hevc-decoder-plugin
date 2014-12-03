#pragma once

#include "mfxplugin++.h"
#include <memory.h>
#include<stddef.h>
#include "lenthevcdec.h"
#include<list>
#include "surface_auto_lock.h"
#include "LentoidHEVCDecoder.h"

int AU_COUNT=0;

class DecoderTask
{
public:
	DecoderTask():
		m_bsIn(),
		m_pOut(NULL),
		m_coreAndData(NULL, NULL),
	    m_lenthevcdec()
	{
	}
	DecoderTask(mfxBitstream *bs_in,lenthevcdec_ctx ctx,mfxFrameSurface1 *frame_out, MFXCoreInterface &pCore, FFScaler *scl,LentoidHEVCDecoder &dec)
		:m_bsIn(bs_in? *bs_in:mfxBitstream())
		,m_ctx(ctx)
		,m_pOut(frame_out)
		,m_bBusy(!bs_in)
		,m_coreAndData(&pCore, &frame_out->Data)
		,m_scl(scl)
		,m_lenthevcdec(&dec)
	{
	}
	mfxStatus operator () () {          
		// Task is run once and saved status
		if (m_executeSts)
			return m_executeSts;

		//copy ctor implementation is absent, and this buffer will point to wrong location, if initialized in curent ctor
		if (!m_bsData.empty()) {
			m_bsIn.Data = &m_bsData.front();
		}

		mfxFrameSurface1 surface;

		//do the decoding
		m_executeSts = m_lenthevcdec->DecodeBS(m_bsIn ,m_ctx,surface,AU_COUNT);  //decode one AU
		if (m_executeSts != MFX_ERR_NONE) {
			return m_executeSts;
		}

		SurfaceAutoLock lock(m_coreAndData.m_pCore->FrameAllocator(), *m_pOut);
		if (MFX_ERR_NONE != lock) {
			return lock;
		}
		//need to transfer data from HEVC internal buffers to mediasdk surface
		if(surface.Data.Y!=NULL)
		{
			m_executeSts = m_lenthevcdec->ConvertSurface(*m_scl,surface, *m_pOut);
			return m_executeSts;
		}
	}

protected:
	LentoidHEVCDecoder       *m_lenthevcdec;
	lenthevcdec_ctx			  m_ctx;
	FFScaler				 *m_scl;
	mfxBitstream		      m_bsIn;
	bool					  m_bBusy;
	mfxFrameSurface1		 *m_pOut;
	mfxFrameAllocator		 *m_pAlloc;
	std::vector<mfxU8>		  m_bsData;
	struct ExecuteStatus {
		std::pair<bool, mfxStatus> m_CompletedSts;
		ExecuteStatus & operator = (mfxStatus sts) {
			m_CompletedSts.first = true;
			m_CompletedSts.second = sts;
			return *this;
		}
		operator mfxStatus & () {
			m_CompletedSts.first = true;
			return m_CompletedSts.second;
		}
		operator const mfxStatus & () const {
			return m_CompletedSts.second;
		}
		operator bool() {
			return m_CompletedSts.first;
		}
	} m_executeSts;
	struct CoreAndData
	{
		MFXCoreInterface *m_pCore;
		mfxFrameData *m_pData;
		CoreAndData(MFXCoreInterface *pCore, mfxFrameData *pData)
			: m_pCore(pCore)
			, m_pData(pData) {
				if (m_pCore) {
					m_pCore->IncreaseReference(m_pData);
				}
		}
		CoreAndData(const CoreAndData & that)
			: m_pCore(that.m_pCore)
			, m_pData(that.m_pData) {
				if (m_pCore) {
					m_pCore->IncreaseReference(m_pData);
				}
		}
		~CoreAndData()
		{
			if (m_pCore) {
				m_pCore->DecreaseReference(m_pData);
			}
		}
	}m_coreAndData;
};

class LentoidHEVCDecoderPlugin : public MFXDecoderPlugin
{
public:
	LentoidHEVCDecoderPlugin();
	virtual ~LentoidHEVCDecoderPlugin();

	// mfxPlugin functions
	virtual mfxStatus PluginInit(mfxCoreInterface *core);
	virtual mfxStatus PluginClose();
	virtual mfxStatus GetPluginParam(mfxPluginParam *par);
	virtual mfxStatus Execute(mfxThreadTask task, mfxU32 uid_p, mfxU32 uid_a);
	virtual mfxStatus FreeResources(mfxThreadTask task, mfxStatus sts);
	//mfxVideoCodecPlugin functions
	virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);
	virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *out);
	virtual mfxStatus Init(mfxVideoParam *mfxParam);
	virtual mfxStatus Reset(mfxVideoParam *par)
	{
		return MFX_ERR_NONE;
	}
	virtual mfxStatus Close();
	virtual mfxStatus GetVideoParam(mfxVideoParam *par)
	{
		return MFX_ERR_NONE;
	}
	virtual void Release()
	{
		delete this;
	}
	static  MFXDecoderPlugin* Create()
	{
		return new LentoidHEVCDecoderPlugin();
	}
	virtual mfxStatus SetAuxParams(void* , int )
	{
		return MFX_ERR_UNKNOWN;
	}
	virtual mfxStatus DecodeHeader(mfxBitstream *bs, mfxVideoParam *par);
	virtual mfxStatus GetPayload(mfxU64 *, mfxPayload *) {
		return MFX_ERR_NONE;
	}
	virtual mfxStatus DecodeFrameSubmit(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out,  mfxThreadTask *task);
	virtual mfxStatus DecoderFlush(lenthevcdec_ctx ctx);	

protected:
	bool								 m_bInited;
	bool								 m_bIsOutOpaque;
	MFXCoreInterface				     m_mfxCore;
	FFScaler						     m_scl;
	mfxVideoParam					     m_VideoParam;
	mfxU32								 m_MaxNumTasks;
	lenthevcdec_ctx					     m_decoder;
	std::list<DecoderTask>				 m_pTasks;
	std::auto_ptr<MFXPluginParam>        m_PluginParam;

};