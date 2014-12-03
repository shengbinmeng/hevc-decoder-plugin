#pragma once

#include "mfxstructures.h"
#include "lenthevcdec.h"
extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
}

typedef void* lenthevcdec_ctx;
#define AU_COUNT_MAX	(1024 * 64)

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
	int LentHEVCGetFrame(unsigned char* buf, int size, int *is_idr) ;
	lenthevcdec_ctx CreatDecoder(int threads, int compatibility, void* reserved);
	mfxStatus DecodeBS(mfxBitstream &bs, lenthevcdec_ctx ctx,mfxFrameSurface1 &srf,int au_number);
	mfxStatus DecoderFirst(mfxBitstream &bs, lenthevcdec_ctx ctx, mfxFrameSurface1 &srf);
	mfxStatus ConvertSurface(FFScaler &scl,const mfxFrameSurface1& surfaceIn, mfxFrameSurface1 & surfaceOut);
	void     ReleaseDecoder(lenthevcdec_ctx ctx);

protected:
	unsigned int    au_pos[AU_COUNT_MAX];
	unsigned int	au_count;
	unsigned int	au_buf_size;
	int             got_frame;
	int				width;
	int				height;
	int				stride[3];
	void			*pixels[3];
	int64_t         pts;
	int64_t			got_pts;
};