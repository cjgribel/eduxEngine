
#include <entt/entt.hpp>

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include "nlohmann/json.hpp"

#include "imgui.h" // If GUI code is here

#include "meta_literals.h" // for entt literals
#include "meta_aux.h"
#include "CoreComponents.hpp"
#include "InspectType.hpp"

//#include <iostream>

using TypeModifiedCallbackType = std::function<void(entt::meta_any, const Entity&)>;

// === Transform ==============================================================

std::string Transform::to_string() const
{
    std::stringstream ss;
    ss << "Transform { x = " << std::to_string(x)
        << ", y = " << std::to_string(y)
        << ", angle = " << std::to_string(angle) << " }";
    return ss.str();
}

// + const (e.g. when used as key) ?
bool inspect_Transform(void* ptr, Editor::InspectorState& inspector)
{
    Transform* t = static_cast<Transform*>(ptr);
    bool mod = false;

    inspector.begin_leaf("x");
    mod |= Editor::inspect_type(t->x, inspector);
    inspector.end_leaf();

    inspector.begin_leaf("y");
    mod |= Editor::inspect_type(t->y, inspector);
    inspector.end_leaf();

    inspector.begin_leaf("angle");
    mod |= Editor::inspect_type(t->angle, inspector);
    inspector.end_leaf();

    return mod;
}

template<>
void register_meta<Transform>(Editor::Context& context)
{
    assert(context.lua);

    entt::meta<Transform>()
        //.type("Transform"_hs)                 // <- this hashed string is used implicitly
        .prop(display_name_hs, "Transform")  // <- Can be used without .type()

        .data<&Transform::x>("x"_hs).prop(display_name_hs, "x")
        .data<&Transform::y>("y"_hs).prop(display_name_hs, "y")
        .data<&Transform::angle>("angle"_hs).prop(display_name_hs, "angle")
        .data<&Transform::x_global>("x_global"_hs).prop(display_name_hs, "x_global").prop(readonly_hs, true)
        .data<&Transform::y_global>("y_global"_hs).prop(display_name_hs, "y_global").prop(readonly_hs, true)
        .data<&Transform::angle_global>("angle_global"_hs).prop(display_name_hs, "angle_global").prop(readonly_hs, true)

        // Inspection function (optional)
        // Sign: bool(void* ptr, Editor::InspectorState& inspector)
        // .func<&...>(inspect_hs)

        // Clone (optional)
        // Sign: Type(void* src)
        // .func<&...>(clone_hs)

        //.func<&vec3_to_json>(to_json_hs)
        //.func < [](nlohmann::json& j, const void* ptr) { to_json(j, *static_cast<const vec3*>(ptr)); }, entt::as_void_t > (to_json_hs)
        //.func < [](const nlohmann::json& j, void* ptr) { from_json(j, *static_cast<vec3*>(ptr)); }, entt::as_void_t > (from_json_hs)
        //        .func<&vec3::to_string>(to_string_hs)
        //.func<&vec3_to_string>(to_string_hs)
        ;

    assert("Transform"_hs == entt::resolve<Transform>().id());

    context.lua->new_usertype<Transform>("Transform",

        sol::call_constructor,
        sol::factories([](float x, float y, float angle) {
            return Transform{
                .x = x, .y = y, .angle = angle
            };
            }),

        // type_id is required for component types, copying and inspection
        "type_id", &entt::type_hash<Transform>::value,

        // Default construction
        // Needed to copy userdata
        "construct",
        []() { return Transform{}; },

        "x", &Transform::x,
        "y", &Transform::y,
        "angle", &Transform::angle,
        "x_global", &Transform::x_global,
        "y_global", &Transform::y_global,
        "angle_global", &Transform::angle_global,

        sol::meta_function::to_string, &Transform::to_string
    );
}

// === HeaderComponent ========================================================

std::string HeaderComponent::to_string() const
{
    std::stringstream ss;
    ss << "HeaderComponent { name = " << name << " }";
    return  ss.str();
}

namespace {
    bool HeaderComponent_inspect(void* ptr, Editor::InspectorState& inspector)
    {
        return false;
    }
}

template<>
void register_meta<HeaderComponent>(Editor::Context& context)
{

    // chunk_tag callback
    // struct ChunkModifiedEvent { std::string chunk_tag; Entity entity; };
    const TypeModifiedCallbackType chunk_tag_cb = [context](entt::meta_any any, const Entity& entity)
        {
            const auto& new_tag = any.cast<std::string>();
            // std::cout << new_tag << ", " << entity.to_integral() << std::endl;

            // Dispatch immediately since entity may be in an invalid state
            assert(!context.dispatcher.expired());
            context.dispatcher.lock()->dispatch(ChunkModifiedEvent{ entity, new_tag });
        };

    entt::meta<HeaderComponent>()
        .type("HeaderComponent"_hs).prop(display_name_hs, "Header")

        .data<&HeaderComponent::name>("name"_hs).prop(display_name_hs, "name")
        .data<&HeaderComponent::chunk_tag>("chunk_tag"_hs).prop(display_name_hs, "chunk_tag") //.prop(readonly_hs, true)
        .prop("callback"_hs, chunk_tag_cb)

        .data<&HeaderComponent::guid>("guid"_hs).prop(display_name_hs, "guid").prop(readonly_hs, true)
        .data<&HeaderComponent::entity_parent>("entity_parent"_hs).prop(display_name_hs, "entity_parent").prop(readonly_hs, true)

        // Optional meta functions

        // to_string, member version
            //.func<&DebugClass::to_string>(to_string_hs)
        // to_string, lambda version
        .func < [](const void* ptr) {
        return static_cast<const HeaderComponent*>(ptr)->name;
        } > (to_string_hs)
            // inspect
                // .func<&inspect_Transform>(inspect_hs)
            // clone
                //.func<&cloneDebugClass>(clone_hs)
            ;

        // Register to sol

        assert(context.lua);
        context.lua->new_usertype<HeaderComponent>("HeaderComponent",
            //sol::constructors<HeaderComponent(), HeaderComponent(const std::string&)>(),

            // If the type has defined ctors
            //sol::constructors<HeaderComponent(), HeaderComponent(const std::string&)>(),

#if 0
            sol::call_constructor,
            sol::factories(
                []() -> HeaderComponent {  // Default constructor
                    return HeaderComponent{};
                },
                [](const std::string& name) {
                    return HeaderComponent{
                        .name = name,
                        .chunk_tag = "default_chunk",
                        .guid = 0
                    };
                }),
#endif

            "type_id", &entt::type_hash<HeaderComponent>::value,

            "name", &HeaderComponent::name,
            "chunk_tag", &HeaderComponent::chunk_tag,
            "guid", &HeaderComponent::guid,
            "entity_parent", &HeaderComponent::entity_parent,

#if 0
            // clone
            "copy",
            [](sol::userdata userdata)
            {
                // TODO: check fields
                return HeaderComponent{ userdata.get<std::string>("name") };
            },
#endif

            // Needed for value-copying
            "construct",
            []() { return HeaderComponent{}; },

            sol::meta_function::to_string, &HeaderComponent::to_string
        );

        // TEST
        // struct ABC { int x; };

        // lua.new_usertype<ABC>("ABC",
        //     sol::meta_function::construct,
        //     sol::factories([] { return ABC{}; }),

        //     "x", &ABC::x
        // );
}

