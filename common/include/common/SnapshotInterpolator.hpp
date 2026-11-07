#pragma once

#include "common/Constants.hpp"
#include "common/Protocol.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <deque>
#include <optional>
#include <unordered_map>
#include <vector>

namespace rtype::protocol {

/// @brief Shows the world a little in the past, and blends the two snapshots around the time being shown.
///
/// The server sends a snapshot only every few ticks (30 per second by default) and the network delivers them with
/// some jitter. Drawing each snapshot the moment it arrives would make entities move in uneven jumps. Instead the
/// client keeps a few snapshots, shows the world as it was `delaySnapshots` snapshot intervals ago, and moves each
/// entity along the straight line between its position in the snapshot before that time and the one after it.
/// A snapshot lost on the way is hidden as long as its neighbours are there.
///
/// Time is measured in server ticks. The client does not know the server's clock, so it keeps an estimate of the
/// offset between the two: the smallest offset seen (the packet with the least delay), which creeps up slowly so that
/// the estimate follows a drift between the clocks.
///
/// Usage, once per frame: push() the snapshots that arrived, handle every snapshot popDue() returns (these are the
/// moments the world "becomes" a new snapshot: sounds, score, lives...), then draw sample().
/// Not thread-safe: use it from the rendering thread.
class SnapshotInterpolator {
public:
    using Clock = std::chrono::steady_clock;

    /// @param delaySnapshots How many snapshot intervals behind the newest snapshot the world is shown. Two keeps
    ///        one lost snapshot invisible; a smaller value reacts faster but stutters when the network is irregular.
    explicit SnapshotInterpolator(double delaySnapshots = 2.0) : _delaySnapshots(delaySnapshots) {}

    /// Remembers a snapshot (all the packets of its tick already merged) received at `arrival`.
    /// A snapshot of a tick already known replaces it (the missing packets of that tick came later); one older than
    /// what is already shown is ignored.
    void push(const Snapshot &snapshot, Clock::time_point arrival)
    {
        if (!_pending.empty() && snapshot.tick == _pending.back().tick) {
            _pending.back() = snapshot;
            return;
        }
        if ((_current && snapshot.tick <= _current->tick) ||
            (!_pending.empty() && snapshot.tick < _pending.back().tick))
            return;

        if (_lastPushedTick && snapshot.tick > *_lastPushedTick) {
            const double gap = static_cast<double>(snapshot.tick - *_lastPushedTick);
            // The smallest gap is the real interval: a bigger one means a snapshot was lost.
            if (!_hasInterval || gap < _intervalTicks) {
                _intervalTicks = gap;
                _hasInterval = true;
            }
        }
        _lastPushedTick = snapshot.tick;

        const double offset = seconds(arrival) - static_cast<double>(snapshot.tick) / rtype::TickRate;
        if (!_hasOffset || offset < _offset)
            _offset = offset;
        else
            _offset += OffsetDrift * (offset - _offset);
        _hasOffset = true;

        _pending.push_back(snapshot);
        while (_pending.size() > MaxPending)
            _pending.pop_front();
    }

    /// @return The next snapshot whose time has come, in order, or nothing. Call it until it returns nothing.
    /// The first snapshot is released at once so that the game does not start with an empty screen.
    std::optional<Snapshot> popDue(Clock::time_point now)
    {
        if (_pending.empty())
            return std::nullopt;
        if (_current && static_cast<double>(_pending.front().tick) > renderTick(now))
            return std::nullopt;
        _current = std::move(_pending.front());
        _pending.pop_front();
        return _current;
    }

    /// @return The entities to draw at `now`: those of the snapshot being shown, moved towards the next one.
    /// An entity that is only in the next snapshot appears when that snapshot is released; one that is only in the
    /// current snapshot stays until then. Without a next snapshot the current one is held as it is.
    std::vector<EntityState> sample(Clock::time_point now) const
    {
        if (!_current)
            return {};
        if (_pending.empty())
            return _current->entities;

        const Snapshot &from = *_current;
        const Snapshot &to = _pending.front();
        const double span = static_cast<double>(to.tick - from.tick);
        const double progress = std::clamp((renderTick(now) - static_cast<double>(from.tick)) / span, 0.0, 1.0);

        std::unordered_map<std::uint32_t, const EntityState *> next;
        next.reserve(to.entities.size());
        for (const EntityState &entity : to.entities)
            next.emplace(entity.netId, &entity);

        std::vector<EntityState> shown;
        shown.reserve(from.entities.size());
        for (const EntityState &entity : from.entities) {
            EntityState blended = entity;
            if (const auto found = next.find(entity.netId); found != next.end()) {
                blended.x = lerp(entity.x, found->second->x, progress);
                blended.y = lerp(entity.y, found->second->y, progress);
            }
            shown.push_back(blended);
        }
        return shown;
    }

    /// Forgets everything, for a new session whose ticks start again from zero.
    void reset() { *this = SnapshotInterpolator(_delaySnapshots); }

    /// @return The tick being shown at `now`, as a fraction (it lags behind the newest snapshot).
    double renderTick(Clock::time_point now) const { return (seconds(now) - _offset) * rtype::TickRate - delayTicks(); }

    /// @return How many ticks the world is shown behind the newest snapshot.
    double delayTicks() const { return _delaySnapshots * _intervalTicks; }

    /// @return Ticks between two snapshots, as measured (a guess of 2 until two snapshots were seen).
    double intervalTicks() const { return _intervalTicks; }

private:
    static constexpr std::size_t MaxPending = 64;
    static constexpr double OffsetDrift = 0.02; // share of the gap between the estimate and a later packet followed

    static double seconds(Clock::time_point time)
    {
        return std::chrono::duration<double>(time.time_since_epoch()).count();
    }

    static float lerp(float from, float to, double progress)
    {
        return static_cast<float>(static_cast<double>(from) +
                                  (static_cast<double>(to) - static_cast<double>(from)) * progress);
    }

    double _delaySnapshots;
    std::optional<Snapshot> _current;
    std::deque<Snapshot> _pending;
    std::optional<std::uint32_t> _lastPushedTick;
    double _intervalTicks = 2.0;
    bool _hasInterval = false;
    double _offset = 0.0;
    bool _hasOffset = false;
};

} // namespace rtype::protocol
