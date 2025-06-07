#ifndef ddvirtual_camera_com_ddvc_source_filter_h_
#define ddvirtual_camera_com_ddvc_source_filter_h_

#include "ddbase/windows/ddcom_utils.h"
#include "ddbase/thread/ddtask_thread.h"
#include <strmif.h>

class ddvc_output_pin;

UUIDREG("{7C23805B-ED25-4ADC-B2EB-435DBE68415F}", ddvc_source_filter);
class ddvc_source_filter : public IBaseFilter, public IAMovieSetup
{
    friend class ddvc_pin_enumer;

public:
    ddvc_source_filter(dd::u32 w, dd::u32 h);
    ~ddvc_source_filter();
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override;

    FILTER_STATE get_state();

    IFilterGraph* get_graph();

public:
    // from IBaseFilter
    HRESULT STDMETHODCALLTYPE EnumPins(IEnumPins** ppEnum) override;
    HRESULT STDMETHODCALLTYPE FindPin(LPCWSTR Id, IPin** ppPin) override;
    HRESULT STDMETHODCALLTYPE QueryFilterInfo(FILTER_INFO* pInfo) override;
    HRESULT STDMETHODCALLTYPE JoinFilterGraph(IFilterGraph* pGraph, LPCWSTR pName) override;
    HRESULT STDMETHODCALLTYPE QueryVendorInfo(LPWSTR* pVendorInfo) override;

    // from IBaseFilter : IMediaFilter
    HRESULT STDMETHODCALLTYPE Stop(void) override;
    HRESULT STDMETHODCALLTYPE Pause(void) override;
    HRESULT STDMETHODCALLTYPE Run(REFERENCE_TIME tStart) override;
    HRESULT STDMETHODCALLTYPE GetState(DWORD dwMilliSecsTimeout, FILTER_STATE* ppState) override;
    HRESULT STDMETHODCALLTYPE SetSyncSource(IReferenceClock* pClock) override;
    HRESULT STDMETHODCALLTYPE GetSyncSource(IReferenceClock** pClock) override;

    // from IBaseFilter : IMediaFilter : IID_IPersist
    HRESULT STDMETHODCALLTYPE GetClassID(CLSID* pClassID) override;

    // from IAMovieSetup
    HRESULT STDMETHODCALLTYPE Register(void) override;
    HRESULT STDMETHODCALLTYPE Unregister(void) override;

public:
    DDREF_COUNT_GEN(__stdcall, m_RefCount);

private:
    FILTER_INFO m_filter_info;
    volatile FILTER_STATE m_state = State_Stopped;
    DDComPtr<ddvc_output_pin> m_output_pin;
    std::shared_ptr<dd::ddtask_thread> m_thread;
};

#endif // ddvirtual_camera_com_ddvc_source_filter_h_