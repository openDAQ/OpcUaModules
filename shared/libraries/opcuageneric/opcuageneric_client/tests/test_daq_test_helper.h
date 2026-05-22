#pragma once
#include <opcua_generic_client_module/module_dll.h>
#include <opendaq/device_ptr.h>
#include <opendaq/instance_factory.h>
#include "opcuageneric_client/common.h"
#include "opcuageneric_client/constants.h"
#include <opcuageneric_client/generic_client_device_impl.h>

namespace daq::opcua::generic
{
class DaqTestHelper
{
public:
    daq::InstancePtr daqInstance;
    daq::DevicePtr device;

    static daq::PropertyObjectPtr buildDeviceConfig(DomainSource ds)
    {
        auto deviceConfig = OpcuaGenericClientDeviceImpl::createDefaultConfig();
        deviceConfig.setPropertyValue(PROPERTY_NAME_OPCUA_TS_MODE, static_cast<int>(ds));
        return deviceConfig;
    }

    void StartUp(daq::PropertyObjectPtr config = nullptr, std::string connectionStr = "daq.opcua.generic://127.0.0.1:4842")
    {
        DaqInstanceInit();
        DaqOpcuaGenericClientDeviceInit(connectionStr, config);
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
