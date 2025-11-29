#ifndef SceneTypes_hpp
#define SceneTypes_hpp

#include <string>
#include "Entity.hpp" // Include the header file that defines Entity
#include "SelectionManager.hpp" // Include the header file that defines EntitySelection

namespace SceneTypes {

    // Todo: Should not be hard coded obviously
    static inline const std::string script_dir = "../../LuaGame/lua2/";
    static inline const std::string save_dir = "../../LuaGame/json/";

    enum class GamePlayState : int { Play, Stop, Pause };

    using EntitySelection = Editor::SelectionManager<Entity>;

    struct SaveChunkToFileEvent { std::string chunk_tag; };
    struct SaveAllChunksToFileEvent { int placeholder; };
    struct UnloadChunkEvent { std::string chunk_tag; };
    struct UnloadAllChunksEvent { int placeholder; };
    struct LoadChunkFromFileEvent { std::string path; };

    struct SetGamePlayStateEvent { GamePlayState play_state; };

    struct CreateEntityEvent { Entity parent_entity; };
    struct DestroyEntitySelectionEvent { EntitySelection entity_selection; };
    struct CopyEntitySelectionEvent { EntitySelection entity_selection; };

    struct SetParentEntitySelectionEvent { EntitySelection entity_selection; };
    struct UnparentEntitySelectionEvent { EntitySelection entity_selection; };

    struct AddComponentToEntitySelectionEvent { entt::id_type component_id; EntitySelection entity_selection; };
    struct RemoveComponentFromEntitySelectionEvent { entt::id_type component_id; EntitySelection entity_selection; };
    struct AddScriptToEntitySelectionEvent { std::string script_path; EntitySelection entity_selection; };
    struct RemoveScriptFromEntitySelectionEvent { std::string script_path; EntitySelection entity_selection; };

    struct RunScriptEvent { std::string script_path; };

} // SceneCore

#endif