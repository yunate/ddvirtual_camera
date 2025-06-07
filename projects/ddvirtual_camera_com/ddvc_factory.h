#ifndef ddvirtual_camera_com_ddvc_factory_h_
#define ddvirtual_camera_com_ddvc_factory_h_

#include "ddbase/windows/ddcom_utils.h"

UUIDREG("{B6505148-D377-42E9-97E0-0513F7465761}", ddvc_factory);
class ddvc_factory : public dd::IDDComFactory
{
public:
    static ddvc_factory& get_instance()
    {
        static ddvc_factory inst;
        return inst;
    }

    dd::DDComDesc GetComDesc() override;
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObject) override;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override;
    HRESULT RegisterCore() override;
    HRESULT UnRegisterCore() override;
};

#endif // ddvirtual_camera_com_ddvc_factory_h_