// === CircleColliderGridComponent ============================================

std::string CircleColliderGridComponent::to_string() const
{
    return "CircleColliderGridComponent { ... }";
    //     std::stringstream ss;
    //     ss << "{ radii = ";
    //     for (int i = 0; i < count; i++) ss << std::to_string(radii[i]) << ", ";
    //     ss << "{ is_active_flags = ";
    //     for (int i = 0; i < count; i++) ss << std::to_string(is_active_flags[i]) << ", ";
    //     return ss.str();
}

template<>
void register_meta<CircleColliderGridComponent>(Editor::Context& context)
{
    entt::meta<CircleColliderGridComponent::Circle>()
        .type("Circle"_hs)
        .prop(display_name_hs, "Circle")

        .data<&CircleColliderGridComponent::Circle::pos>("pos"_hs)
        .prop(display_name_hs, "position")

        .data<&CircleColliderGridComponent::Circle::radius>("radius"_hs)
        .prop(display_name_hs, "radius")
        ;

    entt::meta<linalg::v2f>()
        .type("v2f"_hs)
        .prop(display_name_hs, "Vector2D")

        .data<&linalg::v2f::x>("x"_hs)
        .prop(display_name_hs, "x")

        .data<&linalg::v2f::y>("y"_hs)
        .prop(display_name_hs, "y")
        ;

    entt::meta<GridSparseSet>()
        .type("GridSparseSet"_hs)
        .prop(display_name_hs, "GridSparseSet")

        .data<&GridSparseSet::set_dense_array, &GridSparseSet::get_dense_array>("dense_array"_hs)
        // .data<nullptr, &GridSparseSet::get_dense_array>("dense_array"_hs) // Prevents deserialization
        .prop(display_name_hs, "dense_array")
        .prop(readonly_hs, true)

        .data<&GridSparseSet::set_sparse_array, &GridSparseSet::get_sparse_array>("sparse_array"_hs)
        // .data<nullptr, &GridSparseSet::get_sparse_array>("sparse_array"_hs) // Prevents deserialization
        .prop(display_name_hs, "sparse_array")
        .prop(readonly_hs, true)

        .data<&GridSparseSet::set_dense_count, &GridSparseSet::get_dense_count>("dense_count"_hs)
        // .data<nullptr, &GridSparseSet::get_dense_count>("dense_count"_hs) // Prevents deserialization
        .prop(display_name_hs, "dense_count")
        .prop(readonly_hs, true)
        ;

    entt::meta<CircleColliderGridComponent>()
        .type("CircleColliderGridComponent"_hs).prop(display_name_hs, "CircleColliderGrid")

        .data<&CircleColliderGridComponent::is_active>("is_active"_hs)
        .prop(display_name_hs, "is_active")

        .data<&CircleColliderGridComponent::element_count>("element_count"_hs)
        .prop(display_name_hs, "element_count")
        .prop(readonly_hs, true)

        .data<&CircleColliderGridComponent::width>("width"_hs)
        .prop(display_name_hs, "width")
        .prop(readonly_hs, true)

        .data<&CircleColliderGridComponent::layer_bit>("layer_bit"_hs)
        .prop(display_name_hs, "layer_bit")

        .data<&CircleColliderGridComponent::layer_mask>("layer_mask"_hs)
        .prop(display_name_hs, "layer_mask")

        .data<&CircleColliderGridComponent::circles>("circles"_hs)
        .prop(display_name_hs, "circles")

        .data<&CircleColliderGridComponent::active_indices>("active_indices"_hs)
        .prop(display_name_hs, "active_indices")
        // .prop(readonly_hs, true)
        // + hidden from GUI?

        // Optional meta functions

        // to_string, member version
            //.func<&DebugClass::to_string>(to_string_hs)
        // to_string, lambda version
        // .func < [](const void* ptr) {
        // return static_cast<const HeaderComponent*>(ptr)->name;
        // } > (to_string_hs)
            // inspect
                // .func<&inspect_Transform>(inspect_hs)
            // clone
                //.func<&cloneDebugClass>(clone_hs)
        ;

    assert(context.lua);
    context.lua->new_usertype<CircleColliderGridComponent>("CircleColliderGridComponent",
        "type_id",
        &entt::type_hash<CircleColliderGridComponent>::value,

        sol::call_constructor,
        sol::factories([](
            int width,
            int height,
            bool is_active,
            unsigned char layer_bit,
            unsigned char layer_mask)
            {
                assert(width * height <= GridSize);
                auto c = CircleColliderGridComponent{
                    .element_count = width * height,
                    .width = width,
                    .is_active = is_active,
                    .layer_bit = layer_bit,
                    .layer_mask = layer_mask };
                //for (int i = 0; i < width * height; i++)
                //    c.active_indices.add(i);
                return c;
            }),

        // "add_circle",
        // [](CircleColliderGridComponent& c, float x, float y, float radius, bool is_active)
        // {
        //     if (c.count >= GridSize) throw std::out_of_range("Index out of range");
        //     c.pos[c.count].x = x;
        //     c.pos[c.count].y = y;
        //     c.radii[c.count] = radius;
        //     c.is_active_flags[c.count] = is_active;
        //     c.count++;
        // },
        "set_circle_at",
        [](CircleColliderGridComponent& c,
            int index,
            float x,
            float y,
            float radius,
            bool is_active)
        {
            assert(index >= 0 && index < c.element_count);
            // c.pos[index].x = x;
            // c.pos[index].y = y;
            // c.radii[index] = radius;
            // c.is_active_flags[index] = is_active;

            auto& circle = c.circles[index];
            circle.pos = v2f{ x, y };
            circle.radius = radius;

            if (is_active) c.active_indices.add(index);
            else c.active_indices.remove(index);
        },
        "set_active_flag_all", [](CircleColliderGridComponent& c, bool is_active)
        {
            for (int i = 0; i < c.element_count; i++)
            {
                // c.is_active_flags[i] = is_active;
                if (is_active) c.active_indices.add(i);
                else c.active_indices.remove(i);
            }
            c.is_active = is_active;
        },
        // "get_radius", [](CircleColliderGridComponent& ccsc, int index) -> float {
        //     if (index < 0 || index >= GridSize) throw std::out_of_range("Index out of range");
        //     return ccsc.radii[index];
        // },
        // "set_radius", [](CircleColliderGridComponent& ccsc, int index, float value) {
        //     if (index < 0 || index >= GridSize) throw std::out_of_range("Index out of range");
        //     ccsc.radii[index] = value;
        // },
        // "get_is_active_flag", [](CircleColliderGridComponent& ccsc, int index) -> float {
        //     if (index < 0 || index >= GridSize) throw std::out_of_range("Index out of range");
        //     return ccsc.is_active_flags[index];
        // },
        "set_active_flag_at", [](CircleColliderGridComponent& c, int index, bool is_active)
        {
            assert(index >= 0 && index < c.element_count);
            //if (index < 0 || index >= GridSize) throw std::out_of_range("Index out of range");
            //std::cout << index << std::endl;
            // c.is_active_flags[index] = is_active;
            if (is_active) c.active_indices.add(index);
            else c.active_indices.remove(index);
        },
        // TODO
        "is_any_active", [](CircleColliderGridComponent& c) -> bool
        {
            return c.active_indices.get_dense_count() > 0;
            // for (int i = 0; i < c.count; i++)
            // {
            //     if (c.is_active_flags[i]) return true;
            // }
            // return false;
        },
        "get_element_count", [](CircleColliderGridComponent& c)
        {
            return c.element_count;
        },
        "is_active",
        &CircleColliderGridComponent::is_active,

        sol::meta_function::to_string,
        &CircleColliderGridComponent::to_string
    );
}

