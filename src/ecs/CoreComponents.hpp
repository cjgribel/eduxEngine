#ifndef CoreComponents_hpp
#define CoreComponents_hpp

#include <cassert>
#include <array>
#include <queue>

//#include "meta_reg.h"

#include "vec.h"
#include "Entity.hpp"
#include "SparseSet.hpp"
#include "BehaviorScript.hpp"

using linalg::v2f;
#define GridSize 64
using GridSparseSet = SparseSet<unsigned char, GridSize>;

// === Transform ==============================================================

struct Transform
{
    // If stable pointers to Transform are needed, e.g. in scene graph nodes
    // https://github.com/skypjack/entt/blob/master/docs/md/entity.md
    //static constexpr auto in_place_delete = true;

    float x{ 0.0f }, y{ 0.0f }, angle{ 0.0f };

    // Not meta-registered
    float x_parent{ 0.0f }, y_parent{ 0.0f }, angle_parent{ 0.0f };
    float x_global{ 0.0f }, y_global{ 0.0f }, angle_global{ 0.0f };

    // void compute_global_transform()
    // {
    //     x_global = x * cos(angle_parent) - y * sin(angle_parent) + x_parent;
    //     y_global = x * sin(angle_parent) + y * cos(angle_parent) + y_parent;
    //     angle_global = angle + angle_parent;
    // }

    std::string to_string() const;
};


// === HeaderComponent ========================================================

struct HeaderComponent
{
    std::string name;
    std::string chunk_tag;
    uint32_t guid;
    Entity entity_parent;

    HeaderComponent() = default;
    HeaderComponent(
        const std::string& name,
        const std::string& chunk_tag,
        uint32_t guid,
        const Entity& entity_parent) : 
        name(name), 
        chunk_tag(chunk_tag), 
        guid(guid),
        entity_parent(entity_parent)
    {}

    std::string to_string() const;
};

struct ChunkModifiedEvent 
{ 
    Entity entity; 
    std::string chunk_tag;
};

// === CircleColliderGridComponent ============================================

struct CircleColliderGridComponent
{
    struct Circle
    {
        v2f pos;
        float radius;
    };

    std::array<Circle, GridSize> circles;
    GridSparseSet active_indices; // not meta

    // v2f pos[GridSize];
    // float radii[GridSize];
    // bool is_active_flags[GridSize] = { false };

    // int sparse[GridSize]; // sparse
    // int nbr_active = 0;

    int element_count = 0;
    int width = 0; // not meta?
    bool is_active = true;
    unsigned char layer_bit, layer_mask;

    std::string to_string() const;
};

// === IslandFinderComponent ==================================================

struct IslandFinderComponent
{
    int core_x, core_y;

    // For flood-fill. Not serialized.
    std::vector<bool> visited;
    std::queue<std::pair<int, int>> visit_queue;

    // Exposed to Lua. Not serialized.
    std::vector<int> islands;

    std::string to_string() const;
};

// === QuadGridComponent ======================================================

struct QuadGridComponent
{
    std::array<v2f, GridSize> positions;
    std::array<float, GridSize> sizes;
    std::array<uint32_t, GridSize> colors;
    std::array<bool, GridSize> is_active_flags = { false };

    // v2f pos[GridSize];
    // float sizes[GridSize];
    // uint32_t colors[GridSize];
    // bool is_active_flags[GridSize] = { false };

    int count = 0, width = 0;
    bool is_active = true;

    std::string to_string() const;
};

// === DataGridComponent ======================================================

struct DataGridComponent
{
    std::array<float, GridSize> slot1 = { 0.0f };
    std::array<float, GridSize> slot2 = { 0.0f };
    int count = 0, width = 0;

    std::string to_string() const;
};

// === NOT USED ===============================================================

struct QuadComponent
{
    // static constexpr auto in_place_delete = true;

    float w;
    uint32_t color;
    bool is_visible;

    [[nodiscard]] std::string to_string() const {
        std::stringstream ss;
        ss << "{ w " << std::to_string(w) << ", color " << color << ", is_visible " << is_visible << " }";
        return ss.str();
    }
};

struct CircleColliderComponent
{
    // static constexpr auto in_place_delete = true;

    float r;
    bool is_active;

    [[nodiscard]] std::string to_string() const {
        std::stringstream ss;
        ss << "{ r =" << std::to_string(r) << ", is_active " << is_active << " }";
        return ss.str();
    }
};

// === ScriptedBehaviorComponent ==============================================

struct ScriptedBehaviorComponent
{
    std::vector<BehaviorScript> scripts;

    [[nodiscard]] std::string to_string() const;
};

// True in Debug mode, but false in Release mode (Clang -O4)
//static_assert(std::is_same_v<sol::function, sol::protected_function>); 

static_assert(std::is_move_constructible_v<ScriptedBehaviorComponent>);

namespace sol
{
    class state;
}
void serialization_test(std::shared_ptr<sol::state>& lua);

// === Meta registration ======================================================

template<>
void register_meta<Transform>(Editor::Context& context);

template<>
void register_meta<HeaderComponent>(Editor::Context& context);

template<>
void register_meta<CircleColliderGridComponent>(Editor::Context& context);

template<>
void register_meta<IslandFinderComponent>(Editor::Context& context);

template<>
void register_meta<QuadGridComponent>(Editor::Context& context);

template<>
void register_meta<DataGridComponent>(Editor::Context& context);

template<>
void register_meta<ScriptedBehaviorComponent>(Editor::Context& context);

#endif // CoreComponents_hpp
