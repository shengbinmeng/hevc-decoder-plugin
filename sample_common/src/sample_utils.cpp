/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2005-2014 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#include "mfx_samples_config.h"

#include <math.h>
#include <iostream>
//#include <process.h>

#include "vm/strings_defs.h"
#include "sample_defs.h"
#include "sample_utils.h"
#include "mfxcommon.h"
#include "mfxjpeg.h"

#pragma warning( disable : 4748 )

msdk_tick CTimer::frequency = 0;

CSmplYUVReader::CSmplYUVReader()
{
    m_bInited = false;
    m_bIsMultiView = false;
    m_fSource = NULL;
    m_fSourceMVC = NULL;
    m_numLoadedFiles = 0;
    m_ColorFormat = MFX_FOURCC_YV12;
}

mfxStatus CSmplYUVReader::Init(const msdk_char *strFileName, const mfxU32 ColorFormat, const mfxU32 numViews, std::vector<msdk_char*> srcFileBuff)
{
    MSDK_CHECK_POINTER(strFileName, MFX_ERR_NULL_PTR);
    MSDK_CHECK_ERROR(msdk_strlen(strFileName), 0, MFX_ERR_NULL_PTR);

    Close();

    //open source YUV file
    if (!m_bIsMultiView)
    {
        MSDK_FOPEN(m_fSource, strFileName, MSDK_STRING("rb"));
        MSDK_CHECK_POINTER(m_fSource, MFX_ERR_NULL_PTR);
        ++m_numLoadedFiles;
    }
    else if (m_bIsMultiView)
    {
        if (srcFileBuff.size() > 1)
        {
            if (srcFileBuff.size() != numViews)
                return MFX_ERR_UNSUPPORTED;

            mfxU32 i;
            m_fSourceMVC = new FILE*[numViews];
            for (i = 0; i < numViews; ++i)
            {
                MSDK_FOPEN(m_fSourceMVC[i], srcFileBuff.at(i), MSDK_STRING("rb"));
                MSDK_CHECK_POINTER(m_fSourceMVC[i], MFX_ERR_NULL_PTR);
                ++m_numLoadedFiles;
            }
        }
        else
        {
            /*mfxU32 i;
            m_fSourceMVC = new FILE*[numViews];
            for (i = 0; i < numViews; ++i)
            {
                MSDK_FOPEN(m_fSourceMVC[i], FormMVCFileName(strFileName, i).c_str(), MSDK_STRING("rb"));
                MSDK_CHECK_POINTER(m_fSourceMVC[i], MFX_ERR_NULL_PTR);
                ++m_numLoadedFiles;
            }*/
            return MFX_ERR_UNSUPPORTED;
        }
    }

    //set init state to true in case of success
    m_bInited = true;

    if (ColorFormat == MFX_FOURCC_NV12 || ColorFormat == MFX_FOURCC_YV12)
    {
        m_ColorFormat = ColorFormat;
    }
    else
    {
        return MFX_ERR_UNSUPPORTED;
    }

    return MFX_ERR_NONE;
}

CSmplYUVReader::~CSmplYUVReader()
{
    Close();
}

void CSmplYUVReader::Close()
{
    if (m_fSource)
    {
        fclose(m_fSource);
        m_fSource = NULL;

    }

    if (m_fSourceMVC)
    {
        mfxU32 i = 0;
        for (i = 0; i < m_numLoadedFiles; ++i)
        {
            if  (m_fSourceMVC[i] != NULL)
            {
                fclose(m_fSourceMVC[i]);
                m_fSourceMVC[i] = NULL;
            }
        }
    }

    m_numLoadedFiles = 0;
    m_bInited = false;
}

mfxStatus CSmplYUVReader::LoadNextFrame(mfxFrameSurface1* pSurface)
{
    // check if reader is initialized
    MSDK_CHECK_ERROR(m_bInited, false, MFX_ERR_NOT_INITIALIZED);
    MSDK_CHECK_POINTER(pSurface, MFX_ERR_NULL_PTR);

    mfxU32 nBytesRead;
    mfxU16 w, h, i, pitch;
    mfxU8 *ptr, *ptr2;
    mfxFrameInfo& pInfo = pSurface->Info;
    mfxFrameData& pData = pSurface->Data;

    mfxU32 vid = pInfo.FrameId.ViewId;

    // this reader supports only NV12 mfx surfaces for code transparency,
    // other formats may be added if application requires such functionality
    if (MFX_FOURCC_NV12 != pInfo.FourCC && MFX_FOURCC_YV12 != pInfo.FourCC)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    if (pInfo.CropH > 0 && pInfo.CropW > 0)
    {
        w = pInfo.CropW;
        h = pInfo.CropH;
    }
    else
    {
        w = pInfo.Width;
        h = pInfo.Height;
    }

    pitch = pData.Pitch;
    ptr = pData.Y + pInfo.CropX + pInfo.CropY * pData.Pitch;

    // read luminance plane
    for(i = 0; i < h; i++)
    {
        if (!m_bIsMultiView)
        {
            nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w, m_fSource);
        }
        else
        {
            nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w, m_fSourceMVC[vid]);
        }
        if (w != nBytesRead)
        {
            return MFX_ERR_MORE_DATA;
        }
    }

    // read chroma planes
    switch (m_ColorFormat) // color format of data in the input file
    {
    case MFX_FOURCC_YV12: // YUV420 is implied
        switch (pInfo.FourCC)
        {
        case MFX_FOURCC_NV12:

            mfxU8 buf[2048]; // maximum supported chroma width for nv12
            mfxU32 j;
            w /= 2;
            h /= 2;
            ptr = pData.UV + pInfo.CropX + (pInfo.CropY / 2) * pitch;
            if (w > 2048)
            {
                return MFX_ERR_UNSUPPORTED;
            }
            // load U
            for (i = 0; i < h; i++)
            {
                if (!m_bIsMultiView)
                {
                    nBytesRead = (mfxU32)fread(buf, 1, w, m_fSource);
                }
                else
                {
                    nBytesRead = (mfxU32)fread(buf, 1, w, m_fSourceMVC[vid]);
                }
                if (w != nBytesRead)
                {
                    return MFX_ERR_MORE_DATA;
                }
                for (j = 0; j < w; j++)
                {
                    ptr[i * pitch + j * 2] = buf[j];
                }
            }
            // load V
            for (i = 0; i < h; i++)
            {
                if (!m_bIsMultiView)
                {
                    nBytesRead = (mfxU32)fread(buf, 1, w, m_fSource);
                }
                else
                {
                    nBytesRead = (mfxU32)fread(buf, 1, w, m_fSourceMVC[vid]);
                }
                if (w != nBytesRead)
                {
                    return MFX_ERR_MORE_DATA;
                }
                for (j = 0; j < w; j++)
                {
                    ptr[i * pitch + j * 2 + 1] = buf[j];
                }
            }

            break;
        case MFX_FOURCC_YV12:
            w /= 2;
            h /= 2;
            pitch /= 2;

            ptr  = pData.U + (pInfo.CropX / 2) + (pInfo.CropY / 2) * pitch;
            ptr2 = pData.V + (pInfo.CropX / 2) + (pInfo.CropY / 2) * pitch;

            for(i = 0; i < h; i++)
            {
                if (!m_bIsMultiView)
                {
                    nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w, m_fSource);
                }
                else
                {
                    nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w, m_fSourceMVC[vid]);
                }
                if (w != nBytesRead)
                {
                    return MFX_ERR_MORE_DATA;
                }
            }
            for(i = 0; i < h; i++)
            {
                if (!m_bIsMultiView)
                {
                    nBytesRead = (mfxU32)fread(ptr2 + i * pitch, 1, w, m_fSource);
                }
                else
                {
                    nBytesRead = (mfxU32)fread(ptr2 + i * pitch, 1, w, m_fSourceMVC[vid]);
                }
                if (w != nBytesRead)
                {
                    return MFX_ERR_MORE_DATA;
                }
            }

            break;
        default:
            return MFX_ERR_UNSUPPORTED;
            }
        break;
    case MFX_FOURCC_NV12:
        h /= 2;
        ptr  = pData.UV + pInfo.CropX + (pInfo.CropY / 2) * pitch;
        for(i = 0; i < h; i++)
        {
            if (!m_bIsMultiView)
            {
                nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w, m_fSource);
            }
            else
            {
                nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w, m_fSourceMVC[vid]);
            }
            if (w != nBytesRead)
            {
                return MFX_ERR_MORE_DATA;
            }
        }

        break;
    default:
        return MFX_ERR_UNSUPPORTED;
    }

    return MFX_ERR_NONE;
}

