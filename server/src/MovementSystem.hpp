#pragma once

#include "engine/ecs/Registry.hpp"
#include "common/Components.hpp"

namespace rtype::server {

// Fonction principale appelée dans le système de mise à jour du jeu
void updateEnemyMovements(engine::ecs::Registry &registry, float dt);

} // namespace rtype::server