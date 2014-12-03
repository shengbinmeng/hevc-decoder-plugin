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
	int GetFrame(unsigned char* buf, int size, int *is_idr) ;
	lenthevcdec_ctx CreatDecoder(int threads, int compatibility, void* reserved);
	mfxStatus DecodeBS(mfxBitstream &bs, lenthevcdec_ctx ctx,mfxFrameSurface1 &srf,int au_number);
	mfxStatus DecoderFirst(mfxBitstream &bs, lenthevcdec_ctx ctx, mfxFrameSurface1 &srf);
	mfxStatus ConvertSurface(FFScaler &scl,const mfxFrameSurface1& surfaceIn, mfxFrameSurface1 & surfaceOut);
	void     ReleaseDecoder(lenthevcdec_ctx ctx);

protected:
	unsigned int    p_au_pos[AU_COUNT_MAX];
	unsigned int	p_au_count;
	unsigned int	p_au_buf_size;
	int             p_got_frame;
	int				p_width;
	int				p_height;
	int				p_stride[3];
	void			*p_pixels[3];
	int64_t         p_pts;
	int64_t			p_got_pts;
};