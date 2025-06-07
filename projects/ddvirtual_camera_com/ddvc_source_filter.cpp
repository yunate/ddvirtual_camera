#include "ddvirtual_camera_com/stdafx.h"

#include "ddvc_utils.h"
#include "ddvc_source_filter.h"
#include "ddvc_output_pin.h"

#include "ddbase/ddmini_include.h"
#include "ddbase/ddio.h"

#include <vfwmsgs.h>

using namespace dd;

ddvc_source_filter::ddvc_source_filter( u32 w, u32 h)
{
    m_output_pin = new ddvc_output_pin(w, h, this);
    m_filter_info.pGraph = NULL;
    m_filter_info.achName[0] = 0;
}

ddvc_source_filter::~ddvc_source_filter()
{
    m_output_pin = nullptr;
    m_filter_info.pGraph = NULL;
    m_filter_info.achName[0] = 0;
}

HRESULT ddvc_source_filter::QueryInterface(REFIID riid, void** ppvObject)
{
    ddcout(ddconsole_color::gray) << L"ddvc_source_filter::QueryInterface\r\n";
    std::wstring guid;
    dd::ddguid::ddguid_str(riid, guid);
    ddcout(ddconsole_color::gray) << guid << L"\r\n";

    DDVC_NULLPTR_CHECK(ppvObject);

    if (riid == IID_IUnknown || riid == IID_IBaseFilter) {
        *ppvObject = static_cast<IBaseFilter*>(this);
        AddRef();
        return S_OK;
    }

    if (riid == IID_IMediaFilter) {
        *ppvObject = (IMediaFilter*)this;
        AddRef();
        return S_OK;
    }

    if (riid == IID_IAMovieSetup) {
        *ppvObject = (IAMovieSetup*)this;
        AddRef();
        return S_OK;
    }

    return E_NOINTERFACE;
}

// from IBaseFilter
UUIDREG("{BD324C87-8CF7-4E5D-8BCC-4C69FD0CF4C8}", ddvc_pin_enumer);
class ddvc_pin_enumer : public IEnumPins
{
public:
    ddvc_pin_enumer(ddvc_source_filter* filter) : m_filter(filter)
    {
        DDASSERT(m_filter != nullptr);
    }

