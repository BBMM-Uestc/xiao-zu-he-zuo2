#include "ll/sim/waveform.h"

#include <algorithm>

namespace ll::sim {

void Waveform::SetWatched(std::vector<NetId> nets) {
    watched_ = std::move(nets);
    watched_set_.clear();
    watched_set_.reserve(watched_.size());
    for (NetId id : watched_) watched_set_.insert(id.value);
}

void Waveform::Clear() {
    samples_.clear();
}

bool Waveform::IsWatched(NetId net) const {
    return watched_set_.empty() || watched_set_.count(net.value) != 0;
}

void Waveform::Record(std::uint64_t tick, NetId net, const LogicValue& value) {
    if (!IsWatched(net)) return;
    if (samples_.size() >= kMaxSamples) {
        const std::size_t drop = kMaxSamples / 4;
        samples_.erase(samples_.begin(),
                       samples_.begin() + static_cast<std::ptrdiff_t>(drop));
    }
    samples_.push_back(WaveSample{tick, net, value});
}

}  // namespace ll::sim
