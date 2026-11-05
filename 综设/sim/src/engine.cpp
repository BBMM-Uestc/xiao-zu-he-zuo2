#include "ll/sim/engine.h"

#include "netlist_internal.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ll::sim {
namespace {

constexpr std::uint64_t kMaxSettleIterations = 200;

bool IsInputSourceType(std::string_view type) {
    return type == "input" || type == "button" || type == "switch" ||
           type == "dip" || type == "constant";
}

std::string NetListText(const std::vector<NetId>& nets) {
    std::string text;
    for (std::size_t i = 0; i < nets.size(); ++i) {
        if (i != 0) text += ", ";
        text += "#" + std::to_string(nets[i].value);
    }
    return text;
}

}  // namespace

const char* ErrorKindName(NetlistError::Kind kind) {
    switch (kind) {
        case NetlistError::Kind::MultipleDrivers: return "MultipleDrivers";
        case NetlistError::Kind::FloatingInput:   return "FloatingInput";
        case NetlistError::Kind::Oscillation:     return "Oscillation";
        case NetlistError::Kind::WidthMismatch:   return "WidthMismatch";
        case NetlistError::Kind::FloatingNet:     return "FloatingNet";
    }
    return "Unknown";
}

struct Engine::Impl {
    struct RuntimeComponent {
        FlatComponent flat;
        std::vector<std::size_t> pin_slots;
        bool input_source = false;
        bool clock_source = false;
        std::unique_ptr<ComponentState> state;
    };

    struct NetAcc {
        LogicValue value;
        bool has_driver = false;
        bool effective_driver = false;
        bool conflict = false;
    };

    explicit Impl(const ComponentRegistry& registry_in) : registry(&registry_in) {}

    const ComponentRegistry* registry;

    Netlist netlist;
    std::vector<RuntimeComponent> comps;
    std::vector<PinInfo> pins;
    std::unordered_map<std::uint64_t, std::size_t> pin_index;
    std::vector<NetId> pin_net;
    std::vector<int> pin_component;  // runtime component index, -1 for boundary

    std::vector<std::uint8_t> net_width;
    std::vector<bool> net_has_sink;
    std::vector<bool> net_has_real_pin;
    std::vector<bool> net_width_mismatch;

    std::vector<LogicValue> net_values;
    std::vector<LogicValue> next_values;
    std::vector<LogicValue> prev_net_values;
    std::vector<LogicValue> pin_values;
    std::vector<LogicValue> prev_pin_values;

    std::vector<std::uint8_t> net_driver_flags;  // bit 0: any, bit 1: non-Z
    std::vector<std::uint8_t> net_conflict;
    std::vector<NetAcc> acc;
    std::vector<LogicValue> eval_in;
    std::vector<LogicValue> eval_out;

    std::unordered_map<std::uint64_t, LogicValue> overrides;
    std::vector<PinValueChange> changes;
    std::vector<NetlistError> errors;
    std::set<std::pair<int, std::vector<std::uint32_t>>> seen_errors;

    Waveform waveform;
    Listener listener;
    bool recording = false;
    bool loaded = false;
    bool converged = true;
    std::uint64_t tick = 0;
    bool clock_level = false;
    RunMode mode = RunMode::Stopped;

    LogicValue DefaultInputValue(const AttrMap& attrs, std::uint8_t width) const {
        const auto it = attrs.find("value");
        if (it != attrs.end()) {
            if (const auto* b = std::get_if<bool>(&it->second)) {
                return LogicAdaptWidth(LogicValue::FromUInt(*b ? 1 : 0, 1), width, true);
            }
            if (const auto* i = std::get_if<std::int64_t>(&it->second)) {
                return LogicAdaptWidth(LogicValue::FromUInt(static_cast<std::uint64_t>(*i), 1),
                                       width, true);
            }
        }
        return LogicValue::Zero(width);
    }