CSmplBitstreamWriter::CSmplBitstreamWriter()
{
    m_fSource = NULL;
    m_bInited = false;
    m_nProcessedFramesNum = 0;
}

CSmplBitstreamWriter::~CSmplBitstreamWriter()
{
    Close();
}

void CSmplBitstreamWriter::Close()
{
    if (m_fSource)
    {
        fclose(m_fSource);
        m_fSource = NULL;
    }

    m_bInited = false;
    m_nProcessedFramesNum = 0;
}

mfxStatus CSmplBitstreamWriter::Init(const msdk_char *strFileName)
{
    MSDK_CHECK_POINTER(strFileName, MFX_ERR_NULL_PTR);
    MSDK_CHECK_ERROR(msdk_strlen(strFileName), 0, MFX_ERR_NOT_INITIALIZED);

    Close();

    //init file to write encoded data
    MSDK_FOPEN(m_fSource, strFileName, MSDK_STRING("wb+"));
    MSDK_CHECK_POINTER(m_fSource, MFX_ERR_NULL_PTR);

	
   
    //set init state to true in case of success
    m_bInited = true;
    return MFX_ERR_NONE;
}

mfxStatus CSmplBitstreamWriter::WriteNextFrame(mfxBitstream *pMfxBitstream, bool isPrint)
{
    // check if writer is initialized
    MSDK_CHECK_ERROR(m_bInited, false, MFX_ERR_NOT_INITIALIZED);
    MSDK_CHECK_POINTER(pMfxBitstream, MFX_ERR_NULL_PTR);

    mfxU32 nBytesWritten = 0;

    nBytesWritten = (mfxU32)fwrite(pMfxBitstream->Data + pMfxBitstream->DataOffset, 1, pMfxBitstream->DataLength, m_fSource);
    MSDK_CHECK_NOT_EQUAL(nBytesWritten, pMfxBitstream->DataLength, MFX_ERR_UNDEFINED_BEHAVIOR);

    // mark that we don't need bit stream data any more
    pMfxBitstream->DataLength = 0;

    m_nProcessedFramesNum++;

    // print encoding progress to console every certain number of frames (not to affect performance too much)
    if (isPrint && (1 == m_nProcessedFramesNum  || (0 == (m_nProcessedFramesNum % 100))))
    {
        msdk_printf(MSDK_STRING("Frame number: %u\r"), m_nProcessedFramesNum);
    }

    return MFX_ERR_NONE;
}


CSmplBitstreamDuplicateWriter::CSmplBitstreamDuplicateWriter()
    : CSmplBitstreamWriter()
{
    m_fSourceDuplicate = NULL;
    m_bJoined = false;
}

mfxStatus CSmplBitstreamDuplicateWriter::InitDuplicate(const msdk_char *strFileName)
{
    MSDK_CHECK_POINTER(strFileName, MFX_ERR_NULL_PTR);
    MSDK_CHECK_ERROR(msdk_strlen(strFileName), 0, MFX_ERR_NOT_INITIALIZED);

    if (m_fSourceDuplicate)
    {
        fclose(m_fSourceDuplicate);
        m_fSourceDuplicate = NULL;
    }
    MSDK_FOPEN(m_fSourceDuplicate, strFileName, MSDK_STRING("wb+"));
    MSDK_CHECK_POINTER(m_fSourceDuplicate, MFX_ERR_NULL_PTR);

    m_bJoined = false; // mark we own the file handle

    return MFX_ERR_NONE;
}

mfxStatus CSmplBitstreamDuplicateWriter::JoinDuplicate(CSmplBitstreamDuplicateWriter *pJoinee)
{
    MSDK_CHECK_POINTER(pJoinee, MFX_ERR_NULL_PTR);
    MSDK_CHECK_ERROR(pJoinee->m_fSourceDuplicate, NULL, MFX_ERR_NOT_INITIALIZED);

    m_fSourceDuplicate = pJoinee->m_fSourceDuplicate;
    m_bJoined = true; // mark we do not own the file handle

    return MFX_ERR_NONE;
}

mfxStatus CSmplBitstreamDuplicateWriter::WriteNextFrame(mfxBitstream *pMfxBitstream, bool isPrint)
{
    MSDK_CHECK_ERROR(m_fSourceDuplicate, NULL, MFX_ERR_NOT_INITIALIZED);
    MSDK_CHECK_POINTER(pMfxBitstream, MFX_ERR_NULL_PTR);

    mfxU32 nBytesWritten = (mfxU32)fwrite(pMfxBitstream->Data + pMfxBitstream->DataOffset, 1, pMfxBitstream->DataLength, m_fSourceDuplicate);
    MSDK_CHECK_NOT_EQUAL(nBytesWritten, pMfxBitstream->DataLength, MFX_ERR_UNDEFINED_BEHAVIOR);

    CSmplBitstreamWriter::WriteNextFrame(pMfxBitstream, isPrint);

    return MFX_ERR_NONE;
}

void CSmplBitstreamDuplicateWriter::Close()
{
    if (m_fSourceDuplicate && !m_bJoined)
    {
        fclose(m_fSourceDuplicate);
    }

    m_fSourceDuplicate = NULL;
    m_bJoined = false;

    CSmplBitstreamWriter::Close();
}

CSmplBitstreamReader::CSmplBitstreamReader()
{
    m_fSource = NULL;
    m_bInited = false;
}

CSmplBitstreamReader::~CSmplBitstreamReader()
{
    Close();
}

void CSmplBitstreamReader::Close()
{
    if (m_fSource)
    {
        fclose(m_fSource);
        m_fSource = NULL;
    }

    m_bInited = false;
}