// === IslandFinderComponent ==================================================

std::string IslandFinderComponent::to_string() const
{
    return "IslandFinderComponent { ... }";
}

template<>
void register_meta<IslandFinderComponent>(Editor::Context& context)
{
    entt::meta<IslandFinderComponent>()
        .type("IslandFinderComponent"_hs).prop(display_name_hs, "IslandFinder")

        .data<&IslandFinderComponent::core_x>("core_x"_hs).prop(display_name_hs, "core_x")
        .data<&IslandFinderComponent::core_y>("core_y"_hs).prop(display_name_hs, "core_y")//.prop(readonly_hs, true)
        .data<&IslandFinderComponent::islands>("islands"_hs).prop(display_name_hs, "islands") // ???
        ;

    assert(context.lua);
    context.lua->new_usertype<IslandFinderComponent>("IslandFinderComponent",
        "type_id",
        &entt::type_hash<IslandFinderComponent>::value,
        sol::call_constructor,
        sol::factories([](int core_x, int core_y) {
            return IslandFinderComponent{
                .core_x = core_x,
                .core_y = core_y
            };
            }),
        "get_nbr_islands", [](IslandFinderComponent& c) {
            return c.islands.size();
        },
        "get_island_index_at", [](IslandFinderComponent& c, int index) {
            assert(index < c.islands.size());
            return c.islands[index];
        },

        sol::meta_function::to_string,
        &IslandFinderComponent::to_string
    );
}

// === QuadGridComponent ======================================================

std::string QuadGridComponent::to_string() const
{
    return "QuadGridComponent { ... }";
    // std::stringstream ss;
    // ss << "{ size = ";
    // for (int i = 0; i < count; i++) ss << std::to_string(sizes[i]) << ", ";
    // ss << "{ colors = ";
    // for (int i = 0; i < count; i++) ss << std::to_string(colors[i]) << ", ";
    // ss << "{ is_active_flags = ";
    // for (int i = 0; i < count; i++) ss << std::to_string(is_active_flags[i]) << ", ";
    // return ss.str();
}

template<>
void register_meta<QuadGridComponent>(Editor::Context& context)
{
    entt::meta<QuadGridComponent>()
        .type("QuadGridComponent"_hs).prop(display_name_hs, "QuadGrid")

        .data<&QuadGridComponent::count>("count"_hs).prop(display_name_hs, "count").prop(readonly_hs, true)
        .data<&QuadGridComponent::width>("width"_hs).prop(display_name_hs, "width").prop(readonly_hs, true)
        .data<&QuadGridComponent::is_active>("is_active"_hs).prop(display_name_hs, "is_active")

        // Todo: custom inspect for this type to visualize arrays more efficently
        .data<&QuadGridComponent::positions>("positions"_hs).prop(display_name_hs, "positions")
        .data<&QuadGridComponent::sizes>("sizes"_hs).prop(display_name_hs, "sizes")
        .data<&QuadGridComponent::colors>("colors"_hs).prop(display_name_hs, "colors")
        .data<&QuadGridComponent::is_active_flags>("is_active_flags"_hs).prop(display_name_hs, "is_active_flags")
        ;

    assert(context.lua);
    context.lua->new_usertype<QuadGridComponent>("QuadGridComponent",

        "type_id",
        &entt::type_hash<QuadGridComponent>::value,

        sol::call_constructor,
        sol::factories([](
            int width,
            int height,
            bool is_active)
            {
                assert(width * height <= GridSize);
                return QuadGridComponent{
                    .count = width * height,
                    .width = width,
                    .is_active = is_active
                };
            }),

        // Needed for value-copying
        // Skip - has containers I cannot copy
        // "construct",
        // []() { return QuadGridComponent{}; },

        "count", &QuadGridComponent::count,
        "width", &QuadGridComponent::width,
        "is_active", &QuadGridComponent::is_active,

        // Containers become userdata - not sure how to clone/inspect/serialize when type-erased
        // "positions", &QuadGridComponent::positions,
        // "sizes", &QuadGridComponent::sizes,
        // "colors", &QuadGridComponent::colors,
        // "is_active_flags", &QuadGridComponent::is_active_flags,

        sol::meta_function::to_string,
        &QuadGridComponent::to_string,

        "set_quad_at",
        [](QuadGridComponent& c,
            int index,
            float x,
            float y,
            float size,
            uint32_t color,
            bool is_active)
        {
            assert(index >= 0 && index < c.count);
            c.positions[index].x = x;
            c.positions[index].y = y;
            c.sizes[index] = size;
            c.colors[index] = color;
            // if (is_active && !c.is_active_flags[index]) c.active_indices[c.nbr_active++] = index;
            c.is_active_flags[index] = is_active;
        },

        "set_active_flag_all", [](QuadGridComponent& c, bool is_active) {
            for (int i = 0; i < c.count; i++)
                c.is_active_flags[i] = is_active;
            c.is_active = is_active;
        },

        "get_pos_at", [](QuadGridComponent& c, int index) {
            //if (index < 0 || index >= c.count) throw std::out_of_range("Index out of range");
            assert(index >= 0 && index < c.count);
            return std::make_tuple(c.positions[index].x, c.positions[index].y);
        },
        // "set_pos", [](QuadGridComponent& c, int index, float x, float y) {
        //     if (index < 0 || index >= GridSize) throw std::out_of_range("Index out of range");
        //     c.pos[index].x = x;
        //     c.pos[index].y = y;
        // },

        "get_size_at", [](QuadGridComponent& c, int index) -> float {
            //if (index < 0 || index >= GridSize) throw std::out_of_range("Index out of range");
            assert(index >= 0 && index < c.count);
            return c.sizes[index];
        },
        // "set_size", [](QuadGridComponent& c, int index, float value) {
        //     if (index < 0 || index >= GridSize) throw std::out_of_range("Index out of range");
        //     c.sizes[index] = value;
        // },

        "get_color_at", [](QuadGridComponent& c, int index) -> uint32_t {
            // if (index < 0 || index >= GridSize) throw std::out_of_range("Index out of range");
            assert(index >= 0 && index < c.count);
            return c.colors[index];
        },

        "set_color_at", [](QuadGridComponent& c, int index, uint32_t color) {
            // if (index < 0 || index >= GridSize) throw std::out_of_range("Index out of range");
            assert(index >= 0 && index < c.count);
            c.colors[index] = color;
        },

        "set_color_all", [](QuadGridComponent& c, uint32_t color) {
            for (int i = 0; i < c.count; i++)
                c.colors[i] = color;
        },

        // "get_is_active_flag", [](QuadGridComponent& c, int index) -> float {
        //     if (index < 0 || index >= GridSize) throw std::out_of_range("Index out of range");
        //     return c.is_active_flags[index];
        // },
        "set_active_flag_at", [](QuadGridComponent& c, int index, bool is_active) {
            // if (index < 0 || index >= GridSize) throw std::out_of_range("Index out of range");
            assert(index >= 0 && index < c.count);
            c.is_active_flags[index] = is_active;
        },

        "get_element_count", [](QuadGridComponent& c) {
            return c.count;
        }
    );
}