    ~ddvc_pin_enumer() = default;

public:
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
    {
        if (riid == UUIDOF(ddvc_pin_enumer) ||
            riid == UUIDOF(IEnumPins) ||
            riid == IID_IUnknown) {
            AddRef();
            *ppvObject = this;
            return S_OK;
        }

        return E_NOINTERFACE;
    }

public:
    virtual HRESULT STDMETHODCALLTYPE Next(ULONG cPins, IPin** ppPins, ULONG* pcFetched) override
    {
        if (pcFetched != NULL) {
            *pcFetched = 0;
        }

        DDNULL_POINT_RTN(ppPins);

        if (cPins == 0) {
            return S_FALSE;
        }

        if (cPins > 1) {
            // we have only one media type
            cPins = 1;
        }

        if (m_pos != 0) {
            return S_FALSE;
        }

        m_pos = 1;
        if (pcFetched != nullptr) {
            *pcFetched = 1;
        }
        *ppPins = m_filter->m_output_pin.Get();
        m_filter->m_output_pin->AddRef();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Skip(ULONG) override
    {
        m_pos = 1;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Reset(void) override
    {
        m_pos = 0;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Clone(IEnumPins** ppEnum) override
    {
        ddvc_pin_enumer* it = new (std::nothrow)ddvc_pin_enumer(m_filter.Get());
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
    DDComPtr<ddvc_source_filter> m_filter;
    u32 m_pos = 0;
};

HRESULT ddvc_source_filter::EnumPins(IEnumPins** ppEnum)
{
    ddcout(ddconsole_color::gray) << L"ddvc_source_filter::EnumPins\r\n";
    DDNULL_POINT_RTN(ppEnum);

    ddvc_pin_enumer* it = new(std::nothrow)ddvc_pin_enumer(this);
    if (it == nullptr) {
        return E_NOTIMPL;
    }

    it->AddRef();
    *ppEnum = it;
    return S_OK;
}

HRESULT ddvc_source_filter::FindPin(LPCWSTR Id, IPin** ppPin)
{
    ddcout(ddconsole_color::gray) << L"ddvc_source_filter::FindPin\r\n";
    DDVC_NULLPTR_CHECK(ppPin);

    *ppPin = NULL;
    LPWSTR queryId = NULL;
    DDHR_FAIL_RTN(m_output_pin->QueryId(&queryId));
    if (std::wstring(queryId) == Id) {
        *ppPin = m_output_pin.Get();
        m_output_pin->AddRef();
    }

    if (queryId != NULL) {
        ::CoTaskMemFree(queryId);
    }
    return *ppPin != NULL ? S_OK : VFW_E_NOT_FOUND;
}

HRESULT ddvc_source_filter::QueryFilterInfo(FILTER_INFO* pInfo)
{
    ddcout(ddconsole_color::gray) << L"ddvc_source_filter::QueryFilterInfo\r\n";
    DDVC_NULLPTR_CHECK(pInfo);

    if (m_filter_info.pGraph != NULL) {
        m_filter_info.pGraph->AddRef();
        pInfo->pGraph = m_filter_info.pGraph;
        ::wcsncpy_s(pInfo->achName, m_filter_info.achName, ARRAYSIZE(pInfo->achName));
        return S_OK;
    }

    return E_NOTIMPL;
}

HRESULT ddvc_source_filter::JoinFilterGraph(IFilterGraph* pGraph, LPCWSTR pName)
{
    ddcout(ddconsole_color::gray) << L"ddvc_source_filter::JoinFilterGraph\r\n";
    DDVC_NULLPTR_CHECK(pGraph);

    if (m_filter_info.pGraph != NULL) {
        m_filter_info.pGraph->Release();
    }

    ::wcsncpy_s(m_filter_info.achName, pName, ARRAYSIZE(m_filter_info.achName));
    m_filter_info.pGraph = pGraph;
    return S_OK;
}

HRESULT ddvc_source_filter::QueryVendorInfo(LPWSTR* pVendorInfo)
{
    ddcout(ddconsole_color::gray) << L"ddvc_source_filter::QueryVendorInfo\r\n";
    DDVC_NULLPTR_CHECK(pVendorInfo);
    *pVendorInfo = nullptr;
    return E_NOTIMPL;
}

// from IBaseFilter : IMediaFilter
HRESULT ddvc_source_filter::Stop(void)
{
    ddcout(ddconsole_color::gray) << L"ddvc_source_filter::Stop\r\n";
    m_state = State_Stopped;
    if (m_thread != nullptr) {
        m_thread->stop();
        m_thread.reset();
    }
    return S_OK;
}

HRESULT ddvc_source_filter::Pause(void)
{
    ddcout(ddconsole_color::gray) << L"ddvc_source_filter::Pause\r\n";
    m_state = State_Paused;
    return S_OK;
}

HRESULT ddvc_source_filter::Run(REFERENCE_TIME tStart)
{
    ddcout(ddconsole_color::gray) << L"ddvc_source_filter::Run\r\n";
    m_output_pin->set_start_time(tStart);
    m_state = State_Running;
    m_thread = std::make_shared<dd::ddtask_thread>();
    m_thread->start();
    m_thread->get_task_queue().push_task([this]() {
        if (m_state != State_Running) {
            return;
        }

        m_output_pin->OnFrame();
    }, 33, MAX_U64);
    return S_OK;
}

FILTER_STATE ddvc_source_filter::get_state()
{
    return m_state;
}

IFilterGraph* ddvc_source_filter::get_graph()
{
    return m_filter_info.pGraph;
}

HRESULT ddvc_source_filter::GetState(DWORD, FILTER_STATE* ppState)
{
    ddcout(ddconsole_color::gray) << L"ddvc_source_filter::GetState\r\n";
    DDVC_NULLPTR_CHECK(ppState);
    *ppState = get_state();
    return S_OK;
}

HRESULT ddvc_source_filter::SetSyncSource(IReferenceClock* pClock)
{
    DDUNUSED(pClock);
    ddcout(ddconsole_color::gray) << L"ddvc_source_filter::SetSyncSource\r\n";
    return S_OK;
}

HRESULT ddvc_source_filter::GetSyncSource(IReferenceClock** pClock)
{
    ddcout(ddconsole_color::gray) << L"ddvc_source_filter::GetSyncSource\r\n";
    DDVC_NULLPTR_CHECK(pClock);
    *pClock = nullptr;
    return S_OK;
}

// from IBaseFilter : IMediaFilter : IID_IPersist
HRESULT ddvc_source_filter::GetClassID(CLSID* pClassID)
{
    ddcout(ddconsole_color::gray) << L"ddvc_source_filter::GetClassID\r\n";
    DDNULL_POINT_RTN(pClassID);
    *pClassID = UUIDOF(ddvc_source_filter);
    return S_OK;
}

// from IAMovieSetup
HRESULT ddvc_source_filter::Register(void)
{
    ddcout(ddconsole_color::gray) << L"ddvc_source_filter::Register\r\n";
    return E_FAIL;
}

HRESULT ddvc_source_filter::Unregister(void)
{
    ddcout(ddconsole_color::gray) << L"ddvc_source_filter::Unregister\r\n";
    return E_FAIL;
}
