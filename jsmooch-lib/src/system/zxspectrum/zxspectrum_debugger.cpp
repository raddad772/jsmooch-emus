//
// Created by . on 1/21/26.
//

#include "zxspectrum_debugger.h"
#include "zxspectrum_bus.h"

namespace ZXSpectrum {
void core::setup_debugger_interface(debugger_interface &intf)
{
    intf.supported_by_core = false;
    printf("\nWARNING: debugger interface not supported on core: zxspectrum");
}

}