    void Apply(BuildResult result) {
        auto old_overrides = std::move(overrides);
        overrides.clear();
        errors.clear();
        seen_errors.clear();
        changes.clear();
        waveform.Clear();

        netlist = std::move(result.netlist);
        pins = std::move(result.pins);
        comps.clear();
        comps.reserve(result.components.size());

        std::sort(result.components.begin(), result.components.end(),
                  [](const FlatComponent& a, const FlatComponent& b) {
                      return a.id < b.id;
                  });

        std::unordered_map<ComponentId, std::size_t> comp_index_by_id;
        for (std::size_t i = 0; i < result.components.size(); ++i) {
            comp_index_by_id[result.components[i].id] = i;
        }

        pin_index.clear();
        pin_index.reserve(pins.size());
        pin_net.assign(pins.size(), NetId{});
        pin_component.assign(pins.size(), -1);
        for (std::size_t i = 0; i < pins.size(); ++i) {
            pin_index[PinKey(pins[i].pin)] = i;
            pin_net[i] = netlist.NetOf(pins[i].pin);
            const auto it = comp_index_by_id.find(pins[i].pin.component);
            if (it != comp_index_by_id.end()) {
                pin_component[i] = static_cast<int>(it->second);
            }
        }

        for (std::size_t ci = 0; ci < result.components.size(); ++ci) {
            FlatComponent& flat = result.components[ci];
            RuntimeComponent rt;
            rt.flat = std::move(flat);
            rt.input_source = rt.flat.def->IsInputSource() ||
                              IsInputSourceType(rt.flat.def->Type());
            rt.clock_source = rt.flat.def->IsClockSource();
            rt.pin_slots.reserve(rt.flat.pins.size());
            for (std::size_t p = 0; p < rt.flat.pins.size(); ++p) {
                const auto it = pin_index.find(
                    PinKey(PinRef{rt.flat.id, static_cast<std::uint16_t>(p)}));
                assert(it != pin_index.end());
                rt.pin_slots.push_back(it == pin_index.end() ? 0 : it->second);
            }
            if (rt.flat.def->IsSequential()) {
                rt.state = rt.flat.def->CreateState();
                if (rt.state) rt.flat.def->Reset(rt.state.get());
            }
            comps.push_back(std::move(rt));
        }

        const std::size_t net_count = netlist.NetCount();
        net_width.assign(net_count + 1, 1);
        net_has_sink.assign(net_count + 1, false);
        net_has_real_pin.assign(net_count + 1, false);
        net_width_mismatch.assign(net_count + 1, false);
        net_values.assign(net_count + 1, LogicValue::HighZ(1));
        next_values.assign(net_count + 1, LogicValue::HighZ(1));
        prev_net_values.assign(net_count + 1, LogicValue::HighZ(1));
        net_driver_flags.assign(net_count + 1, 0);
        net_conflict.assign(net_count + 1, 0);
        acc.assign(net_count + 1, NetAcc{});

        for (std::size_t i = 0; i < pins.size(); ++i) {
            const PinInfo& info = pins[i];
            const NetId net = pin_net[i];
            if (!net.valid()) continue;
            const std::uint8_t w = info.width == 0 ? 1 : info.width;
            net_width[net.value] = std::max(net_width[net.value], w);
            if (!info.boundary) {
                net_has_real_pin[net.value] = true;
                bool is_input_source_pin = false;
                if (pin_component[i] >= 0) {
                    is_input_source_pin = comps[static_cast<std::size_t>(pin_component[i])]
                                              .input_source;
                }
                if (info.dir == PinDir::Input && !is_input_source_pin) {
                    net_has_sink[net.value] = true;
                }
            }
        }

        for (std::size_t i = 0; i < pins.size(); ++i) {
            const NetId net = pin_net[i];
            if (!net.valid()) continue;
            if (pins[i].width > 1 && pins[i].width != net_width[net.value]) {
                net_width_mismatch[net.value] = true;
            }
        }

        for (std::size_t n = 0; n <= net_count; ++n) {
            net_values[n] = LogicValue::HighZ(net_width[n]);
            next_values[n] = net_values[n];
            prev_net_values[n] = net_values[n];
        }

        // Default input values, preserving user values for surviving input pins.
        for (RuntimeComponent& rt : comps) {
            if (!rt.input_source) continue;
            for (std::size_t slot : rt.pin_slots) {
                const PinInfo& info = pins[slot];
                const std::uint64_t key = PinKey(info.pin);
                const auto old = old_overrides.find(key);
                if (old != old_overrides.end()) {
                    overrides[key] = old->second;
                } else {
                    overrides[key] = DefaultInputValue(rt.flat.attrs, info.width);
                }
            }
        }

        tick = 0;
        clock_level = false;
        mode = RunMode::Stopped;
        loaded = true;

        converged = Settle(false);
        UpdatePinValues();
        prev_pin_values = pin_values;
        prev_net_values = net_values;
        RefreshDiagnostics();
    }

