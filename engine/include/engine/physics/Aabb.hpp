#pragma once

namespace engine::physics {

// Axis-aligned bounding box: a rectangle that never rotates.
// (left, top) is the corner with the smallest coordinates; width and height must not be negative.
struct Aabb {
    float left = 0.f;
    float top = 0.f;
    float width = 0.f;
    float height = 0.f;
};

// True when both boxes share some area. Boxes that only touch along an edge do not overlap.
constexpr bool overlaps(const Aabb &first, const Aabb &second)
{
    return first.left < second.left + second.width && second.left < first.left + first.width &&
           first.top < second.top + second.height && second.top < first.top + first.height;
}

} // namespace engine::physics
