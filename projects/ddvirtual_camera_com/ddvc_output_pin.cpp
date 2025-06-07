#include "ddvirtual_camera_com/stdafx.h"

#include "ddvc_utils.h"
#include "ddvc_output_pin.h"
#include "ddvc_source_filter.h"
#include "ddbase/ddexec_guard.hpp"
#include "ddbase/ddrandom.h"
#include "ddbase/ddio.h"
#include <vfwmsgs.h>
#include <uuids.h>

using namespace dd;

namespace {
void free_media_type(AM_MEDIA_TYPE& mt)
{
    if (mt.cbFormat != 0 && mt.pbFormat != nullptr) {
        ::CoTaskMemFree((PVOID)mt.pbFormat);
        mt.cbFormat = 0;
        mt.pbFormat = nullptr;
    }

    if (mt.pUnk != nullptr) {
        mt.pUnk->Release();
        mt.pUnk = nullptr;
    }
}

void copy_media_type(AM_MEDIA_TYPE* pDest, const AM_MEDIA_TYPE* pSrc)
{
    *pDest = *pSrc;
    if (pSrc->cbFormat != 0 && pDest->pbFormat != nullptr) {
        pDest->pbFormat = (BYTE*)CoTaskMemAlloc(pSrc->cbFormat);
        if (pDest->pbFormat != nullptr) {
            ::memcpy(pDest->pbFormat, pSrc->pbFormat, pSrc->cbFormat);
        } else {
            pDest->cbFormat = 0;
        }
    }

    if (pDest->pUnk != nullptr) {
        pDest->pUnk->AddRef();
    }
}

bool compare_media_type(const AM_MEDIA_TYPE* a, const AM_MEDIA_TYPE* b)
{
    if (a == b) {
        return true;
    }

    if (a == nullptr || b == nullptr) {
        return false;
    }

    if (!IsEqualGUID(a->majortype, b->majortype) ||
        !IsEqualGUID(a->subtype, b->subtype) ||
        !IsEqualGUID(a->formattype, b->formattype) ||
        a->cbFormat != b->cbFormat) {
        return false;
    }

    if (a->cbFormat != 0) {
        BITMAPINFOHEADER& p1 = ((VIDEOINFOHEADER*)a->pbFormat)->bmiHeader;
        BITMAPINFOHEADER& p2 = ((VIDEOINFOHEADER*)b->pbFormat)->bmiHeader;
        if (p1.biWidth != p2.biWidth ||
            p1.biHeight != p2.biHeight ||
            p1.biBitCount != p2.biBitCount) {
            return false;
        }
    }

    return true;
}

void fill_media_type_rgb32(AM_MEDIA_TYPE* pmt, u32 w, u32 h, u32 fps)
{
    VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)CoTaskMemAlloc(sizeof(VIDEOINFOHEADER));
    if (vih == nullptr) {
        return;
    }

    if (fps == 0 || fps > 60) {
        fps = 30;
    }

    ::ZeroMemory(vih, sizeof(VIDEOINFOHEADER));
    vih->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    vih->bmiHeader.biWidth = (LONG)w;
    vih->bmiHeader.biHeight = (LONG)h;
    vih->bmiHeader.biPlanes = 1;
    vih->bmiHeader.biBitCount = 32;
    vih->bmiHeader.biSizeImage = w * h * (vih->bmiHeader.biBitCount / 8);
    vih->bmiHeader.biCompression = BI_RGB;
    vih->dwBitRate = vih->bmiHeader.biSizeImage * fps;
    vih->AvgTimePerFrame = 10000000 / fps;
    
    ::ZeroMemory(pmt, sizeof(AM_MEDIA_TYPE));
    pmt->majortype = MEDIATYPE_Video;
    pmt->subtype = MEDIASUBTYPE_RGB32;
    pmt->bFixedSizeSamples = TRUE;
    pmt->bTemporalCompression = FALSE;
    pmt->lSampleSize = vih->bmiHeader.biSizeImage;
    pmt->formattype = FORMAT_VideoInfo;
    pmt->cbFormat = sizeof(VIDEOINFOHEADER);
    pmt->pbFormat = (BYTE*)vih;
}
}

