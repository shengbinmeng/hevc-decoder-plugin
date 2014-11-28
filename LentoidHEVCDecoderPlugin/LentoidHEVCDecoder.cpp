#include "LentoidHEVCDecoder.h"
#include <iostream>
#include <limits>
#include "sample_defs.h"

#define AU_COUNT_MAX	(1024 * 64)

LentoidHEVCDecoder::LentoidHEVCDecoder()
{
}

LentoidHEVCDecoder::~LentoidHEVCDecoder()
{
	// av_lockmgr_register(NULL);
}

lenthevcdec_ctx LentoidHEVCDecoder::CreatDecoder(int threads, int compatibility, void* reserved)
{
	return lenthevcdec_create(threads, compatibility, reserved);
}

int LentoidHEVCDecoder::lent_hevc_get_frame(unsigned char* buf, int size, int *is_idr)
{
	static int seq_hdr = 0;
	int i, nal_type, idr = 0;
	for ( i = 0; i < (size - 6); i++ ) {
		if ( 0 == buf[i] && 0 == buf[i+1] && 1 == buf[i+2] ) {
			nal_type = (buf[i+3] & 0x7E) >> 1;
			if ( nal_type <= 21 ) {
				if ( buf[i+5] & 0x80 ) { /* first slice in pic */
					if ( !seq_hdr )
						break;
					else
						seq_hdr = 0;
				}
			}
			if ( nal_type >= 32 && nal_type <= 34 ) {
				if ( !seq_hdr ) {
					seq_hdr = 1;
					idr = 1;
					break;
				}
				seq_hdr = 1;
			}
			i += 2;
		}
	}
	if ( i == (size - 6) )
		i = size;
	if ( NULL != is_idr )
		*is_idr = idr;
	return i;
}

mfxStatus LentoidHEVCDecoder::DecodeBS(mfxBitstream &bs, lenthevcdec_ctx ctx,mfxFrameSurface1 &srf,int au_number)
{
	//unsigned char *au_buf;
	void *pixels[3];
	int ret,frame_count=0;
	int got_frame, width, height, stride[3],i=0;
	unsigned int au_pos[AU_COUNT_MAX], au_count, au_buf_size;
	int64_t pts,got_pts;

	au_buf_size = bs.DataLength;

	// find all AU 
	au_count = 0;
	for ( i = 0; i < au_buf_size; i+=3 ) {
		i += lent_hevc_get_frame(bs.Data + i, au_buf_size - i, NULL);
		if ( i < au_buf_size )
			au_pos[au_count++] = i;
	}
	au_pos[0] = 0; // include SEI 
	au_pos[au_count] = au_buf_size; //include last AU 

	memset(&srf, 0, sizeof(srf)); //clean srf

	//decoding
	if(au_number<au_count)
	{
		got_frame = 0;
		pts=au_number*40;
		ret = lenthevcdec_decode_frame(ctx, bs.Data + au_pos[au_number], au_pos[au_number + 1] - au_pos[au_number], pts,
			&got_frame, &width, &height, stride, (void**)pixels, &got_pts);
		if ( ret < 0 ) {
			fprintf(stderr, "lenthevcdec_decode_frame failed! ret=%d\n", ret);
			return
				MFX_ERR_UNKNOWN;
		}

		if(ret>=0)
		{
			srf.Info.Height=height;
			srf.Info.CropH =height;
			srf.Info.Width=width;
			srf.Info.CropW=width;
			srf.Info.FourCC=MFX_FOURCC_YV12;
			srf.Data.Pitch=stride[0];
			srf.Data.Locked = 0;
			srf.Data.Corrupted = 0;
			srf.Data.DataFlag = 0;
			if(got_frame>0)
			{
				srf.Data.Y = (mfxU8 *)pixels[0];
				srf.Data.U = (mfxU8 *)pixels[1];
				srf.Data.V = (mfxU8 *)pixels[2];
			}
		}
	}
	//flush decoder
	if (au_number >= au_count) {
		got_frame = 0;
		ret = lenthevcdec_decode_frame(ctx, NULL, 0, pts,
			&got_frame, &width, &height, stride, (void**)pixels, &got_pts);
		if ( ret < 0 || got_frame <= 0) {
			return MFX_ERR_MORE_DATA;
		}

		srf.Info.Height=height;
		srf.Info.CropH =height;
		srf.Info.Width=width;
		srf.Info.CropW=width;
		srf.Info.FourCC=MFX_FOURCC_YV12;
		srf.Data.Pitch=stride[0];
		srf.Data.Locked = 0;
		srf.Data.Corrupted = 0;
		srf.Data.DataFlag = 0;
		srf.Data.Y = (mfxU8 *)pixels[0];
		srf.Data.U = (mfxU8 *)pixels[1];
		srf.Data.V = (mfxU8 *)pixels[2];

		frame_count++;
	}

	return MFX_ERR_NONE;
}

