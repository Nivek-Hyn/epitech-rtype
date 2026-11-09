#pragma once

#include "common/Protocol.hpp"

#include <cstdint>
#include <unordered_set>
#include <utility>
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
        if (_hasTick && snapshot.tick < _world.tick)
            return false;
        if (!_hasTick || snapshot.tick > _world.tick) {
            _hasTick = true;
            _world.entities.clear();
            _knownIds.clear();
        }
        std::vector<EntityState> entities = std::move(_world.entities);
        _world = snapshot; // every field of the header comes from the newest packet
        _world.entities = std::move(entities);
        for (const EntityState &entity : snapshot.entities) {
            if (_knownIds.insert(entity.netId).second)
                _world.entities.push_back(entity);
        }
        return true;
    }

    // Forgets everything, e.g. when connecting to a server whose ticks start again from zero.
    void reset()
    {
        _hasTick = false;
        _world = Snapshot{};
        _knownIds.clear();
    }

    // The whole world of the newest tick, with the entities of all its packets.
    const Snapshot &snapshot() const { return _world; }
    const std::vector<EntityState> &entities() const { return _world.entities; }
    GameState state() const { return _world.state; }
    std::uint32_t score() const { return _world.score; }
    const decltype(Snapshot::lives) &lives() const { return _world.lives; }
    std::uint32_t tick() const { return _world.tick; }

private:
    bool _hasTick = false;
    Snapshot _world;
    std::unordered_set<std::uint32_t> _knownIds;
};

} // namespace rtype::protocol
