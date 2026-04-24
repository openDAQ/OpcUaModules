#pragma once
#include <opcua_generic_client_module/module_dll.h>
#include <opendaq/instance_factory.h>
#include <opendaq/device_ptr.h>

namespace daq::opcua::generic
{
class DaqTestHelper
{
public:
    daq::InstancePtr daqInstance;
    daq::DevicePtr device;

    void StartUp(std::string connectionStr = "daq.opcua.generic://127.0.0.1:4842", daq::PropertyObjectPtr config = nullptr)
    {
        DaqInstanceInit();
        DaqOpcuaGenericClientDeviceInit(connectionStr);
    }

    daq::InstancePtr DaqInstanceInit()
    {
        if (!daqInstance.assigned())
            daqInstance = daq::Instance();
        return daqInstance;
    }

    daq::GenericDevicePtr<daq::IDevice> DaqOpcuaGenericClientDeviceInit(std::string connectionStr, daq::PropertyObjectPtr config = nullptr)
    {
        if (!device.assigned())
            device = daqInstance.addDevice(connectionStr, config);

        return device;
    }

    static daq::ModulePtr CreateModule()
    {
        daq::ModulePtr module;
        createModule(&module, daq::NullContext());
        return module;
    }
};
}