    LogicValue PinValueAt(std::size_t index) const {
        if (index >= pins.size()) return LogicValue::HighZ(1);
        const NetId net = pin_net[index];
        if (!net.valid() || net.value >= net_values.size()) {
            return LogicValue::HighZ(pins[index].width);
        }
        return LogicAdaptWidth(net_values[net.value], pins[index].width, true);
    }

    void UpdatePinValues() {
        pin_values.resize(pins.size());
        for (std::size_t i = 0; i < pins.size(); ++i) {
            pin_values[i] = PinValueAt(i);
        }
    }

    void AddError(NetlistError::Kind kind, std::vector<NetId> nets,
                  std::string message) {
        std::vector<std::uint32_t> ids;
        ids.reserve(nets.size());
        for (NetId n : nets) {
            if (n.valid()) ids.push_back(n.value);
        }
        std::sort(ids.begin(), ids.end());
        ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
        if (ids.empty()) return;

        const auto key = std::make_pair(static_cast<int>(kind), ids);
        if (!seen_errors.insert(key).second) return;

        NetlistError error;
        error.kind = kind;
        for (std::uint32_t id : ids) error.nets.push_back(NetId{id});
        error.message = std::move(message);
        errors.push_back(std::move(error));
    }

    void Drive(std::uint32_t net_id, const LogicValue& raw_value,
               std::uint8_t pin_width) {
        if (net_id == 0 || net_id >= acc.size()) return;
        NetAcc& a = acc[net_id];
        const std::uint8_t width = net_width[net_id];
        const LogicValue v = LogicAdaptWidth(raw_value, width,
                                             raw_value.width == 1 && width > 1);
        a.has_driver = true;
        if (!v.IsHighZ()) a.effective_driver = true;

        const std::uint64_t mask = LogicMask(width);
        for (int i = 0; i < static_cast<int>(width); ++i) {
            const std::uint64_t bit = std::uint64_t{1} << i;
            if ((a.value.zmask & bit) != 0) {
                if ((v.value & bit) != 0) a.value.value |= bit; else a.value.value &= ~bit;
                if ((v.xmask & bit) != 0) a.value.xmask |= bit; else a.value.xmask &= ~bit;
                if ((v.zmask & bit) != 0) a.value.zmask |= bit; else a.value.zmask &= ~bit;
                continue;
            }
            if ((v.zmask & bit) != 0) continue;
            if ((a.value.xmask & bit) != 0 || (v.xmask & bit) != 0) {
                a.value.value &= ~bit;
                a.value.xmask |= bit;
                a.value.zmask &= ~bit;
                continue;
            }
            if (((a.value.value ^ v.value) & bit) != 0) {
                a.value.value &= ~bit;
                a.value.xmask |= bit;
                a.conflict = true;
            }
        }
        a.value.value &= mask;
        a.value.xmask &= mask;
        a.value.zmask &= mask;
        (void)pin_width;
    }

