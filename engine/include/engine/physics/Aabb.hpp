#pragma once

namespace engine::physics {

/// @brief Axis-aligned bounding box: a rectangle that never rotates.
struct Aabb {
    float left = 0.f;   ///< Smallest x coordinate.
    float top = 0.f;    ///< Smallest y coordinate.
    float width = 0.f;  ///< Size along x, never negative.
    float height = 0.f; ///< Size along y, never negative.
};

/// @brief True when both boxes share some area. Boxes that only touch along an edge do not overlap.
constexpr bool overlaps(const Aabb &first, const Aabb &second)
{
    return first.left < second.left + second.width && second.left < first.left + first.width &&
           first.top < second.top + second.height && second.top < first.top + first.height;
}

} // namespace engine::physics
