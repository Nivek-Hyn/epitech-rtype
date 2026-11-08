#pragma once

#include "common/Protocol.hpp"

#include <cstdint>
#include <unordered_set>
#include <vector>

namespace rtype::protocol {

// Rebuilds the world seen by a client from the Snapshot packets of a tick (a big tick is split in several packets).
// A snapshot of a newer tick replaces the previous entities, one of the same tick adds to them, and an older one is
// ignored: UDP may reorder or duplicate datagrams.
class SnapshotAssembler {
public:
    // Returns false when the snapshot was ignored because it belongs to an older tick.
    bool add(const Snapshot &snapshot)
    {
        if (_hasTick && snapshot.tick < _tick)
            return false;
        if (!_hasTick || snapshot.tick > _tick) {
            _hasTick = true;
            _tick = snapshot.tick;
            _entities.clear();
            _knownIds.clear();
        }
        _state = snapshot.state;
        _score = snapshot.score;
        _lives = snapshot.lives;
        for (const EntityState &entity : snapshot.entities) {
            if (_knownIds.insert(entity.netId).second)
                _entities.push_back(entity);
        }
        return true;
    }

    // Forgets everything, e.g. when connecting to a server whose ticks start again from zero.
    void reset()
    {
        _hasTick = false;
        _tick = 0;
        _state = GameState::Waiting;
        _score = 0;
        _lives = {};
        _entities.clear();
        _knownIds.clear();
    }

    const std::vector<EntityState> &entities() const { return _entities; }
    GameState state() const { return _state; }
    std::uint32_t score() const { return _score; }
    const decltype(Snapshot::lives) &lives() const { return _lives; }
    std::uint32_t tick() const { return _tick; }

private:
    bool _hasTick = false;
    std::uint32_t _tick = 0;
    GameState _state = GameState::Waiting;
    std::uint32_t _score = 0;
    decltype(Snapshot::lives) _lives{};
    std::vector<EntityState> _entities;
    std::unordered_set<std::uint32_t> _knownIds;
};

} // namespace rtype::protocol