    bool Settle(bool rising) {
        if (!loaded) return true;
        const std::size_t net_count = netlist.NetCount();
        std::vector<NetId> last_changed;

        for (std::uint64_t iteration = 0; iteration < kMaxSettleIterations;
             ++iteration) {
            for (std::size_t n = 0; n <= net_count; ++n) {
                acc[n] = NetAcc{};
                acc[n].value = LogicValue::HighZ(net_width[n]);
            }

            for (RuntimeComponent& rt : comps) {
                const std::size_t pin_count = rt.flat.pins.size();
                eval_in.resize(pin_count);
                for (std::size_t i = 0; i < pin_count; ++i) {
                    eval_in[i] = PinValueAt(rt.pin_slots[i]);
                }
                eval_out = eval_in;
                EvalContext ctx{std::span<const LogicValue>(eval_in), rt.flat.attrs,
                                tick, clock_level, rising};

                if (rt.clock_source) {
                    const LogicValue level = clock_level ? LogicValue::One(1)
                                                         : LogicValue::Zero(1);
                    for (std::size_t i = 0; i < pin_count; ++i) {
                        if (rt.flat.pins[i].dir != PinDir::Input) {
                            eval_out[i] = LogicAdaptWidth(level, rt.flat.pins[i].width,
                                                          true);
                        }
                    }
                } else if (!rt.input_source) {
                    EvalResult result{std::span<LogicValue>(eval_out)};
                    rt.flat.def->Eval(ctx, rt.state.get(), result);
                }

                if (!rt.input_source) {
                    for (std::size_t i = 0; i < pin_count; ++i) {
                        if (rt.flat.pins[i].dir == PinDir::Input) continue;
                        const NetId net = pin_net[rt.pin_slots[i]];
                        if (!net.valid()) continue;
                        Drive(net.value, eval_out[i], rt.flat.pins[i].width);
                    }
                } else {
                    for (std::size_t i = 0; i < pin_count; ++i) {
                        const std::uint64_t key = PinKey(pins[rt.pin_slots[i]].pin);
                        const auto it = overrides.find(key);
                        const LogicValue value =
                            it == overrides.end()
                                ? LogicValue::Zero(rt.flat.pins[i].width)
                                : it->second;
                        const NetId net = pin_net[rt.pin_slots[i]];
                        if (net.valid()) Drive(net.value, value, rt.flat.pins[i].width);
                    }
                }
            }

            // A SetInput override is an additional driver on that pin's net.
            // Input-source components already consumed their overrides above.
            for (const auto& [key, value] : overrides) {
                const auto it = pin_index.find(key);
                if (it == pin_index.end()) continue;
                const std::size_t index = it->second;
                if (pin_component[index] >= 0 &&
                    comps[static_cast<std::size_t>(pin_component[index])].input_source) {
                    continue;
                }
                const NetId net = pin_net[index];
                if (net.valid()) Drive(net.value, value, pins[index].width);
            }

            bool changed = false;
            last_changed.clear();
            for (std::size_t n = 1; n <= net_count; ++n) {
                const LogicValue value = acc[n].value;
                net_driver_flags[n] =
                    static_cast<std::uint8_t>((acc[n].has_driver ? 1 : 0) |
                                              (acc[n].effective_driver ? 2 : 0));
                net_conflict[n] = acc[n].conflict ? 1 : 0;
                if (!(value == net_values[n])) {
                    changed = true;
                    last_changed.push_back(NetId{static_cast<std::uint32_t>(n)});
                    next_values[n] = value;
                } else {
                    next_values[n] = net_values[n];
                }
            }

            if (!changed) return true;
            net_values.swap(next_values);

            if (iteration + 1 == kMaxSettleIterations) {
                for (NetId net : last_changed) {
                    net_values[net.value] =
                        LogicValue::Unknown(net_width[net.value]);
                }
                AddError(NetlistError::Kind::Oscillation, last_changed,
                         "组合逻辑在 200 轮内未收敛，相关网络已标记为未知: " +
                             NetListText(last_changed));
                converged = false;
                return false;
            }
        }
        return true;
    }