// === DataGridComponent ======================================================

std::string DataGridComponent::to_string() const
{
    return "DataGridComponent { ... }";
}

template<>
void register_meta<DataGridComponent>(Editor::Context& context)
{
    entt::meta<DataGridComponent>()
        .type("DataGridComponent"_hs).prop(display_name_hs, "DataGrid")

        .data<&DataGridComponent::count>("count"_hs).prop(display_name_hs, "count").prop(readonly_hs, true)
        .data<&DataGridComponent::width>("width"_hs).prop(display_name_hs, "width").prop(readonly_hs, true)

        // Todo: custom inspect for this type to visualize arrays more efficently
        .data<&DataGridComponent::slot1>("slot1"_hs).prop(display_name_hs, "slot1")
        .data<&DataGridComponent::slot2>("slot2"_hs).prop(display_name_hs, "slot2")
        ;

    assert(context.lua);
    context.lua->new_usertype<DataGridComponent>("DataGridComponent",
        "type_id",
        &entt::type_hash<DataGridComponent>::value,

        sol::call_constructor,
        sol::factories([](
            int width,
            int height)
            {
                assert(width * height <= GridSize);
                return DataGridComponent{
                    .count = width * height,
                    .width = width };
            }),
        "set_slot1_at",
        [](DataGridComponent& c,
            int index,
            float value)
        {
            assert(index >= 0 && index < c.count);
            c.slot1[index] = value;
        },
        "set_slot2_at",
        [](DataGridComponent& c,
            int index,
            float value)
        {
            assert(index >= 0 && index < c.count);
            c.slot2[index] = value;
        },
        "get_slot1_at",
        [](DataGridComponent& c, int index)
        {
            assert(index >= 0 && index < c.count);
            return c.slot1[index];
        },
        "get_slot2_at",
        [](DataGridComponent& c, int index)
        {
            assert(index >= 0 && index < c.count);
            return c.slot2[index];
        },

        sol::meta_function::to_string,
        &DataGridComponent::to_string
    );
}

// === ScriptedBehaviorComponent ==============================================

std::string ScriptedBehaviorComponent::to_string() const {
    std::stringstream ss;
    ss << "{ scripts = ";
    for (auto& script : scripts) ss << script.identifier << " ";
    ss << "}";
    return ss.str();
}

namespace
{
    std::string sol_object_to_string(std::shared_ptr<const sol::state> lua, const sol::object object)
    {
        return lua->operator[]("tostring")(object).get<std::string>();
    }

    std::string get_lua_type_name(std::shared_ptr<const sol::state> lua, const sol::object object)
    {
        return lua_typename(lua->lua_state(), static_cast<int>(object.get_type()));
    }
}

namespace {

    bool is_array_table(const sol::table& tbl) {
        void table_to_json(nlohmann::json & j, const sol::table & tbl);

        size_t index = 1;
        for (const auto& [key, value] : tbl) {
            if (key.get_type() != sol::type::number || key.as<size_t>() != index++) {
                return false;
            }
        }
        return true;
    }

    void print_table(const sol::table& tbl, int depth = 0) {
        auto indent = [](int d) { return std::string(d * 2, ' '); }; // Indentation function

        if (is_array_table(tbl)) {
            // Handle array-style table
            for (auto kv : tbl) {
                std::cout << indent(depth) << "- ";
                auto value_type = kv.second.get_type();
                switch (value_type) {
                case sol::type::table:
                    std::cout << "{\n";
                    print_table(kv.second.as<sol::table>(), depth + 1); // Recursive call
                    std::cout << indent(depth) << "}\n";
                    break;
                case sol::type::string:
                    std::cout << kv.second.as<std::string>() << "\n";
                    break;
                case sol::type::number:
                    std::cout << kv.second.as<double>() << "\n";
                    break;
                case sol::type::boolean:
                    std::cout << (kv.second.as<bool>() ? "true" : "false") << "\n";
                    break;
                case sol::type::userdata:
                    std::cout << "<userdata>\n";
                    break;
                case sol::type::lightuserdata:
                    std::cout << "<lightuserdata>\n";
                    break;
                case sol::type::lua_nil:
                    std::cout << "nil\n";
                    break;
                case sol::type::function:
                    std::cout << "<function>\n";
                    break;
                default:
                    std::cout << "<unknown>\n";
                    break;
                }
            }
        }
        else {
            // Handle map-style table
            for (const auto& kv : tbl) {
                auto key = kv.first.as<std::string>();
                auto value_type = kv.second.get_type();

                std::cout << indent(depth) << key << ": ";

                switch (value_type) {
                case sol::type::table:
                    std::cout << "{\n";
                    print_table(kv.second.as<sol::table>(), depth + 1); // Recursive call
                    std::cout << indent(depth) << "}\n";
                    break;
                case sol::type::string:
                    std::cout << kv.second.as<std::string>() << "\n";
                    break;
                case sol::type::number:
                    std::cout << kv.second.as<double>() << "\n";
                    break;
                case sol::type::boolean:
                    std::cout << (kv.second.as<bool>() ? "true" : "false") << "\n";
                    break;
                case sol::type::userdata:
                    std::cout << "<userdata>\n";
                    break;
                case sol::type::lightuserdata:
                    std::cout << "<lightuserdata>\n";
                    break;
                case sol::type::lua_nil:
                    std::cout << "nil\n";
                    break;
                case sol::type::function:
                    std::cout << "<function>\n";
                    break;
                default:
                    std::cout << "<unknown>\n";
                    break;
                }
            }
        }
    }

    bool check_field_tag(const sol::table& tbl, const sol::object& key, const std::string& tag_name)
    {
        // Ensure the key is a string (assuming only string keys are tagged)
        if (!key.is<std::string>()) {
            return false; // Non-string keys cannot have tags
        }

        std::string field_name = key.as<std::string>();

        // Access the optional meta table
        sol::optional<sol::table> maybe_meta = tbl["meta"];
        if (!maybe_meta) {
            return false; // No meta table, so no tags
        }

        sol::table meta = *maybe_meta;

        // Access the optional tags table within meta
        // sol::optional<sol::table> maybe_tags = meta["tags"];
        // if (!maybe_tags) {
        //     return false; // No tags table, so no tags
        // }

        // sol::table tags = *maybe_tags;

        // Access the tags for the specific field
        sol::optional<sol::table> maybe_field_tags = meta[field_name];
        if (!maybe_field_tags) {
            return false; // No tags for this field
        }

        sol::table field_tags = *maybe_field_tags;

        // Check the specified tag
        return field_tags[tag_name].get_or(false);
    }
}

// Lua inspection
namespace Editor {

    /// Inspect sol::function
    template<>
    bool inspect_type<sol::function>(sol::function& t, InspectorState& inspector)
    {
        ImGui::TextDisabled("%s", sol_object_to_string(inspector.context.lua, t).c_str());
        return false;
    }

    /// Inspect sol::table
    template<>
    bool inspect_type<sol::table>(sol::table&, InspectorState&);

