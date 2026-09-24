#pragma once
#include <opcua_generic_client_module/module_dll.h>
#include <opendaq/device_ptr.h>
#include <opendaq/instance_factory.h>
#include "opcuageneric_client/common.h"
#include "opcuageneric_client/constants.h"
#include <opcuageneric_client/generic_client_device_impl.h>
#include <chrono>
#include <thread>

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

    // Waits until the reader holds at least `count` packets. Tests wait for the evidence instead of
    // assuming a sampling rate: a slow runner takes longer to deliver the packets, it does not deliver
    // fewer of them, so a generous timeout keeps the assertion meaningful everywhere.
    template <typename ReaderPtr>
    static bool waitForPackets(const ReaderPtr& reader, daq::SizeT count, std::chrono::milliseconds timeout)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (reader.getAvailableCount() < count)
        {
            if (std::chrono::steady_clock::now() >= deadline)
                return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return true;
    }

    static daq::ModulePtr CreateModule()
    {
        daq::ModulePtr module;
        createModule(&module, daq::NullContext());
        return module;
    }
};
}
