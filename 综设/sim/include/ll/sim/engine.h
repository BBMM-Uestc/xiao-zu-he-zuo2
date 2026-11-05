// owner: B  stability: frozen
#pragma once

#include "ll/component/registry.h"
#include "ll/model/document.h"
#include "ll/sim/error.h"
#include "ll/sim/netlist.h"
#include "ll/sim/waveform.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <vector>

namespace ll::sim {

enum class RunMode : std::uint8_t { Stopped, Running, Paused };

struct PinValueChange {
    PinRef pin;
    LogicValue value;
};

struct SimFrame {
    std::uint64_t tick = 0;
    std::span<const PinValueChange> changes;  // pins that changed this tick
    std::span<const NetlistError> errors;
    bool converged = true;
};

using Listener = std::function<void(const SimFrame&)>;

class Engine {
public:
    explicit Engine(const ComponentRegistry& registry);
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    // Compile the netlist and allocate state. On failure the previous netlist
    // and state are kept unchanged.
    Status Load(const Document&);

    void Reset();                     // state clear, inputs zero, tick = 0
    void Tick();                      // advance half a clock cycle
    void Advance(std::uint64_t ticks);

    void SetInput(PinRef, LogicValue);  // user-driven terminal
    LogicValue Value(PinRef) const;     // value seen at this pin
    LogicValue NetValue(NetId) const;

    std::uint64_t TickCount() const;
    bool ClockLevel() const;

    const std::vector<NetlistError>& Errors() const;
    const Netlist& NetlistRef() const;

    void SetListener(Listener);
    void SetRecording(bool);
    const Waveform& GetWaveform() const;
    Waveform&       GetWaveform();  // allows A to call SetWatched()

    // Additive convenience for A's toolbar. A may also drive Tick() itself.
    void Run();
    void Pause();
    void Stop();
    void Step();
    RunMode Mode() const;

    void ClearErrors();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace ll::sim