ddvc_output_pin::ddvc_output_pin(u32 w, u32 h, ddvc_source_filter* filter) :
    m_w(w), m_h(h)
{
    m_filter = filter;
    m_dir = PIN_DIRECTION::PINDIR_OUTPUT;
    m_name = L"ddvirtual camera output pin";

    s32 am_type_count = 1;
    m_support_media_types.resize(am_type_count);
    for (s32 i = 0; i < am_type_count; ++i) {
        fill_media_type_rgb32(&m_support_media_types[i], m_w, m_h, m_fps);
    }
    m_selected_media_type = &m_support_media_types[0];
}

ddvc_output_pin::~ddvc_output_pin()
{
    for (auto& it : m_support_media_types) {
        free_media_type(it);
    }
    m_selected_media_type = nullptr;
    free_target_pin();
}

void ddvc_output_pin::free_target_pin()
{
    if (m_target_pin == nullptr) {
        return;
    }

    if (m_allocator) {
        m_allocator->Decommit();
        m_allocator.Reset();
    }
    m_target_pin.Reset();
}

const AM_MEDIA_TYPE& ddvc_output_pin::get_selected_media_type()
{
    // should never nullptr
    return *m_selected_media_type;
}

void ddvc_output_pin::set_start_time(REFERENCE_TIME time)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_start_time = time;
}

void ddvc_output_pin::OnFrame()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    ddbitmap bp;
    bp.expend(m_w, m_h, ddbgra{ 255, 0, 0, 255 });
    REFERENCE_TIME end_time = m_start_time + 10000000 / m_fps;
    push_frame(bp, m_start_time, end_time);
    m_start_time = end_time;
}

HRESULT ddvc_output_pin::push_frame(const ddbitmap& bitmap, u64 start_time, u64 end_time)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    DDNULL_POINT_RTN(m_target_pin);

    DDComPtr<IMediaSample> pSample;
    DDHR_FAIL_RTN(m_allocator->GetBuffer(&pSample, nullptr, nullptr, 0));

    BYTE* pBuffer = nullptr;
    DDHR_FAIL_RTN(pSample->GetPointer(&pBuffer));

    s32 actual_date_length = pSample->GetSize();
    if (bitmap.height > m_h || bitmap.width > m_w) {
        ddbitmap cpy = bitmap;
        cpy.clip(0, 0, m_w, m_h);
        long date_size = (long)(cpy.colors.size() * sizeof(ddbgra));
        if (date_size < actual_date_length) {
            actual_date_length = date_size;
        }
        ::memcpy(pBuffer, cpy.colors.data(), actual_date_length);
    } else {
        long date_size = (long)(bitmap.colors.size() * sizeof(ddbgra));
        if (date_size < actual_date_length) {
            actual_date_length = date_size;
        }
        ::memcpy(pBuffer, bitmap.colors.data(), actual_date_length);
    }

    pSample->SetActualDataLength(actual_date_length);
    pSample->SetTime((REFERENCE_TIME*)&start_time, (REFERENCE_TIME*)&end_time);
    pSample->SetSyncPoint(TRUE);

    HRESULT result = m_target_pin->Receive(pSample.Get());
    if (result != S_OK) {
        DDComPtr<IPin> pPin;
        if (m_target_pin->QueryInterface(IID_IPin, (void**)&pPin) == S_OK && pPin != NULL) {
            pPin->EndOfStream();
        }
    }
    return result;
}

