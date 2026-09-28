# OPC UA Generic Client — properties and usage

The module connects openDAQ to any OPC UA server. It does not build a device tree: you add one
`MonitoredItem` function block per OPC UA node you want to read, and each block publishes a value
signal and a domain (timestamp) signal.

---

## 1. Connect

```cpp
auto instance = daq::Instance();
auto device = instance.addDevice("daq.opcua.generic://192.168.1.50:4840");
```

Connection string: `daq.opcua.generic://<host>[:<port>][/<path>]` — port defaults to `4840`, path to
empty. IPv6 hosts go in brackets: `daq.opcua.generic://[::1]:4840`.

To connect with settings, take the default config of the device type and pass it along:

```cpp
PropertyObjectPtr config = instance.createDefaultAddDeviceConfig();
PropertyObjectPtr opcuaSetting = config.getPropertyValue("Device.OPCUAGeneric");
opcuaSetting.setPropertyValue("Username", "operator");
opcuaSetting.setPropertyValue("Password", "secret");
auto device = instance.addDevice("daq.opcua.generic://192.168.1.50:4840", config);
device.setPropertyValue("DefaultTimestampMode", 3);   // LocalSystemTimestamp for the blocks added below
```

### Connection config

| Property | Type | Default |
|---|---|---|
| `Username` | String | `""` |
| `Password` | String | `""` |
| `LocalId` | String | `""` |
| `DeviceNodeIDType` | Selection | `1` — `String` |
| `DeviceNodeIDString` | String | `""` |
| `DeviceNodeIDNumeric` | Int | `0` |
| `DeviceNamespaceIndex` | Int | `0` |

These are read once while the device is being created; changing them afterwards has no effect —
remove the device and add it again. None of them becomes a property of the device object.

### Device properties

| Property | Type | Default |
|---|---|---|
| `DefaultTimestampMode` | Selection | `2` — `SourceTimestamp` |
| `DefaultSamplingInterval` | Int | `100` |

These are **not** part of the connection config — the same names in a config passed to `addDevice`
are ignored. Every device starts with the default values above; set them on the device object after
it has been created, at any time.

---

**`Username` / `Password`** — how the client authenticates when it opens the OPC UA session.

An empty `Username` means an **anonymous** session, and `Password` is then ignored entirely — a
password on its own never reaches the server. A non-empty `Username` switches the endpoint to a
username/password identity token.

---

**`LocalId`** — the device's local ID: the component identifier it gets inside the parent folder, so
it is what shows up in component paths and in `device.getLocalId()`.

Set it when you want the same device to keep the same identifier across application runs — for
example when configuration is stored per component path. Leave it empty to let the module derive one.

The value is used as-is unless it is empty or already taken by a sibling device; in those cases the
module falls back, in order, to `<Manufacturer>_<SerialNumber>` read from the device root node, then
the server's `ApplicationUri` (with `/` replaced by `-`), then a generated
`GenericOPCUAClientPseudoDevice<N>`.

Note this is not the device *name*: the name comes from the server's application description, and is
independent of `LocalId`.

---

**`DefaultTimestampMode`** — the initial `TimestampMode` of every newly added `MonitoredItem`. It
takes the same values as the block's [`TimestampMode`](#monitoreditem-properties).

The device only supplies the starting value; the mode actually used is a property of each block.
`TimestampMode` is not part of the `MonitoredItem` config, so a block always starts with the device's
current `DefaultTimestampMode`. To use another mode for one block, write its `TimestampMode` after
adding it.

Writing the property at runtime affects only blocks added afterwards; blocks that already exist keep
their `TimestampMode`.

```cpp
device.setPropertyValue("DefaultTimestampMode", 1);   // ServerTimestamp for blocks added from now on
```

---

**`DefaultSamplingInterval`** — the initial `SamplingInterval`, in milliseconds, of every newly added
`MonitoredItem`. `100` by default.

It works exactly like `DefaultTimestampMode`: `SamplingInterval` is not part of the `MonitoredItem`
config, a block always starts with the device's current `DefaultSamplingInterval` and can be changed
afterwards, and writing the device property at runtime affects only blocks added afterwards.

The value must be greater than `0` and fit into 32 bits. Anything else is ignored with a warning in
the log, and new blocks get the 100 ms default instead.

```cpp
device.setPropertyValue("DefaultSamplingInterval", 500);   // blocks added from now on poll every 500 ms
```

---

**`DeviceNodeIDType` / `DeviceNodeIDString` / `DeviceNodeIDNumeric` / `DeviceNamespaceIndex`** — the
address of one node on the server that describes the device itself, typically a `DeviceType` or
`ComponentType` object from the OPC UA DI companion specification.

