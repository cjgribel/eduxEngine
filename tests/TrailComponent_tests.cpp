#include <gtest/gtest.h>

#include "ecs/TrailComponent.hpp"

namespace
{
    using Trail = eeng::ecs::TrailComponent::Trail;

    glm::vec3 point(float x)
    {
        return glm::vec3{ x, 0.0f, 0.0f };
    }
}

TEST(TrailComponent, EmissionThreshold)
{
    Trail trail{};
    trail.active = true;
    trail.min_emit_distance = 1.0f;

    EXPECT_TRUE(eeng::ecs::TrailComponent_AddSampleIfNeeded(trail, point(0.0f)));
    EXPECT_FALSE(eeng::ecs::TrailComponent_AddSampleIfNeeded(trail, point(0.5f)));
    EXPECT_TRUE(eeng::ecs::TrailComponent_AddSampleIfNeeded(trail, point(1.2f)));
    EXPECT_EQ(trail.count, 2);
}

TEST(TrailComponent, RingBufferWrapsAndKeepsLatestSamples)
{
    Trail trail{};
    trail.active = true;
    trail.min_emit_distance = 0.0f;

    const int total_samples = static_cast<int>(eeng::ecs::TrailComponent::max_vertices_per_trail) + 6;
    for (int i = 0; i < total_samples; ++i)
        EXPECT_TRUE(eeng::ecs::TrailComponent_AddSampleIfNeeded(trail, point(static_cast<float>(i))));

    EXPECT_EQ(trail.count, static_cast<int>(eeng::ecs::TrailComponent::max_vertices_per_trail));
    EXPECT_EQ(trail.start_index, 6);

    const int oldest_index = trail.start_index;
    EXPECT_FLOAT_EQ(trail.vertices[oldest_index].p.x, 6.0f);
}

TEST(TrailComponent, TeleportClearKeepsNewestSample)
{
    Trail trail{};
    trail.active = true;
    trail.min_emit_distance = 0.0f;
    trail.clear_on_teleport = true;
    trail.clear_teleport_distance = 2.0f;

    eeng::ecs::TrailComponent_AddSampleIfNeeded(trail, point(0.0f));
    eeng::ecs::TrailComponent_AddSampleIfNeeded(trail, point(1.0f));
    EXPECT_EQ(trail.count, 2);

    eeng::ecs::TrailComponent_AddSampleIfNeeded(trail, point(10.0f));
    EXPECT_EQ(trail.count, 1);
    EXPECT_EQ(trail.start_index, 0);
    EXPECT_FLOAT_EQ(trail.vertices[0].p.x, 10.0f);
}

TEST(TrailComponent, LinearFadeAndLifetimePruning)
{
    Trail trail{};
    trail.active = true;
    trail.min_emit_distance = 0.0f;
    trail.lifetime = 1.0f;
    trail.fade_mode = eeng::ecs::TrailFadeMode::Linear;
    trail.color = 0xffffffffu;

    eeng::ecs::TrailComponent_AddSampleIfNeeded(trail, point(0.0f));
    eeng::ecs::TrailComponent_AddSampleIfNeeded(trail, point(1.0f));
    ASSERT_EQ(trail.count, 2);

    eeng::ecs::TrailComponent_AgeAndPrune(trail, 0.5f);
    ASSERT_EQ(trail.count, 2);
    const int idx = trail.start_index;
    const std::uint8_t alpha = static_cast<std::uint8_t>((trail.vertices[idx].color >> 24) & 0xffu);
    EXPECT_NEAR(static_cast<float>(alpha), 127.0f, 1.0f);

    eeng::ecs::TrailComponent_AgeAndPrune(trail, 0.6f);
    EXPECT_EQ(trail.count, 0);
}

TEST(TrailComponent, FadeNoneKeepsAlpha)
{
    Trail trail{};
    trail.active = true;
    trail.min_emit_distance = 0.0f;
    trail.lifetime = 1.0f;
    trail.fade_mode = eeng::ecs::TrailFadeMode::None;
    trail.color = 0x80ffffffu;

    eeng::ecs::TrailComponent_AddSampleIfNeeded(trail, point(0.0f));
    ASSERT_EQ(trail.count, 1);

    eeng::ecs::TrailComponent_AgeAndPrune(trail, 0.5f);
    const int idx = trail.start_index;
    const std::uint8_t alpha = static_cast<std::uint8_t>((trail.vertices[idx].color >> 24) & 0xffu);
    EXPECT_EQ(alpha, 0x80u);
}
