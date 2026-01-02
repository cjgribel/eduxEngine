//
//  Command.hpp
//
//  Created by Carl Johan Gribel on 2024-10-14.
//  Copyright © 2024 Carl Johan Gribel. All rights reserved.
//

#ifndef Command_hpp
#define Command_hpp

#include <cstdint>
#include <memory>
#include <entt/fwd.hpp>
#include "config.h"
//#include "Context.hpp"

namespace eeng::editor 
{
    enum class CommandStatus : std::uint8_t
    {
        Done,
        InFlight,
        Failed
    };

    struct Selection
    {
        entt::entity entity;
    };

    // Is this a pure Editor thingy?
    class Command
    {
    public:
        virtual CommandStatus execute() = 0;

        virtual CommandStatus undo() = 0;

        virtual CommandStatus update() { return CommandStatus::Done; }

        virtual std::string get_name() const = 0;

        virtual ~Command() = default;
    };

    using CommandPtr = std::unique_ptr<Command>;

} // eeng::editor

#endif // Command_hpp
