/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012-2014 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#include "mfx_samples_config.h"

#if defined(_WIN32) || defined(_WIN64)

#include "vm/time_defs.h"

msdk_tick msdk_time_get_tick(void)
{
    LARGE_INTEGER t1;

    QueryPerformanceCounter(&t1);
    return t1.QuadPart;
}

msdk_tick msdk_time_get_frequency(void)
{
    LARGE_INTEGER t1;

    QueryPerformanceFrequency(&t1);
    return t1.QuadPart;
}

#endif // #if defined(_WIN32) || defined(_WIN64)