HRESULT ddvc_output_pin::QueryInterface(REFIID riid, void** ppvObject)
{
    ddcout(ddconsole_color::gray) << L"ddvc_output_pin::QueryInterface\r\n";
    std::wstring guid;
    dd::ddguid::ddguid_str(riid, guid);
    ddcout(ddconsole_color::gray) << guid << L"\r\n";

    DDVC_NULLPTR_CHECK(ppvObject);

    if (riid == IID_IUnknown || riid == IID_IPin) {
        *ppvObject = static_cast<IPin*>(this);
        AddRef();
        return S_OK;
    }

    if (riid == IID_IKsPropertySet) {
        *ppvObject = static_cast<IKsPropertySet*>(this);
        AddRef();
        return S_OK;
    }

    if (riid == IID_IQualityControl) {
        *ppvObject = static_cast<IQualityControl*>(this);
        AddRef();
        return S_OK;
    }

    if (riid == IID_IAMStreamConfig) {
        *ppvObject = static_cast<IAMStreamConfig*>(this);
        AddRef();
        return S_OK;
    }

    return E_NOINTERFACE;
}


// from pin
HRESULT ddvc_output_pin::set_target_pin(IMemInputPin* target_pin)
{
    DDNULL_POINT_RTN(target_pin);

    ALLOCATOR_PROPERTIES prop;
    ::ZeroMemory(&prop, sizeof(prop));
    (void)target_pin->GetAllocatorRequirements(&prop);
    if (prop.cbAlign == 0) {
        prop.cbAlign = 1;
    }

    if (target_pin->GetAllocator(&m_allocator) != S_OK) {
        DDHR_FAIL_RTN(CoCreateInstance(CLSID_MemoryAllocator, 0, CLSCTX_INPROC_SERVER, IID_IMemAllocator, (void**)&m_allocator));
    }

    prop.cBuffers = 1;
    prop.cbAlign = 1;
    prop.cbBuffer = get_selected_media_type().lSampleSize;
    ALLOCATOR_PROPERTIES Actual;
    memset(&Actual, 0, sizeof(Actual));
    DDHR_FAIL_RTN(m_allocator->SetProperties(&prop, &Actual));
    if (Actual.cbBuffer < prop.cbBuffer) {
        return E_FAIL;
    }

    DDHR_FAIL_RTN(target_pin->NotifyAllocator(m_allocator.Get(), FALSE));
    m_allocator->Commit();
    m_target_pin = target_pin;
    return S_OK;
}

HRESULT ddvc_output_pin::ConnectImpl(IPin* pReceivePin, const AM_MEDIA_TYPE* pmt, bool need_receive)
{
    DDVC_NULLPTR_CHECK(pReceivePin);

    std::lock_guard<std::recursive_mutex>lock(m_mutex);
    if (m_target_pin != nullptr) {
        return VFW_E_ALREADY_CONNECTED;
    }

    if (m_filter->get_state() != State_Stopped) {
        return VFW_E_NOT_STOPPED;
    }

    PIN_DIRECTION dir;
    DDHR_FAIL_RTN(pReceivePin->QueryDirection(&dir));
    if (dir != PIN_DIRECTION::PINDIR_INPUT) {
        return VFW_E_INVALID_DIRECTION;
    }

    if (pmt != nullptr) {
        bool find = false;
        for (auto& it : m_support_media_types) {
            if (compare_media_type(pmt, &it)) {
                m_selected_media_type = &it;
                find = true;
                break;
            }
        }

        if (!find) {
            return VFW_E_NO_ACCEPTABLE_TYPES;
        }
    }

    PIN_DIRECTION target_dir;
    DDHR_FAIL_RTN(pReceivePin->QueryDirection(&target_dir));
    if (target_dir == m_dir) {
        return VFW_E_INVALID_DIRECTION;
    }

    DDComPtr<IMemInputPin> target_pin;
    DDHR_FAIL_RTN(pReceivePin->QueryInterface(IID_IMemInputPin, (void**)&target_pin));
    DDHR_FAIL_RTN(set_target_pin(target_pin.Get()));

    if (need_receive) {
        DDHR_FAIL_RTN(pReceivePin->ReceiveConnection((IPin*)this, &get_selected_media_type()));
    }
    return S_OK;
}