void CSmplBitstreamReader::Reset()
{
    fseek(m_fSource, 0, SEEK_SET);
}

mfxStatus CSmplBitstreamReader::Init(const msdk_char *strFileName)
{
    MSDK_CHECK_POINTER(strFileName, MFX_ERR_NULL_PTR);
    MSDK_CHECK_ERROR(msdk_strlen(strFileName), 0, MFX_ERR_NOT_INITIALIZED);

    Close();

    //open file to read input stream
   MSDK_FOPEN(m_fSource, strFileName, MSDK_STRING("rb"));
 

    MSDK_CHECK_POINTER(m_fSource, MFX_ERR_NULL_PTR);

	 m_bInited = true;
	 return MFX_ERR_NONE;
}

mfxStatus CSmplBitstreamReader::ReadNextFrame(mfxBitstream *pBS)
{
    MSDK_CHECK_POINTER(pBS, MFX_ERR_NULL_PTR);
    MSDK_CHECK_ERROR(m_bInited, false, MFX_ERR_NOT_INITIALIZED);

    mfxU32 nBytesRead = 0;

    memmove(pBS->Data, pBS->Data + pBS->DataOffset, pBS->DataLength);
    pBS->DataOffset = 0;
    nBytesRead = (mfxU32)fread(pBS->Data + pBS->DataLength, 1, pBS->MaxLength - pBS->DataLength, m_fSource);

    if (0 == nBytesRead)
    {
        return MFX_ERR_MORE_DATA;
    }

    pBS->DataLength += nBytesRead;

    return MFX_ERR_NONE;
}

CH264FrameReader::CH264FrameReader()
{
    MSDK_ZERO_MEMORY(m_lastBs);
}

int CH264FrameReader::FindSlice(mfxBitstream *pBS, int & pos2ndnalu)
{
    int nNalu = 0;
    size_t i = 0;
    for (i = pBS->DataOffset; nNalu < 2 && i + 3 < pBS->DataOffset + pBS->DataLength; i++)
    {
        if (pBS->Data[i]   == 0 &&
            pBS->Data[i+1] == 0 &&
            pBS->Data[i+2] == 1 )
        {
            if (0 == nNalu)
            {
                int nType = pBS->Data[i+3] & 0x1F;

                if (nType == 1 ||//slice
                    nType == 5  )//IDR slice
                    nNalu++;
            }
            else
            {
                //any backend nalu
                nNalu ++;
            }
        }
    }
    if (nNalu == 2)
    {
        pos2ndnalu = (int)i;
    }
    return nNalu;
}

inline bool isPathDelimiter(msdk_char c)
{
    return c == '/' || c == '\\';
}