    /// Inspect sol::userdata
    template<>
    bool inspect_type<sol::userdata>(sol::userdata& userdata, InspectorState& inspector)
    {
        auto& lua = inspector.context.lua;
        bool mod = false;

        // Fetch type id of the inspected usertype
        auto type_id = userdata["type_id"];

#if 0
        // Inspect the raw metatable directly
        sol::table metatable = userdata[sol::metatable_key];
        mod |= inspect_type(metatable, inspector);
        return mod;
#endif

        // Non-component types typically don't have a "type_id" field
        if (!type_id.valid())
        {
#if 1
            // No inspection
            ImGui::TextDisabled("[Unvailable]");
            return mod;
#else
            // Check if the userdata has an `__index` metamethod that acts like a table
            sol::optional<sol::table> metatable = userdata[sol::metatable_key];
            if (metatable && metatable->get<sol::object>("__index").is<sol::table>())
            {
                sol::table index_table = metatable->get<sol::table>("__index");
                mod |= Editor::inspect_type(index_table, inspector);
            }
            else
                ImGui::TextDisabled("[Unvailable]");
            return mod;
#endif
}

        assert(type_id.get_type() == sol::type::function);
        entt::id_type id = type_id.call();

        // Get entt meta type for this type id
        auto meta_type = entt::resolve(id);
        assert(meta_type);

        // List type fields via entt::meta
        for (auto&& [id, meta_data] : meta_type.data())
        {
            // entt field name
            std::string key_name = meta_data_name(id, meta_data);
            const auto key_name_cstr = key_name.c_str();

            bool readonly = get_meta_data_prop<bool, ReadonlyDefault>(meta_data, readonly_hs);
            if (readonly) inspector.begin_disabled();

            // Fetch usertype field with this field name
            // Note: requires fields to be registered with the EXACT same name ...
            sol::object value = userdata[key_name_cstr];

            if (!value)
            {
                // entt meta data was not found in sol usertype
                inspector.begin_leaf(key_name_cstr);
                ImGui::TextDisabled("[Not in usertype]");
                inspector.end_leaf();
            }
            // userdata, table ???
            else if (value.get_type() == sol::type::string)
            {
                std::string str = value.as<std::string>();
                inspector.begin_leaf(key_name_cstr);
                if (inspect_type(str, inspector)) { userdata[key_name_cstr] = str; mod = true; }
                inspector.end_leaf();
            }
            else if (value.get_type() == sol::type::number)
            {
                double nbr = value.as<double>();
                inspector.begin_leaf(key_name_cstr);
                if (inspect_type(nbr, inspector)) { userdata[key_name_cstr] = nbr; mod = true; }
                inspector.end_leaf();
            }
            else if (value.get_type() == sol::type::boolean)
            {
                bool bl = value.as<bool>();
                inspector.begin_leaf(key_name_cstr);
                if (inspect_type(bl, inspector)) { userdata[key_name_cstr] = bl; mod = true; }
                inspector.end_leaf();
            }
            else
            {
                inspector.begin_leaf(key_name_cstr);
                ImGui::TextDisabled("[to_string] %s", sol_object_to_string(lua, value).c_str());
                inspector.end_leaf();
            }
            if (readonly) inspector.end_disabled();
        }
        return mod;
    }

    template<>
    bool inspect_type<sol::table>(sol::table& tbl, InspectorState& inspector)
    {
        // std::cout << "inspecting sol::table" << std::endl;
        auto& lua = inspector.context.lua;
        bool mod = false;

        assert(tbl.valid());
        if (!tbl.valid()) return mod;

        // TODO inspect array_table
        // if (is_array_table(tbl)) {
        //     array_table_to_json(j, tbl);
        //     return;
        // }

        for (auto& [key, value] : tbl)
        {
            if (!check_field_tag(tbl, key, "inspectable")) continue;

            std::string key_str = sol_object_to_string(lua, key) + " [" + get_lua_type_name(lua, value) + "]";
            std::string key_str_label = "##" + key_str;

            // Note: value.is<sol::table>() is true also for sol::type::userdata and possibly other lua types
            // This form,
            //      value.get_type() == sol::type::table
            // seems more precise when detecting types, even though it isn't type-safe
            if (value.get_type() == sol::type::table)
            {
                if (inspector.begin_node(key_str.c_str()))
                {
                    sol::table tbl_nested = value.as<sol::table>();

                    mod |= Editor::inspect_type(tbl_nested, inspector);
                    inspector.end_node();
                }
            }
            else if (value.get_type() == sol::type::userdata)
            {
                if (inspector.begin_node(key_str.c_str()))
                {
                    sol::userdata userdata = value.as<sol::userdata>();
                    mod |= inspect_type(userdata, inspector);
                    inspector.end_node();
                }
            }
            else
            {
                inspector.begin_leaf(key_str.c_str());

                if (value.get_type() == sol::type::function)
                {
                    sol::function func = value.as<sol::function>();
                    mod |= Editor::inspect_type(func, inspector);
                }
                else if (value.get_type() == sol::type::number)
                {
                    double dbl = value.as<double>();
                    if (ImGui::InputDouble(key_str_label.c_str(), &dbl, 0.1, 0.5))
                    {
                        // Commit modified value to Lua
                        tbl[key] = dbl;
                        mod = true;
                    }
                }
                else if (value.get_type() == sol::type::boolean)
                {
                    bool b = value.as<bool>();
                    if (ImGui::Checkbox(key_str_label.c_str(), &b))
                    {
                        // Commit modified value to Lua
                        tbl[key] = b;
                        mod = true;
                    }
                }
                else if (value.get_type() == sol::type::string)
                {
                    std::string str = value.as<std::string>();
                    if (inspect_type(str, inspector))
                    {
                        // Commit modified value to Lua
                        tbl[key] = str;
                        mod = true;
                    }
                }
                else
                {
                    // Fallback: display object as a string
                    // Applies to
                    // sol::type::lightuserdata
                    // sol::type::lua_nil
                    // sol::type::none
                    // sol::type::poly
                    // sol::type::thread

                    ImGui::TextDisabled("%s", sol_object_to_string(lua, value).c_str());
                }
                inspector.end_leaf();
            }
        }

        return mod;
    }

    template<>
    bool inspect_type<BehaviorScript>(BehaviorScript& script, InspectorState& inspector)
    {
        bool mod = false;

        inspector.begin_leaf("id");
        inspector.begin_disabled();
        mod |= inspect_type(script.identifier, inspector);
        inspector.end_disabled();
        inspector.end_leaf();

        inspector.begin_leaf("path");
        inspector.begin_disabled();
        mod |= inspect_type(script.path, inspector);
        inspector.end_disabled();
        inspector.end_leaf();

        if (inspector.begin_node("script"))
        {
            mod |= inspect_type(script.self, inspector);
            inspector.end_node();
        }

        return mod;
    }
}

// Lua copying
namespace {
    void deep_copy_table(const sol::table& tbl_src, sol::table& tbl_dst);

