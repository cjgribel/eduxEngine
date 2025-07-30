#ifndef ENTITY_HPP
#define ENTITY_HPP

#include <entt/fwd.hpp> // Forward declarations for entt types
#include <cstdint>      // For uint32_t
#include <functional>   // For std::hash

namespace eeng::ecs
{
    /**
     * @class Entity
     * @brief A wrapper for entities using an integral type, with compatibility for entt::entity.
     */
    class Entity
    {
    public:
        /**
         * @brief The underlying type of the entity.
         */
        using entity_type = std::uint32_t;

        /**
         * @brief Default constructor. Initializes the entity to null.
         */
        Entity();

        /**
         * @brief Constructor that initializes the entity with a given entt::entity.
         * @param e The entt::entity to wrap.
         */
        explicit Entity(entt::entity e);

        /**
         * @brief Constructor that initializes the entity from an entity_type.
         * @param id The entity ID as an entity_type.
         */
        explicit Entity(entity_type id);

        /**
         * @brief Implicit conversion operator to entt::entity.
         * @return The entt::entity representation of the entity.
         */
        operator entt::entity() const;

        /**
         * @brief Retrieves the internal entity as an entity_type.
         * @return The integral representation of the entity.
         */
        entity_type get_id() const;

        /**
         * @brief Sets the internal entity to an entity_type.
         */
        void set_id(entity_type new_id);

        /**
         * @brief Converts the internal entity to its integral representation.
         * @return The integral value of the entity.
         */
        entity_type to_integral() const;

        /**
         * @brief Checks if the entity is null.
         * @return True if the entity is null, false otherwise.
         */
        bool is_null() const;

        /**
         * @brief Sets the entity to null.
         */
        void set_null();

        // Comparison operators
        bool operator==(const Entity& other) const;
        bool operator!=(const Entity& other) const;
        bool operator==(entt::entity other) const;
        bool operator!=(entt::entity other) const;

        /**
         * @brief A constexpr null entity instance.
         */
        static const Entity EntityNull;

    private:
        entity_type id; ///< The internal integral representation of the entity.
    };
} // namespace eeng::ecs

    // Specialization of std::hash for Entity
namespace std
{
    template <>
    struct hash<eeng::ecs::Entity>
    {
        std::size_t operator()(const eeng::ecs::Entity& entity) const noexcept
        {
            return std::hash<eeng::ecs::Entity::entity_type>()(entity.to_integral());
        }
    };
}

#endif // ENTITY_HPP
