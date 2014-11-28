#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include"LentoidHEVCDecoderPlugin.h"
#include "sample_defs.h"
#include "signal.h"

#define AU_COUNT_MAX	(1024 * 64)
namespace {
	const mfxU32 FF_CODEC_DECO = MFX_MAKEFOURCC('D','E','C','O');
	LentoidHEVCDecoder g_lenthevcdec;
}

//defining module template for decoder plugin
#include "mfx_plugin_module.h"
PluginModuleTemplate g_PluginModule = {
	LentoidHEVCDecoderPlugin::Create,
	NULL,
	NULL
};

LentoidHEVCDecoderPlugin::LentoidHEVCDecoderPlugin():                                     //构造函数
	m_bInited(false),
	m_bIsOutOpaque(false),
	m_VideoParam(),
	m_pTasks(NULL),
	m_decoder(NULL),
	m_MaxNumTasks(1)
{
	MSDK_ZERO_MEMORY(m_VideoParam);
	MSDK_ZERO_MEMORY(m_PluginParam);
	mfxPluginUID g_Decoder_PluginGuid ={0x16, 0xdd, 0x94, 0x68, 0x25, 0xad, 0x47, 0x5e, 0xa3, 0x4e, 0x35, 0xf3, 0xf5, 0x42, 0x17, 0xa6};
	m_PluginParam.reset(new MFXPluginParam(FF_CODEC_DECO, 0, g_Decoder_PluginGuid));
	if (m_PluginParam.get())
	{
		mfxPluginParam& param = ((mfxPluginParam&)(*m_PluginParam.get()));
		param.APIVersion.Major = MFX_VERSION_MAJOR;
		param.APIVersion.Minor = MFX_VERSION_MINOR;
	}
}

LentoidHEVCDecoderPlugin::~LentoidHEVCDecoderPlugin()                   //析构函数
{
	Close();
}

mfxStatus LentoidHEVCDecoderPlugin::PluginInit(mfxCoreInterface *core)
{
	MSDK_CHECK_POINTER(core, MFX_ERR_NULL_PTR);
	m_mfxCore=MFXCoreInterface(*core);
	return MFX_ERR_NONE;
}

mfxStatus LentoidHEVCDecoderPlugin::PluginClose()
{
	Close();
	return MFX_ERR_NONE;
}

mfxStatus LentoidHEVCDecoderPlugin::GetPluginParam(mfxPluginParam *par)   //获取Plugin信息
{
	MSDK_CHECK_POINTER(par, MFX_ERR_NULL_PTR);

	*par = *m_PluginParam;

	return MFX_ERR_NONE;
}

mfxStatus LentoidHEVCDecoderPlugin::DecodeFrameSubmit(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxThreadTask *task)
{
	MSDK_CHECK_POINTER(surface_work, MFX_ERR_NULL_PTR);
	MSDK_CHECK_POINTER(surface_out, MFX_ERR_NULL_PTR);
	MSDK_CHECK_POINTER(task, MFX_ERR_NULL_PTR);
	MSDK_CHECK_ERROR(m_bInited, false, MFX_ERR_NOT_INITIALIZED);

	mfxFrameSurface1 *real_surface_work = surface_work;
	*surface_out = surface_work;

	mfxStatus sts = MFX_ERR_NONE;

	if (m_pTasks.size()==m_MaxNumTasks)
	{
		return MFX_WRN_DEVICE_BUSY;
	}

	if (m_bIsOutOpaque)
	{
		sts = m_mfxCore.GetRealSurface(surface_work, &real_surface_work);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
	}

	m_pTasks.push_back(DecoderTask(bs,m_decoder,real_surface_work,m_mfxCore,&m_scl));

	*task = (mfxThreadTask)&m_pTasks.back();

	sts = m_pTasks.back()();
	// TODO: who is responsible to unqueue of task if err_more_data
	if (sts < MFX_ERR_NONE) {
		FreeResources(*task, sts);
		return sts;
	}

	return MFX_ERR_NONE;
}

mfxStatus LentoidHEVCDecoderPlugin::Execute(mfxThreadTask task, mfxU32 uid_p, mfxU32 uid_a)
{
	MSDK_CHECK_ERROR(m_bInited, false, MFX_ERR_NOT_INITIALIZED);

	mfxStatus sts=MFX_ERR_NONE;

	DecoderTask &current_task = *(DecoderTask  *)task;

	sts=current_task();

	AU_COUNT=AU_COUNT+1;  //next AU

	return sts;
}

mfxStatus LentoidHEVCDecoderPlugin::FreeResources(mfxThreadTask task, mfxStatus sts)
{
	MSDK_CHECK_ERROR(m_bInited, false, MFX_ERR_NOT_INITIALIZED);
	if (m_pTasks.empty()) {
		return MFX_ERR_UNKNOWN;
	}
	DecoderTask *current_task = (DecoderTask *)task;

	for (std::list<DecoderTask>::iterator i = m_pTasks.begin(); i != m_pTasks.end(); i++)
	{
		//pointer to task
		if (&*i == current_task) {
			m_pTasks.erase(i);
			return MFX_ERR_NONE;
		}
	}

	return MFX_ERR_NONE;
}
/*The mfxVideoParam structure contains configuration parameters for encoding, decoding,transcoding and video processing*/