    void deep_copy_userdata(const sol::userdata& userdata_src, sol::userdata& userdata_dst)
    {
        // sol::function construct = userdata_src["construct"];
        // if (!construct.valid())  return userdata_src; // Copy by reference
        // // Let userdata_src decide how to copy
        // sol::userdata userdata_cpy = construct();

        // ASSUME DEST IS LOADED?
        assert(userdata_dst.valid());
        if (!userdata_dst.valid())
        {
            sol::function construct = userdata_src["construct"];
            if (!construct.valid()) { userdata_dst = userdata_src; return; } // return userdata_src; // Fallback: copy by reference
            sol::userdata userdata_cpy = construct();
        }

        // Fetch type id
        auto type_id = userdata_src["type_id"];
        if (type_id.get_type() != sol::type::function) { userdata_dst = userdata_src; return; } // return userdata_src;

        // Fetch entt meta type, iterate its data and copy to userdata
        entt::id_type id = type_id.call();
        entt::meta_type meta_type = entt::resolve(id);
        assert(meta_type);
        // entt::meta_any meta_any = meta_type.construct(); // cannot go any -> userdata_src
        for (auto&& [id, meta_data] : meta_type.data())
        {
            // entt field name
            std::string key_name = meta_data_name(id, meta_data); // Don't use displayname
            const auto key_name_cstr = key_name.c_str();

            sol::object value = userdata_src[key_name_cstr];

            if (!value.valid())
            {
                std::cout << "Copy userdata warning: meta data '" << key_name
                    << "' was not found in userdata" << std::endl;
                assert(0);
                continue;
            }
            else if (value.get_type() == sol::type::table)
            {
                // userdata_cpy[key_name_cstr] = deep_copy_table(lua, value.as<sol::table>());

                sol::table nested_tbl_dst = userdata_dst[key_name_cstr];
                deep_copy_table(value.as<sol::table>(), nested_tbl_dst);
            }
            else if (value.get_type() == sol::type::userdata)
            {
                // userdata_cpy[key_name_cstr] = deep_copy_userdata(lua, value.as<sol::userdata>());

                sol::userdata nested_userdata_dst = userdata_dst[key_name_cstr];
                deep_copy_userdata(value.as<sol::userdata>(), nested_userdata_dst);
            }
            else
            {
                userdata_dst[key_name_cstr] = value;
            }
        }

        // return userdata_cpy;
    }

    void deep_copy_table(const sol::table& tbl_src, sol::table& tbl_dst)
    {
        // ASSUME DEST IS LOADED?
        assert(tbl_dst.valid());
        if (!tbl_dst.valid())
        {
            sol::state_view lua = tbl_src.lua_state();
            tbl_dst = lua.create_table();
        }
        // assert(tbl_dst.get_type() == sol::type::table);
        //sol::table tbl_dst = lua.create_table();

        for (const auto& [key, value] : tbl_src)
        {
            if (!check_field_tag(tbl_src, key, "serializable")) continue;

            if (value.get_type() == sol::type::table)
            {
                sol::table nested_tbl_dst = tbl_dst[key];
                deep_copy_table(value.as<sol::table>(), nested_tbl_dst);
            }
            else if (value.get_type() == sol::type::userdata)
            {
                //tbl_dst[key] = deep_copy_userdata(lua, value.as<sol::userdata>());

                sol::userdata nested_userdata_dst = tbl_dst[key];
                deep_copy_userdata(value.as<sol::userdata>(), nested_userdata_dst);

            }
            else
            {
                tbl_dst[key] = value;
            }
        }
        // return copy;
    }

    BehaviorScript copy_BehaviorScript(const void* ptr, entt::entity dst_entity)
    {
        auto script = static_cast<const BehaviorScript*>(ptr);
        // std::cout << "COPY BehaviorScript" << std::endl;


        // ScriptedBehaviorComponent comp_cpy;

        // for (auto& script : comp_ptr->scripts)
        // {
        auto& self = script->self;

        // LOAD
        auto lua_registry = self["owner"];      assert(lua_registry.valid());
        //if (registry_ref.get_type() == sol::type::userdata) std::cout << "registry_ref is userdata\n";
        //if (rebind_func.get_type() == sol::type::function) std::cout << "rebind_func is function\n";

        // Get registry pointer from source script
        sol::optional<entt::registry*> registry_opt = lua_registry;
        assert(registry_opt);
        entt::registry* registry_ptr = *registry_opt;

        //entt::registry& registry_ptr = lua_registry.get<entt::registry&>(); // REPLACES REBIND?
        assert(registry_ptr);
        assert(registry_ptr->valid(dst_entity));
        //std::cout << registry_ptr->valid(dst_entity) << std::endl; //

        // Do for the copied table
        //auto rebind_func = lua_registry.operator[]("rebind");  assert(rebind_func.valid());
        //rebind_func(registry_ref, self);

        // CHECK
        // auto new_registry_ref = self["owner"];
        // assert(new_registry_ref.valid());
        // auto new_rebind = self["owner"]["rebind"];
        // assert(new_rebind.valid());

        auto script_cpy = BehaviorScriptFactory::create_from_file(
            *registry_ptr,
            dst_entity,
            self.lua_state(),
            script->path,
            script->identifier);

        deep_copy_table(self, script_cpy.self);
        // COPY LUA META FIELDS from self -> script_cpy.self
        // = deep_copy_table 
        //      without lua.create_table() 
        //      & without userdata["construct"]
        //      LOAD creates these - we just need to update specific fields

        // comp_cpy.scripts.push_back(std::move(script_cpy));
    // }

        // std::cout << "DONE COPY BehaviorScript" << std::endl << std::flush;
        return script_cpy;
    };

    ScriptedBehaviorComponent copy_ScriptedBehaviorComponent(const void* ptr, entt::entity dst_entity)
    {
        auto comp_ptr = static_cast<const ScriptedBehaviorComponent*>(ptr);
        std::cout << "COPY ScriptedBehaviorComponent" << std::endl;

        ScriptedBehaviorComponent comp_cpy;

        for (auto& script : comp_ptr->scripts)
        {
            comp_cpy.scripts.push_back(std::move(copy_BehaviorScript(&script, dst_entity)));
        }

        std::cout << "DONE COPY ScriptedBehaviorComponent" << std::endl << std::flush;
        return comp_cpy;
    }
}

// Lua serialization
namespace {

    void table_to_json(nlohmann::json& j, const sol::table& tbl);

    // Sequential (array-like) Lua table to a JSON array
    void array_table_to_json(nlohmann::json& j, const sol::table& tbl)
    {
        j = nlohmann::json::array();
        for (const auto& [key, value] : tbl)
        {
            nlohmann::json element;
            if (value.get_type() == sol::type::table)
            {
                table_to_json(element, value.as<sol::table>());  // Recursive handling of nested tables
            }
            else if (value.get_type() == sol::type::string)
            {
                element = value.as<std::string>();
            }
            else if (value.get_type() == sol::type::number)
            {
                element = value.as<double>();
            }
            else if (value.get_type() == sol::type::boolean)
            {
                element = value.as<bool>();
            }
            else if (value.get_type() == sol::type::lua_nil || value.get_type() == sol::type::none)
            {
                element = nullptr;
            }
            else
            {
                // Skip unsupported types
                continue;
            }
            j.push_back(element);
        }
    }

    // TODO
    void userdata_to_json(nlohmann::json& j, const sol::userdata& userdata)
    {
        // Use fields defined by the type's entt::meta
        j = "userdata...";
    }

