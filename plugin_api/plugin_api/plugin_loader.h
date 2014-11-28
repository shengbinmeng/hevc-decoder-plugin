/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#pragma once

#include "vm/so_defs.h"
#include "sample_utils.h"
#include "mfx_plugin_module.h"
#include "intrusive_ptr.h"
#include <iostream>

class MsdkSoModule
{
protected:
    msdk_so_handle m_module;
public:
    MsdkSoModule(const msdk_string & pluginName)
        : m_module()
    {
        m_module = msdk_so_load(pluginName.c_str());
        if (NULL == m_module)
        {
            MSDK_TRACE_ERROR(MSDK_STRING("Failed to load shared module: ") << pluginName);
        }
    }
    template <class T>
    T GetAddr(const std::string & fncName)
    {
        T pCreateFunc = reinterpret_cast<T>(msdk_so_get_addr(m_module, fncName.c_str()));
        if (NULL == pCreateFunc) {
            MSDK_TRACE_ERROR(MSDK_STRING("Failed to get function addres: ") << fncName.c_str());
        }
        return pCreateFunc;
    }

    virtual ~MsdkSoModule()
    {
        if (m_module)
        {
            msdk_so_free(m_module);
            m_module = NULL;
        }
    }
};


/* for plugin API*/
inline void intrusive_ptr_addref(MFXDecoderPlugin *)
{
}
inline void intrusive_ptr_release(MFXDecoderPlugin * pResource)
{
    //fully destroy plugin resource
    pResource->Release();
}
/* for plugin API*/
inline void intrusive_ptr_addref(MFXEncoderPlugin *)
{
}
inline void intrusive_ptr_release(MFXEncoderPlugin * pResource)
{
    //fully destroy plugin resource
    pResource->Release();
}

/* for plugin API*/
inline void intrusive_ptr_addref(MFXGenericPlugin *)
{
}
inline void intrusive_ptr_release(MFXGenericPlugin * pResource)
{
    //fully destroy plugin resource
    pResource->Release();
}

template<class T>
struct PluginCreateTrait
{
};

template<>
struct PluginCreateTrait<MFXDecoderPlugin>
{
    static const char* Name(){
        return "mfxCreateDecoderPlugin";
    }
    enum PluginType {
        type=MFX_PLUGINTYPE_VIDEO_DECODE
    };
    typedef PluginModuleTemplate::fncCreateDecoderPlugin TCreator;
};

template<>
struct PluginCreateTrait<MFXEncoderPlugin>
{
    static const char* Name() {
        return "mfxCreateEncoderPlugin";
    }
    enum PluginType {
        type=MFX_PLUGINTYPE_VIDEO_ENCODE
    };
    typedef PluginModuleTemplate::fncCreateEncoderPlugin TCreator;
};

template<>
struct PluginCreateTrait<MFXGenericPlugin>
{
    static const char* Name(){
        return "mfxCreateGenericPlugin";
    }
    enum PluginType {
        type=MFX_PLUGINTYPE_VIDEO_GENERAL
    };
    typedef PluginModuleTemplate::fncCreateGenericPlugin TCreator;
};