The four properties together form a single NodeID: `DeviceNodeIDType` selects which identifier is
used (`1` = String → `DeviceNodeIDString`, `0` = Numeric → `DeviceNodeIDNumeric`), and
`DeviceNamespaceIndex` is the namespace of that identifier. Only the matching identifier property is
visible in a UI; the other one is hidden.

The whole group is **optional** and serves two purposes:

* filling in `device.getInfo()` — the module browses the node's `HasProperty` children and takes
  `SerialNumber`, `Manufacturer`, `Model`, `DeviceRevision`, `SoftwareRevision`, `HardwareRevision`,
  `DeviceManual`, `DeviceClass`, `RevisionCounter`, `ManufacturerUri`, `ProductCode`,
  `ProductInstanceUri`, `AssetId`, `ComponentName` from it;
* deriving a stable `LocalId` from `Manufacturer` + `SerialNumber` when `LocalId` is empty.

Leaving it unset (String type with an empty string, or numeric `0` in namespace `0`) simply skips
that step and logs a warning. A node that does not exist, or properties you have no rights to read,
are skipped as well — they never make `addDevice` fail. The data is read once, at connect time; it is
not refreshed after a reconnect.

```cpp
cfg.setPropertyValue("DeviceNodeIDType", 1);          // String
cfg.setPropertyValue("DeviceNodeIDString", "PLC1");
cfg.setPropertyValue("DeviceNamespaceIndex", 2);      // ns=2;s=PLC1
```

---

## 2. Add a monitored node

```cpp
auto fbType = device.getAvailableFunctionBlockTypes().get("MonitoredItem");

auto cfg = fbType.createDefaultConfig();
cfg.setPropertyValue("NodeIDType", 1);            // String
cfg.setPropertyValue("NodeIDString", ".temperature");
cfg.setPropertyValue("NamespaceIndex", 1);

auto fb = device.addFunctionBlock("MonitoredItem", cfg);
fb.setPropertyValue("SamplingInterval", 500);     // ms; starts with the device's DefaultSamplingInterval
```

### MonitoredItem properties

| Property | Type | Default | In config | Applied |
|---|---|---|---|---|
| `LocalId` | String | `""` | yes | at creation only |
| `NodeIDType` | Selection | `1` — `String` | yes | at creation **and** at runtime |
| `NodeIDString` | String | `""` | yes | at creation **and** at runtime |
| `NodeIDNumeric` | Int | `0` | yes | at creation **and** at runtime |
| `NamespaceIndex` | Int | `0` | yes | at creation **and** at runtime |
| `TimestampMode` | Selection | device's `DefaultTimestampMode` | **no** | at runtime |
| `SamplingInterval` | Int | device's `DefaultSamplingInterval` | **no** | at runtime |

`LocalId` is consumed while the block is being created and does not become a property of it.
`TimestampMode` and `SamplingInterval` are not in the config at all — the same names in a config
passed to `addFunctionBlock` are ignored; the block adds them itself, with the device's current
defaults as initial values. The other six are properties of the block, and each write re-reads the
configuration, re-validates the node, refreshes the block status and reconfigures the signals if the
data type or the timestamp mode changed:

```cpp
fb.setPropertyValue("SamplingInterval", 500);
fb.setPropertyValue("NodeIDString", ".otherNode");   // repoints the block at another node
```

---

**`LocalId`** — the block's local ID inside the device's function block folder. It also prefixes the
signal names: `<localId>ValueSignal` and `<localId>DomainSignal`.

Give it a meaningful value (`"temperature"`, `"pressure"`) to get readable signal names and stable
component paths. If it is empty, or a block with that ID already exists, the module generates
`MonitoredItemFb<N>` instead — silently, so a collision does not fail `addFunctionBlock`.

---

**`NodeIDType` / `NodeIDString` / `NodeIDNumeric` / `NamespaceIndex`** — the address of the OPC UA
node this block reads. They map directly onto an OPC UA NodeID:

| Config | Resulting NodeID |
|---|---|
| `NodeIDType = 1`, `NodeIDString = ".temperature"`, `NamespaceIndex = 1` | `ns=1;s=.temperature` |
| `NodeIDType = 0`, `NodeIDNumeric = 1234`, `NamespaceIndex = 2` | `ns=2;i=1234` |

`NodeIDType` decides which of the two identifier properties is used; the unused one is hidden in a UI
and ignored.

The node must exist, be a `Variable`, and be readable. If it is not, the block goes to `Error` with a
message saying which of the three failed, and it stops polling until the configuration is written
again or the connection is re-established.

An empty `NodeIDString` while `NodeIDType` is `String` is a configuration error — the most common
reason for a freshly added block to sit in `Error` and never produce data.

---

**`TimestampMode`** — which clock the domain (time) signal of this block carries. The block starts
with the device's `DefaultTimestampMode`.

