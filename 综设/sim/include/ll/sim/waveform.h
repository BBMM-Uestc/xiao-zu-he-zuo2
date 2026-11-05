// owner: B  stability: trial
#pragma once

#include "ll/model/logic_value.h"
#include "ll/sim/netlist.h"

#include <cstddef>
#include <cstdint>
#include <unordered_set>
#include <vector>

namespace ll::sim {

struct WaveSample {
    std::uint64_t tick = 0;
    NetId net;
    LogicValue value;
};

class Waveform {
public:
    static constexpr std::size_t kMaxSamples = 500000;

    std::size_t SampleCount() const { return samples_.size(); }
    const std::vector<WaveSample>& Samples() const { return samples_; }
    const std::vector<NetId>& WatchedNets() const { return watched_; }

    void SetWatched(std::vector<NetId> nets);
    void Clear();

private:
    friend class Engine;

    void Record(std::uint64_t tick, NetId net, const LogicValue& value);
    bool IsWatched(NetId net) const;

    std::vector<WaveSample> samples_;
    std::vector<NetId> watched_;
    std::unordered_set<std::uint32_t> watched_set_;
};

}  // namespace ll::sim