/*
* Rationale: class to load+register any mediasdk plugin decoder/encoder/generic by given name
*/
template <class T>
class PluginLoader : public MFXPlugin
{
protected:
    MsdkSoModule m_PluginModule;
    intrusive_ptr<T> m_plugin;
    MFXVideoUSER *m_userModule;
    MFXPluginAdapter<T> m_adapter;
    enum {
        ePluginType = PluginCreateTrait<T>::type    //PluginType=MFX_PLUGINTYPE_VIDEO_DECODE
    };
    typedef typename PluginCreateTrait<T>::TCreator TCreator;

public:
    PluginLoader(MFXVideoUSER *userModule, const msdk_string & pluginName)
        : m_PluginModule(pluginName)
        , m_userModule()
    {
        TCreator pCreateFunc = m_PluginModule.GetAddr<TCreator>(PluginCreateTrait<T>::Name());
        if(NULL == pCreateFunc)
        {
            return;
        }
        m_plugin.reset((*pCreateFunc)());

        if (NULL == m_plugin.get())
        {
            MSDK_TRACE_ERROR(MSDK_STRING("Failed to Create plugin: ")<< pluginName);
            return;
        }
        m_adapter = make_mfx_plugin_adapter(m_plugin.get());
        mfxPlugin plg = m_adapter;
        mfxStatus sts = userModule->Register(ePluginType, &plg);
        if (MFX_ERR_NONE != sts) {
            MSDK_TRACE_ERROR(MSDK_STRING("Failed to register plugin: ") << pluginName
                << MSDK_STRING(", MFXVideoUSER::Register(type=") << ePluginType << MSDK_STRING("), mfxsts=") << sts);
            return;
        }
        MSDK_TRACE_INFO(MSDK_STRING("Plugin(type=")<< ePluginType <<MSDK_STRING("): loaded from: ")<< pluginName );
        m_userModule = userModule;
    }

    ~PluginLoader()
    {
        if (m_userModule) {
            mfxStatus sts = m_userModule->Unregister(ePluginType);
            if (sts != MFX_ERR_NONE) {
                MSDK_TRACE_INFO(MSDK_STRING("MFXVideoUSER::Unregister(type=") << ePluginType << MSDK_STRING("), mfxsts=") << sts);
            }
        }
    }
    bool IsOk() {
        return m_userModule != 0;
    }
    virtual mfxStatus Init( mfxVideoParam *par ) {
        return m_plugin->Init(par);
    }
    virtual mfxStatus PluginInit( mfxCoreInterface *core ) {
        return m_plugin->PluginInit(core);
    }
    virtual mfxStatus PluginClose() {
        return m_plugin->PluginClose();
    }
    virtual mfxStatus GetPluginParam( mfxPluginParam *par ) {
        return m_plugin->GetPluginParam(par);
    }
    virtual mfxStatus Execute( mfxThreadTask task, mfxU32 uid_p, mfxU32 uid_a ) {
        return m_plugin->Execute(task, uid_p, uid_a);
    }
    virtual mfxStatus FreeResources( mfxThreadTask task, mfxStatus sts ) {
        return m_plugin->FreeResources(task, sts);
    }
    virtual mfxStatus QueryIOSurf( mfxVideoParam *par, mfxFrameAllocRequest *in , mfxFrameAllocRequest *out ) {
        return m_plugin->QueryIOSurf(par, in, out);
    }
    virtual void Release() {
        return m_plugin->Release();
    }
    virtual mfxStatus Close() {
        return m_plugin->Close();
    }
    virtual mfxStatus SetAuxParams( void* auxParam, int auxParamSize ) {
        return m_plugin->SetAuxParams(auxParam, auxParamSize);
    }
};

template <class T>
MFXPlugin * LoadPluginByType(MFXVideoUSER *userModule, const msdk_string & pluginFullPath) {
    std::auto_ptr<PluginLoader<T> > plg(new PluginLoader<T> (userModule, pluginFullPath));  // PluginLoader<T> plg=new PluginLoader<T>(userModele, pluginFullpath)
    return plg->IsOk() ? plg.release() : NULL;
}

inline MFXPlugin * LoadPlugin(mfxPluginType type, MFXVideoUSER *userModule, const msdk_string & pluginFullPath) {
    switch(type)
    {
    case MFX_PLUGINTYPE_VIDEO_GENERAL:
        return LoadPluginByType<MFXGenericPlugin>(userModule, pluginFullPath);
    case MFX_PLUGINTYPE_VIDEO_DECODE:
        return LoadPluginByType<MFXDecoderPlugin>(userModule, pluginFullPath);
    case MFX_PLUGINTYPE_VIDEO_ENCODE:
        return LoadPluginByType<MFXEncoderPlugin>(userModule, pluginFullPath);
    default:
        MSDK_TRACE_ERROR(MSDK_STRING("Unsupported plugin type : ")<< type << MSDK_STRING(", for plugin: ")<< pluginFullPath);
        return NULL;
    }
}