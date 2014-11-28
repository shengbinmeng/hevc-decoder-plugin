#pragma once

#include "mfxstructures.h"
#include "lenthevcdec.h"
extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
}

typedef void* lenthevcdec_ctx;

struct FFScaler
{
	bool bInitialized;
	SwsContext * pCtx;
	FFScaler()
		: bInitialized()
		, pCtx()
	{
	}
};

class LentoidHEVCDecoder
{
public:
	LentoidHEVCDecoder();
	~LentoidHEVCDecoder();
	int lent_hevc_get_frame(unsigned char* buf, int size, int *is_idr) ;
	lenthevcdec_ctx CreatDecoder(int threads, int compatibility, void* reserved);
	mfxStatus DecodeBS(mfxBitstream &bs, lenthevcdec_ctx ctx,mfxFrameSurface1 &srf,int au_number);
	mfxStatus DecoderFirst(mfxBitstream &bs, lenthevcdec_ctx ctx, mfxFrameSurface1 &srf);
	mfxStatus ConvertSurface(FFScaler &scl,const mfxFrameSurface1& surfaceIn, mfxFrameSurface1 & surfaceOut);
	void     ReleaseDecoder(lenthevcdec_ctx ctx);
};