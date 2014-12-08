#pragma once

#include "mfxstructures.h"
#include "lenthevcdec.h"
extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
}

#define AU_COUNT_MAX (1024 * 64)

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
	unsigned int    m_au_pos[AU_COUNT_MAX];
	unsigned int	m_au_count;
	unsigned int	m_au_buf_size;
	int             m_got_frame;
	int				m_width;
	int				m_height;
	int				m_stride[3];
	void			*m_pixels[3];
	int64_t         m_pts;
	int64_t			m_got_pts;
};