HRESULT ddvc_output_pin::Connect(IPin* pin, const AM_MEDIA_TYPE* pmt)
{
    ddcout(ddconsole_color::gray) << L"ddvc_output_pin::Connect\r\n";
    return ConnectImpl(pin, pmt, true);
}

HRESULT ddvc_output_pin::ReceiveConnection(IPin* pReceivePin, const AM_MEDIA_TYPE* pmt)
{
    ddcout(ddconsole_color::gray) << L"ddvc_output_pin::ReceiveConnection\r\n";
    return ConnectImpl(pReceivePin, pmt, false);
}

HRESULT ddvc_output_pin::Disconnect(void)
{
    ddcout(ddconsole_color::gray) << L"ddvc_output_pin::Disconnect\r\n";
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_target_pin == nullptr) {
        return E_POINTER;
    }

    if (m_filter->get_state() != State_Stopped) {
        DDHR_FAIL_RTN(m_filter->Stop());
    }

    free_target_pin();
    return S_OK;
}

HRESULT ddvc_output_pin::ConnectedTo(IPin** pPin)
{
    ddcout(ddconsole_color::gray) << L"ddvc_output_pin::ConnectedTo\r\n";
    DDVC_NULLPTR_CHECK(pPin);
    *pPin = nullptr;

    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_target_pin == nullptr) {
        return VFW_E_NOT_CONNECTED;
    }

    DDHR_FAIL_RTN(m_target_pin->QueryInterface(IID_IPin, (void**)pPin));
    return S_OK;
}

HRESULT ddvc_output_pin::ConnectionMediaType(AM_MEDIA_TYPE* pmt)
{
    ddcout(ddconsole_color::gray) << L"ddvc_output_pin::ConnectionMediaType\r\n";
    DDVC_NULLPTR_CHECK(pmt);
    copy_media_type(pmt, &get_selected_media_type());
    return S_OK;
}

HRESULT ddvc_output_pin::QueryPinInfo(PIN_INFO* pInfo)
{
    ddcout(ddconsole_color::gray) << L"ddvc_output_pin::QueryPinInfo\r\n";
    DDVC_NULLPTR_CHECK(pInfo);
    m_filter->AddRef();
    pInfo->pFilter = m_filter;
    pInfo->dir = m_dir;
    (void)::wcscpy_s(pInfo->achName, ARRAYSIZE(PIN_INFO::achName), m_name.data());
    return S_OK;
}

HRESULT ddvc_output_pin::QueryDirection(PIN_DIRECTION* pPinDir)
{
    ddcout(ddconsole_color::gray) << L"ddvc_output_pin::QueryDirection\r\n";
    DDVC_NULLPTR_CHECK(pPinDir);
    *pPinDir = m_dir;
    return S_OK;
}

HRESULT ddvc_output_pin::QueryId(LPWSTR* Id)
{
    ddcout(ddconsole_color::gray) << L"ddvc_output_pin::QueryId\r\n";
    DDVC_NULLPTR_CHECK(Id);
    LPWSTR mem = (LPWSTR)::CoTaskMemAlloc(sizeof(PIN_INFO::achName));
    if (mem != NULL) {
        (void)::wcscpy_s(mem, ARRAYSIZE(PIN_INFO::achName), m_name.data());
        *Id = mem;
        return S_OK;
    }
    return E_OUTOFMEMORY;
}

HRESULT ddvc_output_pin::QueryAccept(const AM_MEDIA_TYPE* pmt)
{
    ddcout(ddconsole_color::gray) << L"ddvc_output_pin::QueryAccept\r\n";
    for (auto& it : m_support_media_types) {
        if (!compare_media_type(pmt, &it)) {
            return S_OK;
        }
    }
    return S_FALSE;
}

