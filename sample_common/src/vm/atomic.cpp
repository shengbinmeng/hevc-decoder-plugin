/* ****************************************************************************** *\

 INTEL CORPORATION PROPRIETARY INFORMATION
 This software is supplied under the terms of a license agreement or nondisclosure
 agreement with Intel Corporation and may not be copied or disclosed except in
 accordance with the terms of that agreement
 Copyright(c) 2011-2013 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#if defined(_WIN32) || defined(_WIN64)

#include "vm/atomic_defs.h"

#define _interlockedbittestandset      fake_set
#define _interlockedbittestandreset    fake_reset
#define _interlockedbittestandset64    fake_set64
#define _interlockedbittestandreset64  fake_reset64
#include <intrin.h>
#undef _interlockedbittestandset
#undef _interlockedbittestandreset
#undef _interlockedbittestandset64
#undef _interlockedbittestandreset64
#pragma intrinsic (_InterlockedIncrement16)
#pragma intrinsic (_InterlockedDecrement16)

mfxU16 msdk_atomic_inc16(volatile mfxU16 *pVariable)
{
    return _InterlockedIncrement16((volatile short*)pVariable);
}

/* Thread-safe 16-bit variable decrementing */
mfxU16 msdk_atomic_dec16(volatile mfxU16 *pVariable)
{
    return _InterlockedDecrement16((volatile short*)pVariable);
}

#endif // #if defined(_WIN32) || defined(_WIN64)