    // sol::table to JSON
    void table_to_json(nlohmann::json& j, const sol::table& tbl)
    {
        assert(tbl.valid());

        if (is_array_table(tbl)) {
            array_table_to_json(j, tbl);
            return;
        }

        j = nlohmann::json::object();
        for (const auto& [key, value] : tbl)
        {
            if (!check_field_tag(tbl, key, "serializable")) continue;

            std::string json_key;

            if (key.get_type() == sol::type::string)
            {
                json_key = key.as<std::string>();
            }
            else if (key.get_type() == sol::type::number)
            {
                json_key = std::to_string(key.as<double>());
            }
            else {
                // Skip unsupported key types
                continue;
            }

            if (value.get_type() == sol::type::table)
            {
                nlohmann::json nested_json;
                table_to_json(nested_json, value.as<sol::table>());
                j[json_key] = nested_json;
            }
            else if (value.get_type() == sol::type::userdata)
            {
                nlohmann::json nested_json;
                userdata_to_json(nested_json, value.as<sol::userdata>());
                j[json_key] = nested_json;
            }
            else if (value.get_type() == sol::type::string)
            {
                j[json_key] = value.as<std::string>();
            }
            else if (value.get_type() == sol::type::number)
            {
                j[json_key] = value.as<double>();
            }
            else if (value.get_type() == sol::type::boolean)
            {
                j[json_key] = value.as<bool>();
            }
            else if (value.get_type() == sol::type::lua_nil || value.get_type() == sol::type::none)
            {
                j[json_key] = nullptr;
            }
            else
            {
                // Skip unsupported types
                continue;
            }
        }

    }

    void BehaviorScript_to_json(nlohmann::json& j, const void* ptr)
    {
        std::cout << "BehaviorScript_to_json\n";

        auto script = static_cast<const BehaviorScript*>(ptr);

        // self sol::table
        nlohmann::json self_json;
        table_to_json(self_json, script->self);
        j["self"] = self_json;

        j["identifier"] = script->identifier;
        j["path"] = script->path;
    }

}

// Lua deserialization
namespace {

    void table_from_json(sol::table& tbl, const nlohmann::json& j);

    // TODO: usertypes are expected to remain unchanged when scripts are edited,
    // so the serialized JSON 
    void usertype_from_json(sol::object usertype_obj, const nlohmann::json& json_value)
    {
        // Ensure the JSON value is an object (as expected for usertypes)
        if (!json_value.is_object())
        {
            return; // Skip if the JSON does not match the expected structure
        }

        for (const auto& [subkey, subvalue] : json_value.items())
        {
            if (subvalue.is_number_float())
            {
                usertype_obj.as<sol::table>()[subkey] = subvalue.get<double>();
            }
            else if (subvalue.is_number_integer())
            {
                usertype_obj.as<sol::table>()[subkey] = subvalue.get<int>();
            }
            else if (subvalue.is_string())
            {
                usertype_obj.as<sol::table>()[subkey] = subvalue.get<std::string>();
            }
            else if (subvalue.is_boolean())
            {
                usertype_obj.as<sol::table>()[subkey] = subvalue.get<bool>();
            }
            else
            {
                // Skip unsupported types
                continue;
            }
        }
    }

    // Function to handle JSON arrays and populate a Lua table
    void arrayjson_to_table(sol::table& tbl, const nlohmann::json& j)
    {
        // Ensure the Lua table is array-like before proceeding
        if (!is_array_table(tbl))
        {
            return; // Skip processing if the table is not sequential
        }

        size_t index = 1; // Lua arrays are 1-indexed
        for (const auto& element : j)
        {
            sol::object existing_value = tbl[index];

            if (existing_value.valid())
            {
                // Check type consistency for existing Lua table elements
                if (existing_value.get_type() == sol::type::string && element.is_string())
                {
                    tbl[index++] = element.get<std::string>();
                }
                else if (existing_value.get_type() == sol::type::number && element.is_number())
                {
                    tbl[index++] = element.get<double>();
                }
                else if (existing_value.get_type() == sol::type::boolean && element.is_boolean())
                {
                    tbl[index++] = element.get<bool>();
                }
                else if (existing_value.get_type() == sol::type::table && element.is_object())
                {
                    // Recursively handle nested tables
                    sol::table nested_tbl = tbl[index++];
                    table_from_json(nested_tbl, element);
                }
                else if (existing_value.get_type() == sol::type::table && element.is_array())
                {
                    // Recursively handle nested arrays
                    sol::table nested_tbl = tbl[index++];
                    arrayjson_to_table(nested_tbl, element);
                }
                else
                {
                    // Skip mismatched types
                    ++index;
                    continue;
                }
            }
            else
            {
                // If no existing value, assign new element based on its type
                if (element.is_string())
                {
                    tbl[index++] = element.get<std::string>();
                }
                else if (element.is_number_float())
                {
                    tbl[index++] = element.get<double>();
                }
                else if (element.is_number_integer())
                {
                    tbl[index++] = element.get<int>();
                }
                else if (element.is_boolean())
                {
                    tbl[index++] = element.get<bool>();
                }
                else if (element.is_object())
                {
                    sol::table nested_tbl = tbl.create(index++);
                    table_from_json(nested_tbl, element);
                }
                else if (element.is_array())
                {
                    sol::table nested_tbl = tbl.create(index++);
                    arrayjson_to_table(nested_tbl, element);
                }
                else if (element.is_null())
                {
                    tbl[index++] = sol::lua_nil; // Represent null values as Lua nil
                }
                else
                {
                    // Skip unsupported types
                    ++index;
                    continue;
                }
            }
        }
    }

    // Main function to populate a Lua table from JSON
    void table_from_json(sol::table& tbl, const nlohmann::json& j)
    {
        for (const auto& [key, default_value] : tbl)
        {
            std::string key_str = key.as<std::string>();

            if (j.contains(key_str))
            {
                const auto& json_value = j[key_str];

                if (json_value.is_array())
                {
                    // Check if the Lua table is truly an array
                    if (default_value.get_type() == sol::type::table && is_array_table(tbl[key_str]))
                    {
                        sol::table nested_tbl = tbl[key_str];
                        arrayjson_to_table(nested_tbl, json_value);
                    }
                    else
                    {
                        continue; // Ignore JSON array if the Lua table is not array-like
                    }
                }
                else if (default_value.get_type() == sol::type::userdata)
                {
                    // Ensure JSON is an object before calling usertype_from_json
                    if (json_value.is_object())
                    {
                        usertype_from_json(tbl[key_str], json_value);
                    }
                    else
                    {
                        continue; // Skip if JSON does not match expected structure
                    }
                }
                else if (json_value.is_object())
                {
                    // Check if the Lua table is a nested table
                    if (default_value.get_type() == sol::type::table)
                    {
                        sol::table nested_tbl = tbl[key_str];
                        table_from_json(nested_tbl, json_value);
                    }
                    else
                    {
                        continue; // Ignore mismatched JSON type
                    }
                }
                else if (json_value.is_string())
                {
                    if (default_value.get_type() == sol::type::string)
                    {
                        tbl[key_str] = json_value.get<std::string>();
                    }
                    else
                    {
                        continue; // Ignore mismatched JSON type
                    }
                }
                else if (json_value.is_number_float())
                {
                    if (default_value.get_type() == sol::type::number)
                    {
                        tbl[key_str] = json_value.get<double>();
                    }
                    else
                    {
                        continue; // Ignore mismatched JSON type
                    }
                }
                else if (json_value.is_number_integer())
                {
                    if (default_value.get_type() == sol::type::number)
                    {
                        tbl[key_str] = json_value.get<int>();
                    }
                    else
                    {
                        continue; // Ignore mismatched JSON type
                    }
                }
                else if (json_value.is_boolean())
                {
                    if (default_value.get_type() == sol::type::boolean)
                    {
                        tbl[key_str] = json_value.get<bool>();
                    }
                    else
                    {
                        continue; // Ignore mismatched JSON type
                    }
                }
                else if (json_value.is_null())
                {
                    tbl[key_str] = sol::lua_nil; // Allow null values to reset the Lua value to nil
                }
            }
            // If the key does not exist in JSON, leave the default value intact
        }
    }