UUIDREG("{E3C9D6EB-EC65-480F-8B3C-E84253E359B5}", ddvc_media_type_enumer);
class ddvc_media_type_enumer : public IEnumMediaTypes
{
public:
    ddvc_media_type_enumer(ddvc_output_pin* pin) : m_pin(pin)
    {
        DDASSERT(m_pin != nullptr);
    }

    ~ddvc_media_type_enumer() { }

public:
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
    {
        if (riid == UUIDOF(ddvc_media_type_enumer) || riid == UUIDOF(IEnumMediaTypes) || riid == IID_IUnknown) {
            AddRef();
            *ppvObject = this;
            return S_OK;
        }

        return E_NOINTERFACE;
    }

public:
    virtual HRESULT STDMETHODCALLTYPE Next(ULONG cMediaTypes, AM_MEDIA_TYPE** ppMediaTypes, ULONG* pcFetched) override
    {
        if (pcFetched != NULL) {
            *pcFetched = 0;
        }

        if (ppMediaTypes == nullptr) {
            return E_POINTER;
        }

        if (cMediaTypes == 0) {
            return S_FALSE;
        }

        if (m_pos >= m_pin->m_support_media_types.size()) {
            return S_FALSE;
        }

        bool any_successful = false;
        while (cMediaTypes > 0 && m_pos < m_pin->m_support_media_types.size()) {
            *ppMediaTypes = (AM_MEDIA_TYPE*)CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE));
            if (*ppMediaTypes == nullptr) {
                break;
            }