    void CommitEdges() {
        for (RuntimeComponent& rt : comps) {
            if (!rt.flat.def->IsSequential() || !rt.state) continue;
            const std::size_t pin_count = rt.flat.pins.size();
            eval_in.resize(pin_count);
            for (std::size_t i = 0; i < pin_count; ++i) {
                eval_in[i] = PinValueAt(rt.pin_slots[i]);
            }
            EvalContext ctx{std::span<const LogicValue>(eval_in), rt.flat.attrs, tick,
                            clock_level, true};
            rt.flat.def->CommitEdge(ctx, rt.state.get());
        }
    }

    void RefreshDiagnostics() {
        for (std::size_t n = 1; n < net_driver_flags.size(); ++n) {
            if (!net_has_real_pin[n]) continue;
            const bool has_effective = (net_driver_flags[n] & 2) != 0;
            if (!has_effective) {
                if (net_has_sink[n]) {
                    AddError(NetlistError::Kind::FloatingInput, {NetId{static_cast<std::uint32_t>(n)}},
                             "网络 #" + std::to_string(n) +
                                 " 的输入引脚没有驱动源（悬空输入）");
                } else {
                    AddError(NetlistError::Kind::FloatingNet, {NetId{static_cast<std::uint32_t>(n)}},
                             "网络 #" + std::to_string(n) + " 没有驱动源");
                }
            }
            if (net_conflict[n]) {
                AddError(NetlistError::Kind::MultipleDrivers,
                         {NetId{static_cast<std::uint32_t>(n)}},
                         "网络 #" + std::to_string(n) +
                             " 存在多个驱动源且值冲突");
            }
            if (net_width_mismatch[n]) {
                AddError(NetlistError::Kind::WidthMismatch,
                         {NetId{static_cast<std::uint32_t>(n)}},
                         "网络 #" + std::to_string(n) +
                             " 的连接位宽不一致，已按最大位宽处理");
            }
        }
    }

    void ResetInputs() {
        overrides.clear();
        for (RuntimeComponent& rt : comps) {
            if (!rt.input_source) continue;
            for (std::size_t slot : rt.pin_slots) {
                const PinInfo& info = pins[slot];
                // Contract: Reset() puts every user input back to zero.
                overrides[PinKey(info.pin)] = LogicValue::Zero(info.width);
            }
        }
    }

    void ResetState() {
        for (RuntimeComponent& rt : comps) {
            if (rt.state) rt.flat.def->Reset(rt.state.get());
        }
    }

    void Tick() {
        if (!loaded) {
            assert(false && "Engine::Tick called before Load");
            return;
        }

        ++tick;
        const bool new_clock = (tick % 2) == 1;
        const bool rising = new_clock && !clock_level;
        clock_level = new_clock;

        bool tick_converged = Settle(rising);
        if (rising) {
            CommitEdges();
            tick_converged = Settle(false) && tick_converged;
        }

        UpdatePinValues();
        changes.clear();
        if (prev_pin_values.size() == pin_values.size()) {
            for (std::size_t i = 0; i < pin_values.size(); ++i) {
                if (!(pin_values[i] == prev_pin_values[i])) {
                    changes.push_back(PinValueChange{pins[i].pin, pin_values[i]});
                }
            }
        } else {
            for (std::size_t i = 0; i < pin_values.size(); ++i) {
                changes.push_back(PinValueChange{pins[i].pin, pin_values[i]});
            }
        }

        if (recording) {
            for (std::size_t n = 1; n < net_values.size(); ++n) {
                if (!(net_values[n] == prev_net_values[n])) {
                    waveform.Record(tick, NetId{static_cast<std::uint32_t>(n)},
                                    net_values[n]);
                }
            }
        }

        prev_pin_values = pin_values;
        prev_net_values = net_values;
        RefreshDiagnostics();
        converged = tick_converged;

        if (listener) {
            SimFrame frame;
            frame.tick = tick;
            frame.changes =
                std::span<const PinValueChange>(changes.data(), changes.size());
            frame.errors =
                std::span<const NetlistError>(errors.data(), errors.size());
            frame.converged = converged;
            listener(frame);
        }
    }

