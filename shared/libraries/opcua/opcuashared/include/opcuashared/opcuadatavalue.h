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

#include "opcuavariant.h"

BEGIN_NAMESPACE_OPENDAQ_OPCUA

class OpcUaDataValue;
using OpcUaDataValuePtr = std::shared_ptr<OpcUaDataValue>;

class OpcUaDataValue : public OpcUaObject<UA_DataValue>
{
public:
    using OpcUaObject<UA_DataValue>::OpcUaObject;

    static uint64_t toUnixTimeUs(UA_DateTime date);
    static UA_DateTime fromUnixTimeUs(uint64_t date);

    const UA_DataValue& getDataValue() const;

    bool hasValue() const;

    UA_StatusCode getStatusCode() const;

    bool hasServerTimestamp() const;
    UA_DateTime getServerTimestampUnixEpoch() const;  // us

    bool hasSourceTimestamp() const;
    UA_DateTime getSourceTimestampUnixEpoch() const;  // us

    bool isStatusOK() const;

    bool isInteger() const;
    bool isString() const;
    bool isDouble() const;
    bool isNull() const;
    bool isReal() const;
    bool isNumber() const;

    std::string toString() const;
    int64_t toInteger() const;

    template <typename T>
    inline T readScalar() const
    {
        return VariantUtils::ReadScalar<T>(this->value.value);
    }

    template <typename T, typename UATYPE = TypeToUaDataType<T>>
    void setScalar(const T& value)
    {
        static_assert(UATYPE::DataType != nullptr, "Implement specialization of TypeToUaDataType");

        this->clear();

        const auto status = UA_Variant_setScalarCopy(&(this->value.value), &value, UATYPE::DataType);
        CheckStatusCodeException(status);
    }
};

END_NAMESPACE_OPENDAQ_OPCUA
