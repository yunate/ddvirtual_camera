#include "ddvirtual_camera/stdafx.h"

#include "ddbase/ddlocale.h"
#include "ddbase/ddio.h"
#include "ddbase/ddassert.h"
#include <process.h>

#pragma comment(lib, "ddbase.lib")

using namespace dd;

void main()
{
    // ::_CrtSetBreakAlloc(918);
    NSP_DD::ddlocale::set_utf8_locale_and_io_codepage();
    int result = 0;

#ifdef _DEBUG
    _cexit();
    DDASSERT_FMT(!::_CrtDumpMemoryLeaks(), L"Memory leak!!! Check the output to find the log.");
    ::system("pause");
    ::_exit(result);
#else
    ::system("pause");
    ::exit(result);
#endif
}
