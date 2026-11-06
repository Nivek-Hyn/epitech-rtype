#pragma once

#include "common/Components.hpp"
#include "engine/ecs/Registry.hpp"

namespace rtype::server {

// Fonction principale appelée dans le système de mise à jour du jeu
void updateEnemyMovements(engine::ecs::Registry &registry, float dt);

} // namespace rtype::server