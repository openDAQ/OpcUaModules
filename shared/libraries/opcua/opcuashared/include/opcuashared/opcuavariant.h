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

#include <opcuashared/opcuanodeid.h>

BEGIN_NAMESPACE_OPENDAQ_OPCUA

class OpcUaVariant;
using OpcUaVariantPtr = std::shared_ptr<OpcUaVariant>;

namespace VariantUtils
{
    bool IsScalar(const UA_Variant& value);
    bool IsVector(const UA_Variant& value);
    bool IsInteger(const UA_Variant& value);
    bool isReal(const UA_Variant& value);
    bool isNull(const UA_Variant& value);

    std::string ToString(const UA_Variant& value);
    int64_t ToNumber(const UA_Variant& value);
    OpcUaNodeId ToNodeId(const UA_Variant& value);
    void ToInt32Variant(OpcUaVariant& variant);
    void ToInt64Variant(OpcUaVariant& variant);

    template <typename T>
    inline bool IsType(const UA_Variant& value)
    {
        const auto expectedType = GetUaDataType<T>();
        if (value.type == expectedType)
            return true;

#ifdef __APPLE__
        if (value.type != nullptr && value.type->typeKind == expectedType->typeKind)
        {
            if (value.type->typeName != nullptr && expectedType->typeName != nullptr)
                return std::strcmp(value.type->typeName, expectedType->typeName) == 0;
            return true;
        }
#endif
        return false;
    }

    template <typename T>
    inline bool HasScalarType(const UA_Variant& value)
    {
        return IsScalar(value) && IsType<T>(value);
    }

    template <typename T>
    inline T ReadScalar(const UA_Variant& value)
    {
        if (!IsScalar(value))
            throw std::runtime_error("Variant is not a scalar");
        if (!IsType<T>(value) && value.type->typeKind != UA_TYPES_ENUMERATION)
            throw std::runtime_error("Variant does not contain a scalar of specified return type");
        return *static_cast<T*>(value.data);
    }
}

class OpcUaVariant : public OpcUaObject<UA_Variant>
{
public:
    using OpcUaObject<UA_Variant>::OpcUaObject;

    OpcUaVariant();

    explicit OpcUaVariant(const uint16_t& value);
    explicit OpcUaVariant(const uint32_t& value);
    explicit OpcUaVariant(const int32_t& value);
    explicit OpcUaVariant(const int64_t& value);
    explicit OpcUaVariant(const char* value);
    explicit OpcUaVariant(const double& value);
    explicit OpcUaVariant(const bool& value);
    explicit OpcUaVariant(const OpcUaNodeId& value);
    OpcUaVariant(const UA_DataType* type, size_t dimension);
    OpcUaVariant(const double& genericValue, const UA_DataType& originalType);

    template <typename T>
    inline bool isType() const
    {
        return VariantUtils::IsType<T>(this->value);
    }

    template <typename T>
    inline bool hasScalarType()
    {
        return VariantUtils::HasScalarType<T>(this->value);
    }

    template <typename T>
    T readScalar() const
    {
        return VariantUtils::ReadScalar<T>(this->value);
    }

    template <typename T, typename UATYPE = TypeToUaDataType<T>>
    void setScalar(const T& value)
    {
        static_assert(UATYPE::DataType != nullptr, "Implement specialization of TypeToUaDataType");

        this->clear();

        const auto status = UA_Variant_setScalarCopy(&this->value, &value, UATYPE::DataType);
        CheckStatusCodeException(status);
    }

    void setValue(UA_Variant&& value);
    void setValue(const UA_Variant& value, bool shallowCopy = false);

    bool isInteger() const;
    bool isString() const;
    bool isDouble() const;
    bool isBool() const;
    bool isNodeId() const;
    bool isNull() const;
    bool isReal() const;
    bool isNumber() const;
    bool isScalar() const;
    bool isVector() const;

    std::string toString() const;
    int64_t toInteger() const;
    double toDouble() const;
    float toFloat() const;
    bool toBool() const;
    OpcUaNodeId toNodeId() const;
};

class OpcUaVariableConversionError : public OpcUaException
{
public:
    OpcUaVariableConversionError(UA_StatusCode statusCode)
        : OpcUaException(statusCode, "Conversion error")
    {
    }
};

END_NAMESPACE_OPENDAQ_OPCUA