    // + Context (registry)
    // + dst_entity
    void BehaviorScript_from_json(
        const nlohmann::json& j,
        void* ptr,
        const Entity& entity,
        Editor::Context& context)
    {
        std::cout << "BehaviorScript_from_json\n";

        auto script = static_cast<BehaviorScript*>(ptr);
        std::string identifier = j["identifier"];
        std::string path = j["path"];

        // self sol::table
        // LOAD + copy Lua meta fields
        *script = BehaviorScriptFactory::create_from_file(
            *context.registry,
            entity,
            *context.lua, // self.lua_state() <- nothing here
            path,
            identifier);

        table_from_json(script->self, j["self"]);

        //script->identifier = j["identifier"];
        //script->path = j["path"];

        // *script = BehaviorScript{};
    }
}

void serialization_test(std::shared_ptr<sol::state>& lua)
{
    auto load_lua = [&]() {
        const std::string script_path = "../../LuaGame/lua/bounce_behavior.lua";
        // sol::load_result behavior_script = lua.load_file(script_file);
        sol::load_result behavior_script = lua->load_file(script_path);

        assert(behavior_script.valid());
        sol::protected_function script_function = behavior_script;
        sol::table tbl = script_function();
        assert(tbl.valid());
        return tbl;
        };

    nlohmann::json j;
    {
        std::cout << "\n\n--- SER TEST BEGIN\n\n";
        std::cout << "\n\n--- load & edit table \n\n";
        sol::table tbl = load_lua();
        // print_table(tbl, 0);
        // std::cout << "\n\n--- SERIALIZE edit table\n\n";

        // Editing = make changes to table
        // ...
        tbl["VELOCITY_MIN"] = "ChangedInEditor";
        tbl["FieldAddedInEditor"] = "FieldAddedInEditor";
        print_table(tbl, 0);

        // SAVE
        table_to_json(j, tbl);
        std::cout << "\n\n--- SERIALIZE->JSON " << j.dump() << std::endl << std::endl;
    }

    // Change script
    sol::table tbl = load_lua();

    tbl["VELOCITY_MIN"] = Transform{};  // change to usertype
    tbl["HEADER"] = 15.0f;              // change from usertype

    sol::table array_table = lua->create_table(); array_table[1] = 10; array_table[2] = 20; array_table[3] = 30;
    tbl["VELOCITY_MAX"] = array_table;  // change to array table

    tbl["inventory"] = 35.0f;              // change from array table

    tbl["FieldAddedInScript"] = "FieldAddedInScript";

#if 1
    // LOAD
    {
        // std::cout << "\n\n--- SERIALIZE load table\n\n";
        // sol::table tbl = load_lua();

        std::cout << "\n\n--- LOAD & DESERIALIZE table\n\n";
        table_from_json(tbl, j);
        print_table(tbl, 0);
    }
#endif

    std::cout << "\n\n--- SER TEST DONE \n\n";
}

template<>
void register_meta<ScriptedBehaviorComponent>(Editor::Context& context)
{
    // Note
    // sol::table and sol::function need meta definitions since BehaviorScript 
    // Commands will try to serialize them.
    // If/when BehaviorScript has its own serialization meta function, these might not be needed
#if 1
    // sol::table
    entt::meta<sol::table>()
        .type("sol::table"_hs).prop(display_name_hs, "sol::table")

        // inspect
        //.func < [](void* ptr, Editor::InspectorState& inspector) {return Editor::inspect_type(*static_cast<sol::table*>(ptr), inspector); } > (inspect_hs)

        // clone
        // Note: Let ScriptedBehaviorComponent do the copying
        //.func <&copy_soltable>(clone_hs)
        ;

    // sol::protected_function
    entt::meta<sol::protected_function>()
        .type("sol::protected_function"_hs).prop(display_name_hs, "sol::protected_function")

        // inspect
        //.func<&solfunction_inspect>(inspect_hs)
        // inspect v2 - 'widget not implemented' for BehaviorScript::update, on_collision
        .func < [](void* ptr, Editor::InspectorState& inspector) {return Editor::inspect_type(*static_cast<sol::protected_function*>(ptr), inspector); } > (inspect_hs)
        ;
#endif

    // ScriptedBehaviorComponent::BehaviorScript
    entt::meta<BehaviorScript>()
        .type("BehaviorScript"_hs).prop(display_name_hs, "BehaviorScript")

        .data<&BehaviorScript::identifier>("identifier"_hs)
        .prop(display_name_hs, "identifier")
        .prop(readonly_hs, true)

        .data<&BehaviorScript::path>("path"_hs)
        .prop(display_name_hs, "path")
        .prop(readonly_hs, true)

        // sol stuff
        .data<&BehaviorScript::self/*, entt::as_ref_t*/>("self"_hs).prop(display_name_hs, "self")
        .data<&BehaviorScript::update/*, entt::as_ref_t*/>("update"_hs).prop(display_name_hs, "update")
        .data<&BehaviorScript::on_collision/*, entt::as_ref_t*/>("on_collision"_hs).prop(display_name_hs, "on_collision")

        // inspect_hs
        // We have this, and not one for sol::table, so that when a sol::table if edited,
        // a deep copy of BehaviorScript is made, and not just the sol::table
        .func < [](void* ptr, Editor::InspectorState& inspector) {return Editor::inspect_type(*static_cast<BehaviorScript*>(ptr), inspector); } > (inspect_hs)

        // clone
        .func<&copy_BehaviorScript>(clone_hs)

        // to_json
        .func<&BehaviorScript_to_json>(to_json_hs)

        // from_json
        .func<&BehaviorScript_from_json>(from_json_hs)
        ;

    // [](nlohmann::json& j, const void* ptr)
    //             {
    //                 j = *static_cast<const T*>(ptr);
    //             };

        // ScriptedBehaviorComponent
    entt::meta<ScriptedBehaviorComponent>()
        .type("ScriptedBehaviorComponent"_hs).prop(display_name_hs, "ScriptedBehavior")
        .data<&ScriptedBehaviorComponent::scripts, entt::as_ref_t>("scripts"_hs).prop(display_name_hs, "scripts")

        // Optional meta functions

        // to_string, member version
//        .func<&DebugClass::to_string>(to_string_hs)
        // to_string, lambda version
//        .func < [](const void* ptr) { return static_cast<const HeaderComponent*>(ptr)->name; } > (to_string_hs)
            // inspect
                // .func<&inspect_Transform>(inspect_hs)

            // clone
            // Remove if copy constructor is defined
        .func<&copy_ScriptedBehaviorComponent>(clone_hs)
        ;

    assert(context.lua);
    context.lua->new_usertype<ScriptedBehaviorComponent>("ScriptedBehaviorComponent",
        "type_id",
        &entt::type_hash<ScriptedBehaviorComponent>::value,
        sol::call_constructor,
        sol::factories([]() {
            return ScriptedBehaviorComponent{ };
            }),
        // "scripts",
        // &ScriptedBehaviorComponent::scripts,
        // "get_script_by_id",
        // &ScriptedBehaviorComponent::get_script_by_id,
        sol::meta_function::to_string,
        &ScriptedBehaviorComponent::to_string
    );
}