mfxStatus MoveMfxBitstream(mfxBitstream *pTarget, mfxBitstream *pSrc, mfxU32 nBytes)
{
    MSDK_CHECK_POINTER(pTarget, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(pSrc, MFX_ERR_NULL_PTR);

    mfxU32 nFreeSpaceTail = pTarget->MaxLength - pTarget->DataOffset - pTarget->DataLength;
    mfxU32 nFreeSpace = pTarget->MaxLength - pTarget->DataLength;

    MSDK_CHECK_NOT_EQUAL(pSrc->DataLength >= nBytes, true, MFX_ERR_MORE_DATA);
    MSDK_CHECK_NOT_EQUAL(nFreeSpace >= nBytes, true, MFX_ERR_NOT_ENOUGH_BUFFER);

    if (nFreeSpaceTail < nBytes)
    {
        memmove(pTarget->Data, pTarget->Data + pTarget->DataOffset,  pTarget->DataLength);
        pTarget->DataOffset = 0;
    }
    MSDK_MEMCPY_BITSTREAM(*pTarget, pTarget->DataOffset, pSrc->Data + pSrc->DataOffset, nBytes);
    pTarget->DataLength += nBytes;
    pSrc->DataLength -= nBytes;
    pSrc->DataOffset += nBytes;

    return MFX_ERR_NONE;
}

mfxStatus CH264FrameReader::ReadNextFrame(mfxBitstream *pBS)
{
    MSDK_CHECK_POINTER(pBS, MFX_ERR_NULL_PTR);
    pBS->DataFlag = MFX_BITSTREAM_COMPLETE_FRAME;

    if (m_lastBs.Data == NULL)
    {
        //alloc same bitstream
        m_bsBuffer.resize(pBS->MaxLength);
        m_lastBs.Data =  &m_bsBuffer.front();
        m_lastBs.MaxLength = pBS->MaxLength;
    }

    //checking for available nalu
    int nNalu;
    int pos2ndNaluStart = 0;
    //check nalu in input bs, it always=1 if decoder didnt take a frame
    if ((nNalu = FindSlice(pBS, pos2ndNaluStart)) < 1)
    {
        //copy nalu from internal buffer
        if ((nNalu = FindSlice(&m_lastBs, pos2ndNaluStart)) < 2)
        {
            mfxStatus sts = CSmplBitstreamReader::ReadNextFrame(&m_lastBs);
            if (MFX_ERR_MORE_DATA == sts)
            {
                //lets feed last nalu if present
                if (nNalu == 1)
                {
                    sts = MoveMfxBitstream(pBS, &m_lastBs, m_lastBs.DataLength);
                    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

                    return MFX_ERR_NONE;
                }

                return MFX_ERR_MORE_DATA;
            }
            //buffer is to small to accept whole frame
            MSDK_CHECK_NOT_EQUAL(FindSlice(&m_lastBs, pos2ndNaluStart) == 2, true, MFX_ERR_NOT_ENOUGH_BUFFER);
        }
        mfxU32 naluLen = pos2ndNaluStart-m_lastBs.DataOffset;
        mfxStatus sts = MoveMfxBitstream(pBS, &m_lastBs, naluLen);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    return MFX_ERR_NONE;
}

bool CJPEGFrameReader::SOImarkerIsFound(mfxBitstream *pBS)
{
    mfxU8 SOI_marker[] = { 0xFF, 0xD8 };
    for (mfxU32 i = pBS->DataOffset; i + sizeof(SOI_marker) <= pBS->DataOffset + pBS->DataLength; i++)
    {
        if ( !memcmp(pBS->Data + i, SOI_marker, sizeof(SOI_marker)) )
        {
            return true;
        }
    }
    return false;
}

bool CJPEGFrameReader::EOImarkerIsFound(mfxBitstream *pBS)
{
    mfxU8 EOI_marker[] = { 0xFF, 0xD9 };
    for (mfxU32 i = pBS->DataOffset; i + sizeof(EOI_marker) <= pBS->DataOffset + pBS->DataLength; i++)
    {
        if ( !memcmp(pBS->Data + i, EOI_marker, sizeof(EOI_marker)) )
        {
            return true;
        }
    }
    return false;
}

mfxStatus CJPEGFrameReader::ReadNextFrame(mfxBitstream *pBS)
{
    mfxStatus sts = MFX_ERR_NONE;
    bool isSOIMarkerFound = false, isEOIMarkerFound = false;

    pBS->DataFlag = MFX_BITSTREAM_COMPLETE_FRAME;

    isSOIMarkerFound = SOImarkerIsFound(pBS);
    while (!isSOIMarkerFound && sts == MFX_ERR_NONE)
    {
        sts = CSmplBitstreamReader::ReadNextFrame(pBS);
        isSOIMarkerFound = SOImarkerIsFound(pBS);
    }

    isEOIMarkerFound = EOImarkerIsFound(pBS);
    while (!isEOIMarkerFound && sts == MFX_ERR_NONE)
    {
        sts = CSmplBitstreamReader::ReadNextFrame(pBS);
        isEOIMarkerFound = EOImarkerIsFound(pBS);
    }

    return sts;
}

CIVFFrameReader::CIVFFrameReader()
{
    MSDK_ZERO_MEMORY(m_hdr);
}

#define READ_BYTES(pBuf, size)\
{\
    mfxU32 nBytesRead = (mfxU32)fread(pBuf, 1, size, m_fSource);\
    if (nBytesRead !=size)\
        return MFX_ERR_MORE_DATA;\
}\

mfxStatus CIVFFrameReader::Init(const msdk_char *strFileName)
{
    mfxStatus sts = CSmplBitstreamReader::Init(strFileName);
   MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // read and skip IVF header
    READ_BYTES(&m_hdr.dkif, sizeof(m_hdr.dkif));
    READ_BYTES(&m_hdr.version, sizeof(m_hdr.version));
    READ_BYTES(&m_hdr.header_len, sizeof(m_hdr.header_len));
    READ_BYTES(&m_hdr.codec_FourCC, sizeof(m_hdr.codec_FourCC));
    READ_BYTES(&m_hdr.width, sizeof(m_hdr.width));
    READ_BYTES(&m_hdr.height, sizeof(m_hdr.height));
    READ_BYTES(&m_hdr.frame_rate, sizeof(m_hdr.frame_rate));
    READ_BYTES(&m_hdr.time_scale, sizeof(m_hdr.time_scale));
    READ_BYTES(&m_hdr.num_frames, sizeof(m_hdr.num_frames));
    READ_BYTES(&m_hdr.unused, sizeof(m_hdr.unused));
    MSDK_CHECK_NOT_EQUAL(fseek(m_fSource, m_hdr.header_len, SEEK_SET), 0, MFX_ERR_UNSUPPORTED);

    // check header
    MSDK_CHECK_NOT_EQUAL(MFX_MAKEFOURCC('D','K','I','F'), m_hdr.dkif, MFX_ERR_UNSUPPORTED);
    MSDK_CHECK_NOT_EQUAL(MFX_MAKEFOURCC('V','P','8','0'), m_hdr.codec_FourCC, MFX_ERR_UNSUPPORTED);

    return MFX_ERR_NONE;
}

// reads a complete frame into given bitstream
mfxStatus CIVFFrameReader::ReadNextFrame(mfxBitstream *pBS)
{
    MSDK_CHECK_POINTER(pBS, MFX_ERR_NULL_PTR);
    pBS->DataFlag = MFX_BITSTREAM_COMPLETE_FRAME;

    /*bytes pos-(pos+3)                       size of frame in bytes (not including the 12-byte header)
      bytes (pos+4)-(pos+11)                  64-bit presentation timestamp
      bytes (pos+12)-(pos+12+nBytesInFrame)   frame data
    */

    mfxU32 nBytesInFrame = 0;
    mfxU64 nTimeStamp = 0;

    // read frame size
    READ_BYTES(&nBytesInFrame, sizeof(nBytesInFrame));
    // read time stamp
    READ_BYTES(&nTimeStamp, sizeof(nTimeStamp));
    //check if bitstream has enough space to hold the frame
    if (nBytesInFrame > pBS->MaxLength - pBS->DataLength - pBS->DataOffset)
        return MFX_ERR_NOT_ENOUGH_BUFFER;

    // read frame data
    READ_BYTES(pBS->Data + pBS->DataOffset + pBS->DataLength, nBytesInFrame);
    pBS->DataLength += nBytesInFrame;

    // it is application's responsibility to make sure the bitstream contains a single complete frame and nothing else
    // application has to provide input pBS with pBS->DataLength = 0

    return MFX_ERR_NONE;
}


CSmplYUVWriter::CSmplYUVWriter()
{
    m_bInited = false;
    m_bIsMultiView = false;
    m_fDest = NULL;
    m_fDestMVC = NULL;
    m_numCreatedFiles = 0;
};

mfxStatus CSmplYUVWriter::Init(const msdk_char *strFileName, const mfxU32 numViews)
{
    MSDK_CHECK_POINTER(strFileName, MFX_ERR_NULL_PTR);
    MSDK_CHECK_ERROR(msdk_strlen(strFileName), 0, MFX_ERR_NOT_INITIALIZED);

    Close();

    //open file to write decoded data

    if (!m_bIsMultiView)
    {
        MSDK_FOPEN(m_fDest, strFileName, MSDK_STRING("wb"));
        MSDK_CHECK_POINTER(m_fDest, MFX_ERR_NULL_PTR);
        ++m_numCreatedFiles;
    }
    else
    {
        mfxU32 i;

        MSDK_CHECK_ERROR(numViews, 0, MFX_ERR_NOT_INITIALIZED);

        m_fDestMVC = new FILE*[numViews];
        for (i = 0; i < numViews; ++i)
        {
            MSDK_FOPEN(m_fDestMVC[i], FormMVCFileName(strFileName, i).c_str(), MSDK_STRING("wb"));
            MSDK_CHECK_POINTER(m_fDestMVC[i], MFX_ERR_NULL_PTR);
            ++m_numCreatedFiles;
        }
    }

    m_bInited = true;

    return MFX_ERR_NONE;
}

CSmplYUVWriter::~CSmplYUVWriter()
{
    Close();
}

void CSmplYUVWriter::Close()
{
    if (m_fDest)
    {
        fclose(m_fDest);
        m_fDest = NULL;
    }

    if (m_fDestMVC)
    {
        mfxU32 i = 0;
        for (i = 0; i < m_numCreatedFiles; ++i)
        {
            if  (m_fDestMVC[i] != NULL)
            {
                fclose(m_fDestMVC[i]);
                m_fDestMVC[i] = NULL;
            }
        }
        delete m_fDestMVC;
        m_fDestMVC = NULL;
    }

    m_numCreatedFiles = 0;
    m_bInited = false;
}

mfxStatus CSmplYUVWriter::WriteNextFrame(mfxFrameSurface1 *pSurface)
{
    MSDK_CHECK_ERROR(m_bInited, false,   MFX_ERR_NOT_INITIALIZED);
    MSDK_CHECK_POINTER(pSurface,         MFX_ERR_NULL_PTR);

    mfxFrameInfo &pInfo = pSurface->Info;
    mfxFrameData &pData = pSurface->Data;

    mfxU32 i, j, h, w;
    mfxU32 vid = pInfo.FrameId.ViewId;

    if (!m_bIsMultiView)
    {
        MSDK_CHECK_POINTER(m_fDest, MFX_ERR_NULL_PTR);
    }
    else
    {
        MSDK_CHECK_POINTER(m_fDestMVC, MFX_ERR_NULL_PTR);
        MSDK_CHECK_POINTER(m_fDestMVC[vid], MFX_ERR_NULL_PTR);
    }

    switch (pInfo.FourCC)
    {
        case MFX_FOURCC_YV12:
        case MFX_FOURCC_NV12:
        for (i = 0; i < pInfo.CropH; i++)
        {
            if (!m_bIsMultiView)
            {
                MSDK_CHECK_NOT_EQUAL(
                    fwrite(pData.Y + (pInfo.CropY * pData.Pitch + pInfo.CropX)+ i * pData.Pitch, 1, pInfo.CropW, m_fDest),
                    pInfo.CropW, MFX_ERR_UNDEFINED_BEHAVIOR);
            }
            else
            {
                MSDK_CHECK_NOT_EQUAL(
                    fwrite(pData.Y + (pInfo.CropY * pData.Pitch + pInfo.CropX)+ i * pData.Pitch, 1, pInfo.CropW, m_fDestMVC[vid]),
                    pInfo.CropW, MFX_ERR_UNDEFINED_BEHAVIOR);
            }
        }
        break;
        case MFX_FOURCC_P010:
            for (i = 0; i < pInfo.CropH; i++)
            {
                MSDK_CHECK_NOT_EQUAL(
                    fwrite(pData.Y + (pInfo.CropY * pData.Pitch + pInfo.CropX)+ i * pData.Pitch, 1, (mfxU32)pInfo.CropW*2, m_fDest),
                    (mfxU32)pInfo.CropW*2, MFX_ERR_UNDEFINED_BEHAVIOR);
            }
        break;
        case MFX_FOURCC_RGB4:
        case MFX_FOURCC_A2RGB10:
            // Implementation for RGB4 and A2RGB10 in the next switch below
        break;

        default:
            return MFX_ERR_UNSUPPORTED;
    }
    switch (pInfo.FourCC)
    {
        case MFX_FOURCC_YV12:
        for (i = 0; i < (mfxU32) pInfo.CropH/2; i++)
        {
            MSDK_CHECK_NOT_EQUAL(
                fwrite(pData.U + (pInfo.CropY * pData.Pitch / 2 + pInfo.CropX / 2)+ i * pData.Pitch / 2, 1, pInfo.CropW/2, m_fDest),
                (mfxU32)pInfo.CropW/2, MFX_ERR_UNDEFINED_BEHAVIOR);
        }
        for (i = 0; i < (mfxU32)pInfo.CropH/2; i++)
        {
            MSDK_CHECK_NOT_EQUAL(
                fwrite(pData.V + (pInfo.CropY * pData.Pitch / 2 + pInfo.CropX / 2)+ i * pData.Pitch / 2, 1, pInfo.CropW/2, m_fDest),
                (mfxU32)pInfo.CropW/2, MFX_ERR_UNDEFINED_BEHAVIOR);
        }
        break;

        case MFX_FOURCC_NV12:
        h = pInfo.CropH / 2;
        w = pInfo.CropW;
        for (i = 0; i < h; i++)
        {
            for (j = 0; j < w; j += 2)
            {
                if (!m_bIsMultiView)
                {
                    MSDK_CHECK_NOT_EQUAL(
                        fwrite(pData.UV + (pInfo.CropY * pData.Pitch / 2 + pInfo.CropX) + i * pData.Pitch + j, 1, 1, m_fDest),
                        1, MFX_ERR_UNDEFINED_BEHAVIOR);
                }
                else
                {
                    MSDK_CHECK_NOT_EQUAL(
                        fwrite(pData.UV + (pInfo.CropY * pData.Pitch / 2 + pInfo.CropX) + i * pData.Pitch + j, 1, 1, m_fDestMVC[vid]),
                        1, MFX_ERR_UNDEFINED_BEHAVIOR);
                }
            }
        }
        for (i = 0; i < h; i++)
        {
            for (j = 1; j < w; j += 2)
            {
                if (!m_bIsMultiView)
                {
                    MSDK_CHECK_NOT_EQUAL(
                        fwrite(pData.UV + (pInfo.CropY * pData.Pitch / 2 + pInfo.CropX)+ i * pData.Pitch + j, 1, 1, m_fDest),
                        1, MFX_ERR_UNDEFINED_BEHAVIOR);
                }
                else
                {
                    MSDK_CHECK_NOT_EQUAL(
                        fwrite(pData.UV + (pInfo.CropY * pData.Pitch / 2 + pInfo.CropX)+ i * pData.Pitch + j, 1, 1, m_fDestMVC[vid]),
                        1, MFX_ERR_UNDEFINED_BEHAVIOR);
                }
            }
        }
        break;

        case MFX_FOURCC_P010:
            for (i = 0; i < (mfxU32)pInfo.CropH/2; i++)
            {
                MSDK_CHECK_NOT_EQUAL(
                    fwrite(pData.UV + (pInfo.CropY * pData.Pitch / 2 + pInfo.CropX) + i * pData.Pitch, 1, (mfxU32)pInfo.CropW*2, m_fDest),
                    (mfxU32)pInfo.CropW*2, MFX_ERR_UNDEFINED_BEHAVIOR);
            }
        break;

        case MFX_FOURCC_RGB4:
        case MFX_FOURCC_A2RGB10:
        mfxU8* ptr;

        if (pInfo.CropH > 0 && pInfo.CropW > 0)
        {
            w = pInfo.CropW;
            h = pInfo.CropH;
        }
        else
        {
            w = pInfo.Width;
            h = pInfo.Height;
        }

        ptr = MSDK_MIN( MSDK_MIN(pData.R, pData.G), pData.B);
        ptr = ptr + pInfo.CropX + pInfo.CropY * pData.Pitch;

        for(i = 0; i < h; i++)
        {
            MSDK_CHECK_NOT_EQUAL(
                fwrite(ptr + i * pData.Pitch, 1, 4*w, m_fDest),
                4*w, MFX_ERR_UNDEFINED_BEHAVIOR);
        }
        fflush(m_fDest);
        break;

    default:
        return MFX_ERR_UNSUPPORTED;
    }

    return MFX_ERR_NONE;
}

mfxStatus ConvertFrameRate(mfxF64 dFrameRate, mfxU32* pnFrameRateExtN, mfxU32* pnFrameRateExtD)
{
    MSDK_CHECK_POINTER(pnFrameRateExtN, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(pnFrameRateExtD, MFX_ERR_NULL_PTR);

    mfxU32 fr;

    fr = (mfxU32)(dFrameRate + .5);

    if (fabs(fr - dFrameRate) < 0.0001)
    {
        *pnFrameRateExtN = fr;
        *pnFrameRateExtD = 1;
        return MFX_ERR_NONE;
    }

    fr = (mfxU32)(dFrameRate * 1.001 + .5);

    if (fabs(fr * 1000 - dFrameRate * 1001) < 10)
    {
        *pnFrameRateExtN = fr * 1000;
        *pnFrameRateExtD = 1001;
        return MFX_ERR_NONE;
    }

    *pnFrameRateExtN = (mfxU32)(dFrameRate * 10000 + .5);
    *pnFrameRateExtD = 10000;

    return MFX_ERR_NONE;
}

mfxF64 CalculateFrameRate(mfxU32 nFrameRateExtN, mfxU32 nFrameRateExtD)
{
    if (nFrameRateExtN && nFrameRateExtD)
        return (mfxF64)nFrameRateExtN / nFrameRateExtD;
    else
        return 0;
}

mfxU16 GetFreeSurfaceIndex(mfxFrameSurface1* pSurfacesPool, mfxU16 nPoolSize)
{
    if (pSurfacesPool)
    {
        for (mfxU16 i = 0; i < nPoolSize; i++)
        {
            if (0 == pSurfacesPool[i].Data.Locked)
            {
                return i;
            }
        }
    }

    return MSDK_INVALID_SURF_IDX;
}

mfxU16 GetFreeSurface(mfxFrameSurface1* pSurfacesPool, mfxU16 nPoolSize)
{
    mfxU32 SleepInterval = 10; // milliseconds

    mfxU16 idx = MSDK_INVALID_SURF_IDX;

    //wait if there's no free surface
    for (mfxU32 i = 0; i < MSDK_WAIT_INTERVAL; i += SleepInterval)
    {
        idx = GetFreeSurfaceIndex(pSurfacesPool, nPoolSize);

        if (MSDK_INVALID_SURF_IDX != idx)
        {
            break;
        }
        else
        {
            MSDK_SLEEP(SleepInterval);
        }
    }

    return idx;
}

mfxU16 GetFreeSurfaceIndex(mfxFrameSurface1* pSurfacesPool, mfxU16 nPoolSize, mfxU16 step)
{
    if (pSurfacesPool)
    {
        for (mfxU16 i = 0; i < nPoolSize; i = (mfxU16)(i + step), pSurfacesPool += step)
        {
            if (0 == pSurfacesPool[0].Data.Locked)
            {
                return i;
            }
        }
    }

    return MSDK_INVALID_SURF_IDX;
}

mfxStatus InitMfxBitstream(mfxBitstream* pBitstream, mfxU32 nSize)
{
    //check input params
    MSDK_CHECK_POINTER(pBitstream, MFX_ERR_NULL_PTR);
    MSDK_CHECK_ERROR(nSize, 0, MFX_ERR_NOT_INITIALIZED);

    //prepare pBitstream
    WipeMfxBitstream(pBitstream);

   // prepare buffer
   pBitstream->Data = new mfxU8[nSize];
   MSDK_CHECK_POINTER(pBitstream->Data, MFX_ERR_MEMORY_ALLOC);

    pBitstream->MaxLength = nSize;

	

    return MFX_ERR_NONE;
}

mfxStatus ExtendMfxBitstream(mfxBitstream* pBitstream, mfxU32 nSize)
{
    MSDK_CHECK_POINTER(pBitstream, MFX_ERR_NULL_PTR);

    MSDK_CHECK_ERROR(nSize <= pBitstream->MaxLength, true, MFX_ERR_UNSUPPORTED);

    mfxU8* pData = new mfxU8[nSize];
    MSDK_CHECK_POINTER(pData, MFX_ERR_MEMORY_ALLOC);

    memmove(pData, pBitstream->Data + pBitstream->DataOffset, pBitstream->DataLength);

    WipeMfxBitstream(pBitstream);

    pBitstream->Data       = pData;
    pBitstream->DataOffset = 0;
    pBitstream->MaxLength  = nSize;

    return MFX_ERR_NONE;
}

void WipeMfxBitstream(mfxBitstream* pBitstream)
{
    MSDK_CHECK_POINTER(pBitstream);

    //free allocated memory
    MSDK_SAFE_DELETE_ARRAY(pBitstream->Data);
}

std::basic_string<msdk_char> CodecIdToStr(mfxU32 nFourCC)
{
    std::basic_string<msdk_char> fcc;
    for (size_t i = 0; i < 4; i++)
    {
        fcc.push_back((msdk_char)*(i + (char*)&nFourCC));
    }
    return fcc;
}

PartiallyLinearFNC::PartiallyLinearFNC()
: m_pX()
, m_pY()
, m_nPoints()
, m_nAllocated()
{
}

PartiallyLinearFNC::~PartiallyLinearFNC()
{
    delete []m_pX;
    m_pX = NULL;
    delete []m_pY;
    m_pY = NULL;
}

void PartiallyLinearFNC::AddPair(mfxF64 x, mfxF64 y)
{
    //duplicates searching
    for (mfxU32 i = 0; i < m_nPoints; i++)
    {
        if (m_pX[i] == x)
            return;
    }
    if (m_nPoints == m_nAllocated)
    {
        m_nAllocated += 20;
        mfxF64 * pnew;
        pnew = new mfxF64[m_nAllocated];
        //memcpy_s(pnew, sizeof(mfxF64)*m_nAllocated, m_pX, sizeof(mfxF64) * m_nPoints);
        MSDK_MEMCPY_BUF(pnew,0,sizeof(mfxF64)*m_nAllocated, m_pX,sizeof(mfxF64) * m_nPoints);
        delete [] m_pX;
        m_pX = pnew;

        pnew = new mfxF64[m_nAllocated];
        //memcpy_s(pnew, sizeof(mfxF64)*m_nAllocated, m_pY, sizeof(mfxF64) * m_nPoints);
        MSDK_MEMCPY_BUF(pnew,0,sizeof(mfxF64)*m_nAllocated, m_pY,sizeof(mfxF64) * m_nPoints);
        delete [] m_pY;
        m_pY = pnew;
    }
    m_pX[m_nPoints] = x;
    m_pY[m_nPoints] = y;

    m_nPoints ++;
}

mfxF64 PartiallyLinearFNC::at(mfxF64 x)
{
    if (m_nPoints < 2)
    {
        return 0;
    }
    bool bwasmin = false;
    bool bwasmax = false;

    mfxU32 maxx = 0;
    mfxU32 minx = 0;
    mfxU32 i;

    for (i=0; i < m_nPoints; i++)
    {
        if (m_pX[i] <= x && (!bwasmin || m_pX[i] > m_pX[maxx]))
        {
            maxx = i;
            bwasmin = true;
        }
        if (m_pX[i] > x && (!bwasmax || m_pX[i] < m_pX[minx]))
        {
            minx = i;
            bwasmax = true;
        }
    }

    //point on the left
    if (!bwasmin)
    {
        for (i=0; i < m_nPoints; i++)
        {
            if (m_pX[i] > m_pX[minx] && (!bwasmin || m_pX[i] < m_pX[minx]))
            {
                maxx = i;
                bwasmin = true;
            }
        }
    }
    //point on the right
    if (!bwasmax)
    {
        for (i=0; i < m_nPoints; i++)
        {
            if (m_pX[i] < m_pX[maxx] && (!bwasmax || m_pX[i] > m_pX[minx]))
            {
                minx = i;
                bwasmax = true;
            }
        }
    }

    //linear interpolation
    return (x - m_pX[minx])*(m_pY[maxx] - m_pY[minx]) / (m_pX[maxx] - m_pX[minx]) + m_pY[minx];
}

mfxU16 CalculateDefaultBitrate(mfxU32 nCodecId, mfxU32 nTargetUsage, mfxU32 nWidth, mfxU32 nHeight, mfxF64 dFrameRate)
{
    PartiallyLinearFNC fnc;
    mfxF64 bitrate = 0;

    switch (nCodecId)
    {
    case MFX_CODEC_AVC :
        {
            fnc.AddPair(0, 0);
            fnc.AddPair(25344, 225);
            fnc.AddPair(101376, 1000);
            fnc.AddPair(414720, 4000);
            fnc.AddPair(2058240, 5000);
            break;
        }
    case MFX_CODEC_MPEG2:
        {
            fnc.AddPair(0, 0);
            fnc.AddPair(414720, 12000);
            break;
        }
    default:
        {
            fnc.AddPair(0, 0);
            fnc.AddPair(414720, 12000);
            break;
        }
    }

    mfxF64 at = nWidth * nHeight * dFrameRate / 30.0;

    if (!at)
        return 0;

    switch (nTargetUsage)
    {
    case MFX_TARGETUSAGE_BEST_QUALITY :
        {
            bitrate = (&fnc)->at(at);
            break;
        }
    case MFX_TARGETUSAGE_BEST_SPEED :
        {
            bitrate = (&fnc)->at(at) * 0.5;
            break;
        }
    case MFX_TARGETUSAGE_BALANCED :
    default:
        {
            bitrate = (&fnc)->at(at) * 0.75;
            break;
        }
    }

    return (mfxU16)bitrate;
}

mfxU16 StrToTargetUsage(msdk_char* strInput)
{
    mfxU16 tu = MFX_TARGETUSAGE_BALANCED;

    if (0 == msdk_strcmp(strInput, MSDK_STRING("quality")))
    {
        tu = MFX_TARGETUSAGE_BEST_QUALITY;
    }
    else if (0 == msdk_strcmp(strInput, MSDK_STRING("speed")))
    {
        tu = MFX_TARGETUSAGE_BEST_SPEED;
    }

    return tu;
}

const msdk_char* TargetUsageToStr(mfxU16 tu)
{
    switch(tu)
    {
    case MFX_TARGETUSAGE_BALANCED:
        return MSDK_STRING("balanced");
    case MFX_TARGETUSAGE_BEST_QUALITY:
        return MSDK_STRING("quality");
    case MFX_TARGETUSAGE_BEST_SPEED:
        return MSDK_STRING("speed");
    case MFX_TARGETUSAGE_UNKNOWN:
        return MSDK_STRING("unknown");
    default:
        return MSDK_STRING("unsupported");
    }
}

const msdk_char* ColorFormatToStr(mfxU32 format)
{
    switch(format)
    {
    case MFX_FOURCC_NV12:
        return MSDK_STRING("NV12");
    case MFX_FOURCC_YV12:
        return MSDK_STRING("YUV420");
    default:
        return MSDK_STRING("unsupported");
    }
}

mfxU32 GCD(mfxU32 a, mfxU32 b)
{
    if (0 == a)
        return b;
    else if (0 == b)
        return a;

    mfxU32 a1, b1;

    if (a >= b)
    {
        a1 = a;
        b1 = b;
    }
    else
    {
        a1 = b;
        b1 = a;
    }

    // a1 >= b1;
    mfxU32 r = a1 % b1;

    while (0 != r)
    {
        a1 = b1;
        b1 = r;
        r = a1 % b1;
    }

    return b1;
}

mfxStatus DARtoPAR(mfxU32 darw, mfxU32 darh, mfxU32 w, mfxU32 h, mfxU16 *pparw, mfxU16 *pparh)
{
    MSDK_CHECK_POINTER(pparw, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(pparh, MFX_ERR_NULL_PTR);
    MSDK_CHECK_ERROR(darw, 0, MFX_ERR_UNDEFINED_BEHAVIOR);
    MSDK_CHECK_ERROR(darh, 0, MFX_ERR_UNDEFINED_BEHAVIOR);
    MSDK_CHECK_ERROR(w, 0, MFX_ERR_UNDEFINED_BEHAVIOR);
    MSDK_CHECK_ERROR(h, 0, MFX_ERR_UNDEFINED_BEHAVIOR);

    mfxU16 reduced_w = 0, reduced_h = 0;
    mfxU32 gcd = GCD(w, h);

    // divide by greatest common divisor to fit into mfxU16
    reduced_w =  (mfxU16) (w / gcd);
    reduced_h =  (mfxU16) (h / gcd);

    // for mpeg2 we need to set exact values for par (standard supports only dar 4:3, 16:9, 221:100 or par 1:1)
    if (darw * 3 == darh * 4)
    {
        *pparw = 4 * reduced_h;
        *pparh = 3 * reduced_w;
    }
    else if (darw * 9 == darh * 16)
    {
        *pparw = 16 * reduced_h;
        *pparh = 9 * reduced_w;
    }
    else if (darw * 100 == darh * 221)
    {
        *pparw = 221 * reduced_h;
        *pparh = 100 * reduced_w;
    }
    else if (darw * reduced_h == darh * reduced_w)
    {
        *pparw = 1;
        *pparh = 1;
    }
    else
    {
        *pparw = (mfxU16)((mfxF64)(darw * reduced_h) / (darh * reduced_w) * 1000);
        *pparh = 1000;
    }

    return MFX_ERR_NONE;
}

std::basic_string<msdk_char> FormMVCFileName(const msdk_char *strFileNamePattern, const mfxU32 numView)
{
    if (NULL == strFileNamePattern)
        return MSDK_STRING("");

    std::basic_string<msdk_char> fileName, mvcFileName, fileExt;
    fileName = strFileNamePattern;

    msdk_char postfixBuffer[4];
    msdk_itoa_decimal(numView, postfixBuffer);
    mvcFileName = fileName;
    mvcFileName.append(MSDK_STRING("_"));
    mvcFileName.append(postfixBuffer);
    mvcFileName.append(MSDK_STRING(".yuv"));

    return mvcFileName;
}

// function for getting a pointer to a specific external buffer from the array
mfxExtBuffer* GetExtBuffer(mfxExtBuffer** ebuffers, mfxU32 nbuffers, mfxU32 BufferId)
{
    if (!ebuffers) return 0;
    for(mfxU32 i=0; i<nbuffers; i++) {
        if (!ebuffers[i]) continue;
        if (ebuffers[i]->BufferId == BufferId) {
            return ebuffers[i];
        }
    }
    return 0;
}

mfxStatus MJPEG_AVI_ParsePicStruct(mfxBitstream *bitstream)
{
    // check input for consistency
    MSDK_CHECK_POINTER(bitstream->Data, MFX_ERR_MORE_DATA);
    if (bitstream->DataLength <= 0)
        return MFX_ERR_MORE_DATA;

    // define JPEG markers
    const mfxU8 APP0_marker [] = { 0xFF, 0xE0 };
    const mfxU8 SOI_marker  [] = { 0xFF, 0xD8 };
    const mfxU8 AVI1        [] = { 'A', 'V', 'I', '1' };

    // size of length field in header
    const mfxU8 len_size  = 2;
    // size of picstruct field in header
    const mfxU8 picstruct_size = 1;

    mfxU32 length = bitstream->DataLength;
    const mfxU8 *ptr = reinterpret_cast<const mfxU8*>(bitstream->Data);

    //search for SOI marker
    while ((length >= sizeof(SOI_marker)) && memcmp(ptr, SOI_marker, sizeof(SOI_marker)))
    {
        skip(ptr, length, (mfxU32)1);
    }

    // skip SOI
    if (!skip(ptr, length, (mfxU32)sizeof(SOI_marker)) || length < sizeof(APP0_marker))
        return MFX_ERR_MORE_DATA;

    // if there is no APP0 marker return
    if (memcmp(ptr, APP0_marker, sizeof(APP0_marker)))
    {
        bitstream->PicStruct = MFX_PICSTRUCT_UNKNOWN;
        return MFX_ERR_NONE;
    }

    // skip APP0 & length value
    if (!skip(ptr, length, (mfxU32)sizeof(APP0_marker) + len_size) || length < sizeof(AVI1))
        return MFX_ERR_MORE_DATA;

    if (memcmp(ptr, AVI1, sizeof(AVI1)))
    {
        bitstream->PicStruct = MFX_PICSTRUCT_UNKNOWN;
        return MFX_ERR_NONE;
    }

    // skip 'AVI1'
    if (!skip(ptr, length, (mfxU32)sizeof(AVI1)) || length < picstruct_size)
        return MFX_ERR_MORE_DATA;

    // get PicStruct
    switch (*ptr)
    {
        case 0:
            bitstream->PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
            break;
        case 1:
            bitstream->PicStruct = MFX_PICSTRUCT_FIELD_TFF;
            break;
        case 2:
            bitstream->PicStruct = MFX_PICSTRUCT_FIELD_BFF;
            break;
        default:
            bitstream->PicStruct = MFX_PICSTRUCT_UNKNOWN;
    }

    return MFX_ERR_NONE;
}

mfxVersion getMinimalRequiredVersion(const APIChangeFeatures &features)
{
    mfxVersion version = {{1, 1}};

    if (features.MVCDecode || features.MVCEncode || features.LowLatency || features.JpegDecode)
    {
        version.Minor = 3;
    }

    if (features.ViewOutput)
    {
        version.Minor = 4;
    }

    if (features.JpegEncode || features.IntraRefresh)
    {
        version.Minor = 6;
    }

    if (features.LookAheadBRC)
    {
        version.Minor = 7;
    }

    if (features.AudioDecode) {
        version.Minor = 8;
    }

    if (features.SupportCodecPluginAPI) {
        version.Minor = 8;
    }

    return version;
}

bool CheckVersion(mfxVersion* version, msdkAPIFeature feature)
{
    if (!version->Major || (version->Major > 1)) {
        return false;
    }

    switch (feature) {
    case MSDK_FEATURE_NONE:
        return true;
    case MSDK_FEATURE_MVC:
        if ((version->Major == 1) && (version->Minor >= 3)) {
            return true;
        }
        break;
    case MSDK_FEATURE_JPEG_DECODE:
        if ((version->Major == 1) && (version->Minor >= 3)) {
            return true;
        }
        break;
   case MSDK_FEATURE_LOW_LATENCY:
        if ((version->Major == 1) && (version->Minor >= 3)) {
            return true;
        }
        break;
    case MSDK_FEATURE_MVC_VIEWOUTPUT:
        if ((version->Major == 1) && (version->Minor >= 4)) {
            return true;
        }
        break;
    case MSDK_FEATURE_JPEG_ENCODE:
        if ((version->Major == 1) && (version->Minor >= 6)) {
            return true;
        }
        break;
    case MSDK_FEATURE_LOOK_AHEAD:
        if ((version->Major == 1) && (version->Minor >= 7)) {
            return true;
        }
        break;
    case MSDK_FEATURE_PLUGIN_API:
        if ((version->Major == 1) && (version->Minor >= 8)) {
            return true;
        }
        break;
    default:
        return false;
    }
    return false;
}

void ConfigureAspectRatioConversion(mfxInfoVPP* pVppInfo)
{
    if (!pVppInfo) return;

    if (pVppInfo->In.AspectRatioW &&
        pVppInfo->In.AspectRatioH &&
        pVppInfo->In.CropW &&
        pVppInfo->In.CropH &&
        pVppInfo->Out.AspectRatioW &&
        pVppInfo->Out.AspectRatioH &&
        pVppInfo->Out.CropW &&
        pVppInfo->Out.CropH)
    {
        mfxF64 dFrameAR         = ((mfxF64)pVppInfo->In.AspectRatioW * pVppInfo->In.CropW) /
                                   (mfxF64)pVppInfo->In.AspectRatioH /
                                   (mfxF64)pVppInfo->In.CropH;

        mfxF64 dPixelAR         = pVppInfo->Out.AspectRatioW / (mfxF64)pVppInfo->Out.AspectRatioH;

        mfxU16 dProportionalH   = (mfxU16)(pVppInfo->Out.CropW * dPixelAR / dFrameAR + 1) & -2; //round to closest odd (values are always positive)

        if (dProportionalH < pVppInfo->Out.CropH)
        {
            pVppInfo->Out.CropY = (mfxU16)((pVppInfo->Out.CropH - dProportionalH) / 2. + 1) & -2;
            pVppInfo->Out.CropH = pVppInfo->Out.CropH - 2 * pVppInfo->Out.CropY;
        }
        else if (dProportionalH > pVppInfo->Out.CropH)
        {
            mfxU16 dProportionalW = (mfxU16)(pVppInfo->Out.CropH * dFrameAR / dPixelAR + 1) & -2;

            pVppInfo->Out.CropX = (mfxU16)((pVppInfo->Out.CropW - dProportionalW) / 2 + 1) & -2;
            pVppInfo->Out.CropW = pVppInfo->Out.CropW - 2 * pVppInfo->Out.CropX;
        }
    }
}

namespace {
    int g_trace_level = MSDK_TRACE_LEVEL_INFO;
}

int msdk_trace_get_level() {
    return g_trace_level;
}

void msdk_trace_set_level(int newLevel) {
    g_trace_level = newLevel;
}

bool msdk_trace_is_printable(int level) {
    return g_trace_level >= level;
}

msdk_ostream & operator <<(msdk_ostream & os, MsdkTraceLevel tl) {
    switch (tl)
    {
        case MSDK_TRACE_LEVEL_CRITICAL :
            os<<MSDK_STRING("CRITICAL");
            break;
        case MSDK_TRACE_LEVEL_ERROR :
            os<<MSDK_STRING("ERROR");
            break;
        case MSDK_TRACE_LEVEL_WARNING :
            os<<MSDK_STRING("WARNING");
            break;
        case MSDK_TRACE_LEVEL_INFO :
            os<<MSDK_STRING("INFO");
            break;
        case MSDK_TRACE_LEVEL_DEBUG :
            os<<MSDK_STRING("DEBUG");
            break;
        default:
            break;
    }
    return os;
}

msdk_string NoFullPath(const msdk_string & file_path) {
    size_t pos = file_path.find_last_of(MSDK_STRING("\\/"));
    if (pos != msdk_string::npos) {
        return file_path.substr(pos + 1);
    }
    return file_path;
}