    void SetInput(PinRef pin, LogicValue value) {
        const auto it = pin_index.find(PinKey(pin));
        if (it == pin_index.end()) return;
        const std::size_t index = it->second;
        if (pin_component[index] >= 0) {
            const RuntimeComponent& rt =
                comps[static_cast<std::size_t>(pin_component[index])];
            if (rt.clock_source) return;
        }
        overrides[PinKey(pin)] = std::move(value);
    }

    LogicValue Value(PinRef pin) const {
        const auto it = pin_index.find(PinKey(pin));
        if (it == pin_index.end() || it->second >= pin_values.size()) {
            return LogicValue::HighZ(1);
        }
        return pin_values[it->second];
    }

    LogicValue NetValue(NetId net) const {
        if (!net.valid() || net.value >= net_values.size()) return LogicValue::HighZ(1);
        return net_values[net.value];
    }
};

Engine::Engine(const ComponentRegistry& registry)
    : impl_(std::make_unique<Impl>(registry)) {}

Engine::~Engine() = default;

Status Engine::Load(const Document& doc) {
    if (Status s = doc.Validate(); !s) return s;

    BuildResult result = BuildGraph(doc, *impl_->registry);
    if (!result.status) return result.status;

    impl_->Apply(std::move(result));
    return Status::Ok();
}

void Engine::Reset() {
    if (!impl_->loaded) return;
    impl_->tick = 0;
    impl_->clock_level = false;
    impl_->ResetInputs();
    impl_->ResetState();
    for (std::size_t n = 0; n < impl_->net_values.size(); ++n) {
        impl_->net_values[n] = LogicValue::HighZ(impl_->net_width[n]);
    }
    impl_->errors.clear();
    impl_->seen_errors.clear();
    impl_->waveform.Clear();
    impl_->converged = impl_->Settle(false);
    impl_->UpdatePinValues();
    impl_->prev_pin_values = impl_->pin_values;
    impl_->prev_net_values = impl_->net_values;
    impl_->RefreshDiagnostics();
}

void Engine::Tick() { impl_->Tick(); }

void Engine::Advance(std::uint64_t ticks) {
    for (std::uint64_t i = 0; i < ticks; ++i) impl_->Tick();
}

void Engine::SetInput(PinRef pin, LogicValue value) { impl_->SetInput(pin, value); }

LogicValue Engine::Value(PinRef pin) const { return impl_->Value(pin); }

LogicValue Engine::NetValue(NetId net) const { return impl_->NetValue(net); }

std::uint64_t Engine::TickCount() const { return impl_->tick; }

bool Engine::ClockLevel() const { return impl_->clock_level; }

const std::vector<NetlistError>& Engine::Errors() const { return impl_->errors; }

const Netlist& Engine::NetlistRef() const { return impl_->netlist; }

void Engine::SetListener(Listener listener) { impl_->listener = std::move(listener); }

void Engine::SetRecording(bool enabled) { impl_->recording = enabled; }

const Waveform& Engine::GetWaveform() const { return impl_->waveform; }

Waveform& Engine::GetWaveform() { return impl_->waveform; }

void Engine::Run() { impl_->mode = RunMode::Running; }
void Engine::Pause() { impl_->mode = RunMode::Paused; }
void Engine::Stop() { impl_->mode = RunMode::Stopped; }

void Engine::Step() {
    impl_->mode = RunMode::Paused;
    impl_->Tick();
}

RunMode Engine::Mode() const { return impl_->mode; }

void Engine::ClearErrors() {
    impl_->errors.clear();
    impl_->seen_errors.clear();
}

}  // namespace ll::sim
