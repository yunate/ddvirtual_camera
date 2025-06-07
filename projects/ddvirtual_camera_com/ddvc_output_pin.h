#ifndef ddvirtual_camera_com_ddvc_output_pin_h_
#define ddvirtual_camera_com_ddvc_output_pin_h_

#include "ddbase/windows/ddcom_utils.h"
#include "ddbase/ddbitmap.h"
#include <strmif.h>
#include <amvideo.h>
#include <mutex>

class ddvc_source_filter;

UUIDREG("{7C23805B-ED25-4ADC-B2EB-435DBE68415F}", ddvc_output_pin);
class ddvc_output_pin : public IPin, public IKsPropertySet, public IQualityControl, public IAMStreamConfig
{
    friend class ddvc_media_type_enumer;

public:
    ddvc_output_pin(dd::u32 w, dd::u32 h, ddvc_source_filter* filter);
    ~ddvc_output_pin();

    void set_start_time(REFERENCE_TIME time);
    void OnFrame();
    HRESULT push_frame(const dd::ddbitmap& bitmap, dd::u64 start_time, dd::u64 end_time);

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override;

public:
    // from pin
    HRESULT STDMETHODCALLTYPE Connect(IPin* pReceivePin, const AM_MEDIA_TYPE* pmt) override;
    HRESULT STDMETHODCALLTYPE ReceiveConnection(IPin* pConnector, const AM_MEDIA_TYPE* pmt) override;
    HRESULT STDMETHODCALLTYPE Disconnect(void) override;
    HRESULT STDMETHODCALLTYPE ConnectedTo(IPin** pPin) override;
    HRESULT STDMETHODCALLTYPE ConnectionMediaType(AM_MEDIA_TYPE* pmt) override;
    HRESULT STDMETHODCALLTYPE QueryPinInfo(PIN_INFO* pInfo) override;
    HRESULT STDMETHODCALLTYPE QueryDirection(PIN_DIRECTION* pPinDir) override;
    HRESULT STDMETHODCALLTYPE QueryId(LPWSTR* Id) override;
    HRESULT STDMETHODCALLTYPE QueryAccept(const AM_MEDIA_TYPE* pmt) override;
    HRESULT STDMETHODCALLTYPE EnumMediaTypes(IEnumMediaTypes** ppEnum) override;
    HRESULT STDMETHODCALLTYPE QueryInternalConnections(IPin** apPin, ULONG* nPin) override;
    HRESULT STDMETHODCALLTYPE EndOfStream(void) override;
    HRESULT STDMETHODCALLTYPE BeginFlush(void) override;
    HRESULT STDMETHODCALLTYPE EndFlush(void) override;
    HRESULT STDMETHODCALLTYPE NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate) override;

public:
    // from IKsPropertySet
    HRESULT STDMETHODCALLTYPE Set(REFGUID guidPropSet, DWORD dwPropID,
        LPVOID pInstanceData, DWORD cbInstanceData,
        LPVOID pPropData, DWORD cbPropData) override;
    HRESULT STDMETHODCALLTYPE Get(REFGUID guidPropSet, DWORD dwPropID,
        LPVOID pInstanceData, DWORD cbInstanceData,
        LPVOID pPropData, DWORD cbPropData, DWORD* pcbReturned) override;
    HRESULT STDMETHODCALLTYPE QuerySupported(REFGUID guidPropSet, DWORD dwPropID,
        DWORD* pTypeSupport) override;

public:
    // from IQualityControl
    HRESULT STDMETHODCALLTYPE Notify(IBaseFilter* pSelf, Quality q) override;
    HRESULT STDMETHODCALLTYPE SetSink(IQualityControl* piqc) override;

public:
    // from IAMStreamConfig
    HRESULT STDMETHODCALLTYPE SetFormat(AM_MEDIA_TYPE* pmt) override;
    HRESULT STDMETHODCALLTYPE GetFormat(AM_MEDIA_TYPE** ppmt) override;
    HRESULT STDMETHODCALLTYPE GetNumberOfCapabilities(int* piCount, int* piSize) override;
    HRESULT STDMETHODCALLTYPE GetStreamCaps(int iIndex, AM_MEDIA_TYPE** ppmt, BYTE* pSCC) override;

public:
    DDREF_COUNT_GEN(__stdcall, m_RefCount);

private:
    HRESULT ConnectImpl(IPin* pReceivePin, const AM_MEDIA_TYPE* pmt, bool need_receive);
    HRESULT set_target_pin(IMemInputPin* pin);
    void free_target_pin();
    const AM_MEDIA_TYPE& get_selected_media_type();

    // not owned.
    ddvc_source_filter* m_filter = nullptr;
    PIN_DIRECTION m_dir = PIN_DIRECTION::PINDIR_OUTPUT;
    std::wstring m_name;

    std::vector<AM_MEDIA_TYPE> m_support_media_types;
    AM_MEDIA_TYPE* m_selected_media_type = nullptr;

    DDComPtr<IMemAllocator> m_allocator;
    std::recursive_mutex m_mutex;
    DDComPtr<IMemInputPin> m_target_pin;
    dd::u32 m_fps = 30;
    dd::u32 m_w = 0;
    dd::u32 m_h = 0;
    REFERENCE_TIME m_start_time = 0;
};

#endif // ddvirtual_camera_com_ddvc_output_pin_h_