mfxStatus LentoidHEVCDecoder::DecoderFirst(mfxBitstream &bs, lenthevcdec_ctx ctx, mfxFrameSurface1 &srf)
{
	//unsigned char *au_buf;
	void *pixels[3];
	int ret=1,frame_count=0;
	int got_frame, width, height, stride[3],i=0;
	unsigned int au_pos[1024 * 64], au_count, au_buf_size;
	int64_t pts,got_pts;

	au_buf_size = bs.DataLength;

	// find all AU 
	au_count = 0;
	for ( i = 0; i < au_buf_size; i+=3 )
	{
		i += lent_hevc_get_frame(bs.Data + i, au_buf_size - i, NULL);
		if ( i < au_buf_size )
			au_pos[au_count++] = i;
	}
	au_pos[0] = 0; // include SEI 
	au_pos[au_count] = au_buf_size; // include last AU 

	//fill the surface INFO
	i=0;
	got_frame = 0;
	pts=i*40;
	ret = lenthevcdec_decode_frame(ctx, bs.Data + au_pos[i], au_pos[i + 1] - au_pos[i], pts,
		&got_frame, &width, &height, stride, (void**)pixels, &got_pts);
	if ( ret < 0 ) {
		fprintf(stderr, "lenthevcdec_decode_frame failed! ret=%d\n", ret);
		return
			MFX_ERR_UNKNOWN;
	}
	if ( ret >= 0 )
	{
		srf.Info.Height=height;
		srf.Info.CropH =height;
		srf.Info.Width=width;
		srf.Info.CropW=width;
		srf.Info.FourCC=MFX_FOURCC_YV12;
	}

	return MFX_ERR_NONE;
}

mfxStatus LentoidHEVCDecoder::ConvertSurface(FFScaler &scl,const mfxFrameSurface1& surfaceIn, mfxFrameSurface1 & surfaceOut)
{
	//data pointers check
	if (surfaceIn.Data.Y == NULL)
	{
		MSDK_TRACE_ERROR( "0 == pSurfaceIn->Data.MemId");
		return MFX_ERR_UNSUPPORTED;
	}
	if (surfaceOut.Data.Y == NULL)
	{
		MSDK_TRACE_ERROR( "0 == pSurfaceOut->Data.MemId");
		return MFX_ERR_UNSUPPORTED;
	}
	AVPixelFormat inFmt;
	AVPixelFormat outFmt = AV_PIX_FMT_NONE;

	switch(surfaceIn.Info.FourCC)
	{
	case MFX_FOURCC_NV12 :
		inFmt = PIX_FMT_NV12;
		break;
	case MFX_FOURCC_YV12 :
		inFmt = PIX_FMT_YUV420P;
		break;
	case MFX_FOURCC_YUY2 :
		inFmt = PIX_FMT_YUYV422;
		break;
	case MFX_FOURCC_RGB3 :
		inFmt = PIX_FMT_BGR24;
		break;
	case MFX_FOURCC_RGB4 :
		inFmt = PIX_FMT_ARGB;
		break;
	default:
		return MFX_ERR_UNSUPPORTED;
	}

	switch(surfaceOut.Info.FourCC)
	{
	case MFX_FOURCC_NV12 :
		outFmt = PIX_FMT_NV12;
		break;
	case MFX_FOURCC_YV12 :
		outFmt = PIX_FMT_YUV420P;
		break;
	case MFX_FOURCC_YUY2 :
		outFmt = PIX_FMT_YUYV422;
		break;
	case MFX_FOURCC_RGB3 :
		outFmt = PIX_FMT_BGR24;
		break;
	case MFX_FOURCC_RGB4 :
		outFmt = PIX_FMT_ARGB;
		break;
	default:
		return MFX_ERR_UNSUPPORTED;
	}

	scl.pCtx = sws_getCachedContext(scl.pCtx
		, surfaceIn.Info.CropW
		, surfaceIn.Info.CropH
		, inFmt
		, surfaceOut.Info.CropW
		, surfaceOut.Info.CropH
		, outFmt
		, SWS_BILINEAR
		, NULL, NULL, NULL);

	if (NULL == scl.pCtx) {
		MSDK_TRACE_ERROR( "sws_getCachedContext() returned NULL");
		return MFX_ERR_UNSUPPORTED;
	}
	//setting up chroma shifts
	uint32_t chroma_subsampling_in[AV_NUM_DATA_POINTERS]  = {0};
	uint32_t chroma_subsampling_out[AV_NUM_DATA_POINTERS]  = {0};
	{
		int v_shift, h_shift;
		avcodec_get_chroma_sub_sample(inFmt, &h_shift, &v_shift );
		for (size_t i = 1; i < AV_NUM_DATA_POINTERS; i++) {
			chroma_subsampling_in[i] = h_shift;
		}
		avcodec_get_chroma_sub_sample(outFmt, &h_shift, &v_shift );
		for (size_t i = 1; i < AV_NUM_DATA_POINTERS; i++) {
			chroma_subsampling_out[i] = h_shift;
		}
	}
	//all mediasdk planes possible only 4
	uint8_t *srcSlice[] = {surfaceIn.Data.Y, surfaceIn.Data.UV, surfaceIn.Data.Cr, surfaceIn.Data.A};
	uint8_t *dstSlice[] = {surfaceOut.Data.Y, surfaceOut.Data.UV, surfaceOut.Data.Cr, surfaceOut.Data.A};

	int srcStride[AV_NUM_DATA_POINTERS] = {};
	int dstStride[AV_NUM_DATA_POINTERS] = {};

	for(int j = 0; j < _countof(srcStride); j++) {
		srcStride[j] = surfaceIn.Data.Pitch >> chroma_subsampling_in[j];
		dstStride[j] = surfaceOut.Data.Pitch >> chroma_subsampling_out[j];
	}

	int h = sws_scale(scl.pCtx
		, srcSlice
		, srcStride
		, 0
		, surfaceIn.Info.CropH
		, dstSlice
		, dstStride);
	if (h != surfaceOut.Info.CropH ) {
		MSDK_TRACE_ERROR( "sws_scale() returned " << h<<", while output slice height=" << surfaceOut.Info.CropH);
		return MFX_ERR_UNSUPPORTED;
	}

	return MFX_ERR_NONE;
}

void LentoidHEVCDecoder::ReleaseDecoder(lenthevcdec_ctx ctx)
{
	lenthevcdec_destroy(ctx);
}