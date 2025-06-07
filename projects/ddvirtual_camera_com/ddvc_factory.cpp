#include "ddvirtual_camera_com/stdafx.h"

#include "ddvc_factory.h"
#include "ddvc_source_filter.h"
#include "ddbase/windows/ddcom_utils.h"

#include <Strmif.h>
#include <uuids.h>
using namespace dd;

dd::DDComDesc ddvc_factory::GetComDesc()
{
    return dd::DDComDesc{ __uuidof(ddvc_factory), L"ddvisual camera", com_thread_model::Both};
}

HRESULT STDMETHODCALLTYPE ddvc_factory::CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObject)
{
    DDUNUSED(pUnkOuter);
    DDNULL_POINT_RTN(ppvObject);

    if (riid != IID_IBaseFilter && riid != IID_IUnknown) {
        return E_NOINTERFACE;
    }

    ddvc_source_filter* it = new (std::nothrow) ddvc_source_filter(1280, 720);
    if (it == nullptr) {
        return E_NOINTERFACE;
    }

    it->AddRef();
    *ppvObject = it;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE ddvc_factory::QueryInterface(REFIID riid, void** ppvObject)
{
    DDNULL_POINT_RTN(ppvObject);

    if ((riid == IID_IUnknown) || (riid == IID_IClassFactory)) {
        AddRef();
        *ppvObject = (void*)this;
        return S_OK;
    } else {
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }
}

HRESULT ddvc_factory::RegisterCore()
{
    DDComPtr<IFilterMapper2> pFM;
    DDHR_FAIL_RTN(::CoCreateInstance(CLSID_FilterMapper2, NULL, CLSCTX_INPROC_SERVER, IID_IFilterMapper2, (void**)&pFM));
    REGPINTYPES PinTypes = {
        &MEDIATYPE_Video,
        &MEDIASUBTYPE_NULL
    };
    REGFILTERPINS VCamPins = {
        L"Pins",
        FALSE, /// 
        TRUE,  /// output
        FALSE, /// can hav none
        FALSE, /// can have many
        &CLSID_NULL, // obs
        L"PIN",
        1,
        &PinTypes
    };
    DDComDesc desc = GetComDesc();
    REGFILTER2 rf2;
    rf2.dwVersion = 1;
    rf2.dwMerit = MERIT_DO_NOT_USE;
    rf2.cPins = 1;
    rf2.rgPins = &VCamPins;

    IMoniker* pMoniker = 0;
    return pFM->RegisterFilter(desc.comClsId, desc.comDesc.c_str(), &pMoniker, &CLSID_VideoInputDeviceCategory, desc.comDesc.c_str(), &rf2);
}

HRESULT ddvc_factory::UnRegisterCore()
{
    DDComPtr<IFilterMapper2> pFM;
    DDHR_FAIL_RTN(::CoCreateInstance(CLSID_FilterMapper2, NULL, CLSCTX_INPROC_SERVER, IID_IFilterMapper2, (void**)&pFM));
    return pFM->UnregisterFilter(&CLSID_VideoInputDeviceCategory, NULL, GetComDesc().comClsId);
}