            copy_media_type(*ppMediaTypes, &(m_pin->m_support_media_types[m_pos]));
            ++m_pos;
            ++ppMediaTypes;
            --cMediaTypes;
            if (pcFetched != nullptr) {
                ++(*pcFetched);
            }
            any_successful = true;
        }

        return any_successful ? S_OK : S_FALSE;
    }

    HRESULT STDMETHODCALLTYPE Skip(ULONG step) override
    {
        m_pos += step;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Reset(void) override
    {
        m_pos = 0;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Clone(IEnumMediaTypes** ppEnum) override
    {
        ddvc_media_type_enumer* it = new (std::nothrow)ddvc_media_type_enumer(m_pin.Get());
        if (it != nullptr) {
            it->AddRef();
            *it = *this;
            *ppEnum = it;
            return S_OK;
        }
        return E_NOTIMPL;
    }

public:
    DDREF_COUNT_GEN(__stdcall, m_RefCount);

protected:
    DDComPtr<ddvc_output_pin> m_pin;
    u32 m_pos = 0;
};

HRESULT ddvc_output_pin::EnumMediaTypes(IEnumMediaTypes** ppEnum)
{
    ddcout(ddconsole_color::gray) << L"ddvc_output_pin::EnumMediaTypes\r\n";
    DDVC_NULLPTR_CHECK(ppEnum);

    ddvc_media_type_enumer* it = new(std::nothrow)ddvc_media_type_enumer(this);
    if (it == nullptr) {
        return E_OUTOFMEMORY;
    }

    it->AddRef();
    *ppEnum = it;
    return S_OK;
}

HRESULT ddvc_output_pin::QueryInternalConnections(IPin**, ULONG*)
{
    ddcout(ddconsole_color::gray) << L"ddvc_output_pin::QueryInternalConnections\r\n";
    return E_NOTIMPL;
}

HRESULT ddvc_output_pin::EndOfStream(void)
{
    ddcout(ddconsole_color::gray) << L"ddvc_output_pin::EndOfStream\r\n";
    return S_OK;
}

HRESULT ddvc_output_pin::BeginFlush(void)
{
    ddcout(ddconsole_color::gray) << L"ddvc_output_pin::BeginFlush\r\n";
    return E_UNEXPECTED;
}

HRESULT ddvc_output_pin::EndFlush(void)
{
    ddcout(ddconsole_color::gray) << L"ddvc_output_pin::EndFlush\r\n";
    return E_UNEXPECTED;
}

HRESULT ddvc_output_pin::NewSegment(REFERENCE_TIME, REFERENCE_TIME, double)
{
    ddcout(ddconsole_color::gray) << L"ddvc_output_pin::NewSegment\r\n";
    return S_OK;
}

// from IKsPropertySet
HRESULT ddvc_output_pin::Set(REFGUID guidPropSet, DWORD dwPropID,
    LPVOID pInstanceData, DWORD cbInstanceData,
    LPVOID pPropData, DWORD cbPropData)
{
    DDUNUSED(guidPropSet);
    DDUNUSED(dwPropID);
    DDUNUSED(pInstanceData);
    DDUNUSED(cbInstanceData);
    DDUNUSED(pPropData);
    DDUNUSED(cbPropData);
    ddcout(ddconsole_color::gray) << L"ddvc_output_pin::Set\r\n";
    return E_PROP_SET_UNSUPPORTED;
}

HRESULT ddvc_output_pin::Get(REFGUID guidPropSet, DWORD dwPropID,
    LPVOID pInstanceData, DWORD cbInstanceData,
    LPVOID pPropData, DWORD cbPropData, DWORD* pcbReturned)
{
    DDUNUSED(pInstanceData);
    DDUNUSED(cbInstanceData);
    ddcout(ddconsole_color::gray) << L"ddvc_output_pin::Get\r\n";
    if (guidPropSet != AMPROPSETID_Pin) {
        return E_PROP_SET_UNSUPPORTED;
    }

    if (dwPropID != AMPROPERTY_PIN_CATEGORY) {
        return E_PROP_ID_UNSUPPORTED;
    }

    if (pPropData == nullptr && pcbReturned == nullptr) {
        return E_POINTER;
    }

    if (pcbReturned) {
        *pcbReturned = sizeof(GUID);
    }

    if (pPropData) {
        if (cbPropData < sizeof(GUID)) {
            return E_UNEXPECTED;
        }
        *(GUID*)pPropData = PIN_CATEGORY_CAPTURE;
    }
    return S_OK;
}

HRESULT ddvc_output_pin::QuerySupported(REFGUID guidPropSet, DWORD dwPropID,
    DWORD* pTypeSupport)
{
    ddcout(ddconsole_color::gray) << L"ddvc_output_pin::QuerySupported\r\n";
    if (guidPropSet != AMPROPSETID_Pin) {
        return E_PROP_SET_UNSUPPORTED;
    }

    if (dwPropID != AMPROPERTY_PIN_CATEGORY) {
        return E_PROP_ID_UNSUPPORTED;
    }

    if (pTypeSupport) {
        *pTypeSupport = KSPROPERTY_SUPPORT_GET;
    }
    return S_OK;
}

// from IQualityControl
HRESULT ddvc_output_pin::Notify(IBaseFilter* pSelf, Quality q)
{
    DDUNUSED(pSelf);
    DDUNUSED(q);
    ddcout(ddconsole_color::gray) << L"ddvc_output_pin::Notify\r\n";
    return E_NOTIMPL;
}

HRESULT ddvc_output_pin::SetSink(IQualityControl* piqc)
{
    DDUNUSED(piqc);
    ddcout(ddconsole_color::gray) << L"ddvc_output_pin::SetSink\r\n";
    return S_OK;
}

// from IAMStreamConfig
HRESULT ddvc_output_pin::SetFormat(AM_MEDIA_TYPE* pmt)
{
    ddcout(ddconsole_color::gray) << L"ddvc_output_pin::SetFormat\r\n";
    DDNULL_POINT_RTN(pmt);

    bool find = false;
    for (auto& it : m_support_media_types) {
        if (compare_media_type(pmt, &it)) {
            find = true;
            m_selected_media_type = &it;
            break;
        }
    }

    if (!find) {
        return VFW_E_NOT_FOUND;
    }

    // reconnect
    IFilterGraph* graph = m_filter->get_graph();
    if (graph) {
        DDComPtr<IPin> pin;
        ConnectedTo(&pin);
        if (pin != nullptr) {
            graph->Reconnect(pin.Get());
        }
    }

    return S_OK;
}

HRESULT ddvc_output_pin::GetFormat(AM_MEDIA_TYPE** ppmt)
{
    ddcout(ddconsole_color::gray) << L"ddvc_output_pin::GetFormat\r\n";
    DDNULL_POINT_RTN(ppmt);

    *ppmt = (AM_MEDIA_TYPE*)CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE));
    if (*ppmt == nullptr) {
        return E_OUTOFMEMORY;
    }

    copy_media_type(*ppmt, &get_selected_media_type());
    return S_OK;
}

