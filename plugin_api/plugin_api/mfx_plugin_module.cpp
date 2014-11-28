/**********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

***********************************************************************************/

#include <stdlib.h>
#include "mfx_plugin_module.h"

#ifdef __cplusplus
extern "C"
{
#endif

#if defined(_WIN32) || defined(_WIN64)
    #define MSDK_PLUGIN_API(ret_type) __declspec(dllexport) ret_type MFX_CDECL
#else
    #define MSDK_PLUGIN_API(ret_type) ret_type MFX_CDECL
#endif

MSDK_PLUGIN_API(MFXDecoderPlugin*) mfxCreateDecoderPlugin() {
    if (!g_PluginModule.CreateDecoderPlugin) {
        return 0;
    }
    return g_PluginModule.CreateDecoderPlugin();
}

MSDK_PLUGIN_API(MFXEncoderPlugin*) mfxCreateEncoderPlugin() {
    if (!g_PluginModule.CreateEncoderPlugin) {
        return 0;
    }
    return g_PluginModule.CreateEncoderPlugin();
}

MSDK_PLUGIN_API(MFXGenericPlugin*) mfxCreateGenericPlugin() {
    if (!g_PluginModule.CreateGenericPlugin) {
        return 0;
    }
    return g_PluginModule.CreateGenericPlugin();
}

//new API
MSDK_PLUGIN_API(mfxStatus) CreatePlugin(mfxPluginUID guid, mfxPlugin* pluginPtr) {
    if (!g_PluginModule.CreatePlugin) {
        return MFX_ERR_NOT_FOUND;
    }
    return g_PluginModule.CreatePlugin(guid, pluginPtr);
}

#ifdef __cplusplus
}
#endif