mfxStatus LentoidHEVCDecoderPlugin::DecodeHeader(mfxBitstream *bs, mfxVideoParam *mfxParam)
{
	lenthevcdec_ctx ctx=NULL;
	mfxStatus sts=MFX_ERR_NONE;
	mfxFrameSurface1 srff={0};

	ctx=g_lenthevcdec.CreatDecoder(1, 120, NULL);
	sts=g_lenthevcdec.DecoderFirst(*bs,ctx,srff);
	g_lenthevcdec.ReleaseDecoder(ctx);
	if(sts==MFX_ERR_NONE)
	{
		mfxParam->mfx.FrameInfo=srff.Info;
	}
	return sts;
}

mfxStatus LentoidHEVCDecoderPlugin::DecoderFlush(lenthevcdec_ctx ctx)
{
	lenthevcdec_flush(ctx);

	return MFX_ERR_NONE;
}

mfxStatus LentoidHEVCDecoderPlugin::Init(mfxVideoParam *mfxParam)
{
	MSDK_CHECK_POINTER(mfxParam, MFX_ERR_NULL_PTR);
	mfxStatus sts = MFX_ERR_NONE;

	m_VideoParam = *mfxParam;
	mfxInfoMFX  m_pParm=mfxParam->mfx;

	m_bIsOutOpaque = (m_VideoParam.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY) ? true : false;
	mfxExtOpaqueSurfaceAlloc* pluginOpaqueAlloc = NULL;

	if (m_bIsOutOpaque)
	{
		pluginOpaqueAlloc = (mfxExtOpaqueSurfaceAlloc*)GetExtBuffer(m_VideoParam.ExtParam,
			m_VideoParam.NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
		MSDK_CHECK_POINTER(pluginOpaqueAlloc, MFX_ERR_INVALID_VIDEO_PARAM);
	}

	// check existence of corresponding allocs
	if (m_bIsOutOpaque && !pluginOpaqueAlloc->Out.Surfaces)
		return MFX_ERR_INVALID_VIDEO_PARAM;

	if (m_bIsOutOpaque)
	{
		sts = m_mfxCore.MapOpaqueSurface(pluginOpaqueAlloc->Out.NumSurface,
			pluginOpaqueAlloc->Out.Type, pluginOpaqueAlloc->Out.Surfaces);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, MFX_ERR_MEMORY_ALLOC);
	}

	m_MaxNumTasks=m_VideoParam.AsyncDepth;                        //maxTask

	if (m_MaxNumTasks < 1) m_MaxNumTasks = 1;

	m_decoder=g_lenthevcdec.CreatDecoder(1,120,NULL);  //open HEVC decoder
	m_bInited = true;

	return MFX_ERR_NONE;
}

mfxStatus LentoidHEVCDecoderPlugin::Close()
{
	if(!m_bInited)
		return MFX_ERR_NONE;

	mfxStatus sts = MFX_ERR_NONE;

	mfxExtOpaqueSurfaceAlloc* pluginOpaqueAlloc = NULL;

	if (m_bIsOutOpaque)
	{
		pluginOpaqueAlloc = (mfxExtOpaqueSurfaceAlloc*)
			GetExtBuffer(m_VideoParam.ExtParam, m_VideoParam.NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
		MSDK_CHECK_POINTER(pluginOpaqueAlloc, MFX_ERR_INVALID_VIDEO_PARAM);
	}

	if (m_bIsOutOpaque && !pluginOpaqueAlloc->Out.Surfaces)
		return MFX_ERR_INVALID_VIDEO_PARAM;

	if (m_bIsOutOpaque)
	{
		sts = m_mfxCore.UnmapOpaqueSurface(pluginOpaqueAlloc->Out.NumSurface,
			pluginOpaqueAlloc->Out.Type, pluginOpaqueAlloc->Out.Surfaces);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, MFX_ERR_MEMORY_ALLOC);
	}

	g_lenthevcdec.ReleaseDecoder(m_decoder);
	m_pTasks.clear();
	m_bInited = false;

	return MFX_ERR_NONE;
}

mfxStatus LentoidHEVCDecoderPlugin::Query(mfxVideoParam *in, mfxVideoParam *out)      //only expose this interface
{
	return MFX_ERR_NONE;
}

/*The function returns minimum and suggested numbers of the output frame surfaces
required for decoding initialization and their type.*/
mfxStatus LentoidHEVCDecoderPlugin::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *out)
{
	MSDK_CHECK_POINTER(par,MFX_ERR_NULL_PTR);
	MSDK_CHECK_POINTER(out,MFX_ERR_NULL_PTR);

	out->NumFrameSuggested = out->NumFrameMin+par->AsyncDepth;
	out->Info = par->mfx.FrameInfo;

	mfxCoreParam core_par;              //current session info
	mfxStatus sts = m_mfxCore.GetCoreParam(&core_par);

	mfxI32 isInternalManaging = (core_par.Impl & MFX_IMPL_SOFTWARE) ?
		(par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) : (par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY);

	if (isInternalManaging) {
		if (core_par.Impl & MFX_IMPL_SOFTWARE)
			out->Type = MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE;
		else
			out->Type = MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;
	} else {
		if (core_par.Impl & MFX_IMPL_HARDWARE)
			out->Type = MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE;
		else
			out->Type = MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;
	}

	if (par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
	{
		out->Type |= MFX_MEMTYPE_OPAQUE_FRAME;
	}
	else
	{
		out->Type |= MFX_MEMTYPE_EXTERNAL_FRAME;
	}

	return MFX_ERR_NONE;
}