HRESULT ddvc_output_pin::GetNumberOfCapabilities(int* piCount, int* piSize)
{
    ddcout(ddconsole_color::gray) << L"ddvc_output_pin::GetNumberOfCapabilities\r\n";
    DDNULL_POINT_RTN(piCount);
    DDNULL_POINT_RTN(piSize);

    *piCount = (int)m_support_media_types.size();
    *piSize = sizeof(VIDEO_STREAM_CONFIG_CAPS);
    return S_OK;
}

HRESULT ddvc_output_pin::GetStreamCaps(int iIndex, AM_MEDIA_TYPE** ppmt, BYTE* pSCC)
{
    ddcout(ddconsole_color::gray) << L"ddvc_output_pin::GetStreamCaps\r\n";
    DDNULL_POINT_RTN(ppmt);
    DDNULL_POINT_RTN(pSCC);

    if (iIndex < 0 || iIndex >= (int)m_support_media_types.size()) {
        return E_INVALIDARG;
    }

    *ppmt = (AM_MEDIA_TYPE*)CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE));
    if (*ppmt == nullptr) {
        return E_OUTOFMEMORY;
    }

    copy_media_type(*ppmt, &m_support_media_types[iIndex]);

    VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*)(*ppmt)->pbFormat;
    int X = pvi->bmiHeader.biWidth;
    int Y = abs(pvi->bmiHeader.biHeight);

    VIDEO_STREAM_CONFIG_CAPS* pvscc = (VIDEO_STREAM_CONFIG_CAPS*)pSCC;
    pvscc->guid = FORMAT_VideoInfo;
    pvscc->VideoStandard = AnalogVideo_None;
    pvscc->InputSize.cx = X;
    pvscc->InputSize.cy = Y;
    pvscc->MinCroppingSize.cx = X;
    pvscc->MinCroppingSize.cy = Y;
    pvscc->MaxCroppingSize.cx = X;
    pvscc->MaxCroppingSize.cy = Y;
    pvscc->CropGranularityX = X;
    pvscc->CropGranularityY = Y;
    pvscc->CropAlignX = 0;
    pvscc->CropAlignY = 0;

    pvscc->MinOutputSize.cx = X;
    pvscc->MinOutputSize.cy = Y;
    pvscc->MaxOutputSize.cx = X;
    pvscc->MaxOutputSize.cy = Y;
    pvscc->OutputGranularityX = 0;
    pvscc->OutputGranularityY = 0;
    pvscc->StretchTapsX = 0;
    pvscc->StretchTapsY = 0;
    pvscc->ShrinkTapsX = 0;
    pvscc->ShrinkTapsY = 0;
    pvscc->MinFrameInterval = pvi->AvgTimePerFrame;
    pvscc->MaxFrameInterval = pvi->AvgTimePerFrame;
    pvscc->MinBitsPerSecond = pvi->dwBitRate;
    pvscc->MaxBitsPerSecond = pvi->dwBitRate;
    return S_OK;
}
