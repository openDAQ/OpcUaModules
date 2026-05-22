#pragma once

#include "opcuageneric_client/opcuageneric.h"

BEGIN_NAMESPACE_OPENDAQ_OPCUA_GENERIC

enum class DomainSource : int
    {
        None = 0,
        ServerTimestamp,
        SourceTimestamp,
        LocalSystemTimestamp,
        _count
    };

enum class NodeIDType : int
{
    Numeric = 0,
    String = 1,
    // Guid,
    // Opaque,
    _count
};

END_NAMESPACE_OPENDAQ_OPCUA_GENERIC