| Value | Name | What the domain signal carries | When to use it |
|---|---|---|---|
| `0` | `None` | nothing — no domain signal is created at all | you only care about values, or the consumer supplies its own time axis |
| `1` | `ServerTimestamp` | the time the OPC UA server produced the response | the server's clock is the reference, the source timestamp is unreliable |
| `2` | `SourceTimestamp` | the time the value originated at its source | closest to when the data was actually measured |
| `3` | `LocalSystemTimestamp` | the client's system clock at the moment of the read | the server sends no usable timestamps; includes network + polling delay |

With `ServerTimestamp` or `SourceTimestamp`, a server that does not deliver that timestamp puts the
block into `Error` and it publishes nothing — that is a real, and common, failure mode.
`LocalSystemTimestamp` always works, at the cost of accuracy. `None` removes the domain signal, so
readers must not expect a time axis.

Writing the property at runtime takes effect immediately on this block only: its domain signal is
created or removed as needed.

```cpp
fb.setPropertyValue("TimestampMode", 1);   // ServerTimestamp
```

---

**`SamplingInterval`** — how often, in milliseconds, this block issues one OPC UA `Read` for its
node. The block starts with the device's `DefaultSamplingInterval` (`100` by default).

This is client-side polling, not an OPC UA subscription: nothing is configured on the server, and the
server's own sampling and publishing settings do not apply. Every successful read publishes a sample,
even when the value has not changed — there is no deadband or change filter.

The value must be greater than `0` and fit into 32 bits. Anything else (`0`, a negative number, a
huge number) is rejected: the block reports a `Config` error and keeps running at the 100 ms default.

The interval is a target, not a guarantee. All blocks of one device are polled by a single thread, so
their reads are serialized: with many blocks, short intervals, or a slow server, the actual periods
stretch. Deadlines are moved forward when that happens, so a stall is never followed by a burst of
catch-up reads. As a rule of thumb, keep `N_blocks × read_round_trip` well below the shortest
interval you configure.

Add as many blocks as you need; they all share that one thread per device.

---

## 3. Read the values

```cpp
auto valueSignal = fb.getSignals()[0];               // the domain signal is hidden

daq::BaseObjectPtr value;
daq::BaseObjectPtr timestamp = valueSignal.getLastValueWithTimestamp(value);

if (value.assigned())
    std::cout << timestamp << " -> " << value << std::endl;
```

`getLastValueWithTimestamp` hands back the value and the moment it carries in one call, already
resolved against the domain signal: the timestamp is an integer number of **microseconds since
1970-01-01**. Both come back unassigned when nothing has been read yet, and the timestamp alone stays
unassigned when `TimestampMode` is `None`, because then there is no domain signal to resolve it
against.

Streaming works with the usual openDAQ readers:

```cpp
auto reader = daq::StreamReaderBuilder()
                  .setSignal(fb.getSignals()[0])
                  .setValueReadType(daq::SampleType::Float64)
                  .setDomainReadType(daq::SampleType::UInt64)
                  .setSkipEvents(true)
                  .build();

double values[64];
uint64_t domain[64];

daq::SizeT count = std::size(values);
reader.readWithDomain(values, domain, &count, 1000);   // waits up to 1000 ms

for (daq::SizeT i = 0; i < count; ++i)
    std::cout << domain[i] << " -> " << values[i] << std::endl;
```

The reader delivers samples as they are polled, so `count` comes back with however many arrived
within the timeout — with a 100 ms `SamplingInterval` that is about ten per second.

The value signal's sample type follows the node: floats → `Float32`/`Float64`, integers → the
matching `Int*`/`UInt*`, `String`/`LocalizedText`/`QualifiedName` → `String`, `DateTime` → `Int64`
microseconds since `1970-01-01T00:00:00Z` (the signal carries the matching time descriptor — unit
`s`, tick resolution `1 / 1'000'000`, that origin). Arrays, structures and booleans are not
supported.

The domain signal is always `UInt64` microseconds since `1970-01-01T00:00:00Z`.

---

## 4. Check that it works

```cpp
// per node
fb.getStatusContainer().getStatus("ComponentStatus");         // Ok / Error
fb.getStatusContainer().getStatusMessage("ComponentStatus");  // why it is not Ok

// per connection
device.getStatusContainer().getStatus("ConnectionStatus");    // Connected / Reconnecting / Unrecoverable
```

A block reports `Error` when its configuration is incomplete (empty `NodeIDString`, bad
`SamplingInterval`), when the node cannot be used (missing, not a Variable, not readable), when the
server's answer is unusable (bad status, no value, missing timestamp for the selected
`TimestampMode`), or when the value type is not supported. The message says which.

A lost connection is detected within 5 seconds: the device goes to `Reconnecting`, polling pauses,
and after a successful reconnect every block re-validates its node and resumes.

---

## 5. Clean up

```cpp
device.removeFunctionBlock(fb);
instance.removeDevice(device);
```

---
