/*
 * Copyright 2022-2025 openDAQ d.o.o.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
#include <coreobjects/property_object_impl.h>

namespace property_helper
{
    inline daq::PropertyObjectPtr populateDefaultConfig(const daq::PropertyObjectPtr& defaultConfig, const daq::PropertyObjectPtr& config)
    {
        auto newConfig = daq::PropertyObject();
        for (const auto& prop : defaultConfig.getAllProperties())
        {
            newConfig.addProperty(prop.asPtr<daq::IPropertyInternal>(true).clone());
            const auto propName = prop.getName();
            newConfig.setPropertyValue(propName, config.hasProperty(propName) ? config.getPropertyValue(propName) : prop.getValue());
        }
        return newConfig;
    }

    template <typename retT, typename intfT>
    retT readProperty(const daq::PropertyObjectPtr objPtr, const std::string& propertyName, const retT defaultValue)
    {
        retT returnValue{defaultValue};
        if (objPtr.hasProperty(propertyName))
        {
            auto property = objPtr.getPropertyValue(propertyName).asPtrOrNull<intfT>();
            if (property.assigned())
            {
                returnValue = property.getValue(defaultValue);
            }
        }
        return returnValue;
    }
}  // namespace property_helper
