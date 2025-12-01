#include "Entity.hpp"
#include <entt/entity/entity.hpp> // Full definition of entt::entity and entt::null

namespace eeng::ecs
{
    // Default constructor
    Entity::Entity()
        : id(static_cast<entity_type>(entt::null))
    {
    }

    // Constructor from entt::entity
    Entity::Entity(entt::entity e)
        : id(static_cast<entity_type>(e))
    {
    }

    // Constructor from entity_type
    Entity::Entity(entity_type id)
        : id(id)
    {
    }

    // Implicit conversion to entt::entity
    Entity::operator entt::entity() const
    {
        return static_cast<entt::entity>(id);
    }

    // Get the internal entity ID
    Entity::entity_type Entity::get_id() const
    {
        return id;
    }

    void Entity::set_id(entity_type new_id)
    {
        id = new_id;
    }

    // Convert to integral type
    Entity::entity_type Entity::to_integral() const
    {
        return id;
    }

    // Check if the entity is null
    bool Entity::is_null() const
    {
        return id == static_cast<entity_type>(entt::null);
    }

    // Set the entity to null
    void Entity::set_null()
    {
        id = static_cast<entity_type>(entt::null);
    }

    // Equality operators
    bool Entity::operator==(const Entity& other) const
    {
        return id == other.id;
    }

    bool Entity::operator!=(const Entity& other) const
    {
        return id != other.id;
    }

    bool Entity::operator==(entt::entity other) const
    {
        return id == static_cast<entity_type>(other);
    }

    bool Entity::operator!=(entt::entity other) const
    {
        return id != static_cast<entity_type>(other);
    }

    // Define the static null entity
    const Entity Entity::EntityNull = Entity();

    EntityRef::EntityRef(const Guid& guid, const ecs::Entity& entity)
        : guid(guid), entity(entity) {
    }

    EntityRef::EntityRef(const Guid& guid)
        : guid(guid), entity(Entity::EntityNull) {
    }

    EntityRef::EntityRef()
        : guid(Guid::invalid()), entity(Entity::EntityNull) {
    }

    // const Guid& EntityRef::get_guid() const { return guid; }
    // void EntityRef::set_guid(const Guid& guid) { this->guid = guid; }

    // const ecs::Entity& EntityRef::get_entity() const { return entity; }
    // void EntityRef::set_entity(const ecs::Entity& entity) { this->entity = entity; }

    void EntityRef::bind(const ecs::Entity& entity) { this->entity = entity; }
    bool EntityRef::is_bound() const { return !entity.is_null(); }
    void EntityRef::unbind() { entity = {}; }
} 