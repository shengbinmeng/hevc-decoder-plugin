/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#pragma once

#include "mfxplugin++.h"

struct PluginModuleTemplate {
    typedef MFXDecoderPlugin* (*fncCreateDecoderPlugin)();
    typedef MFXEncoderPlugin* (*fncCreateEncoderPlugin)();
    typedef MFXGenericPlugin* (*fncCreateGenericPlugin)();
    typedef mfxStatus (MFX_CDECL *CreatePluginPtr_t)(mfxPluginUID uid, mfxPlugin* plugin);

    fncCreateDecoderPlugin CreateDecoderPlugin;
    fncCreateEncoderPlugin CreateEncoderPlugin;
    fncCreateGenericPlugin CreateGenericPlugin;
    CreatePluginPtr_t CreatePlugin;
};

extern PluginModuleTemplate g_PluginModule;
