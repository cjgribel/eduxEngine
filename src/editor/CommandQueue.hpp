//
//  Command.hpp
//
//  Created by Carl Johan Gribel on 2024-10-14.
//  Copyright © 2024 Carl Johan Gribel. All rights reserved.
//

#ifndef CommandQueue_hpp
#define CommandQueue_hpp

#include <optional>
#include <vector>
#include "Command.hpp"

namespace eeng::editor
{
    class CommandFactory {
    public:
        template <typename CommandType, typename... Args>
        static std::unique_ptr<Command> Create(Args&&... args) {
            return std::make_unique<CommandType>(std::forward<Args>(args)...);
        }
    };

    class CommandQueue
    {
        std::vector<std::unique_ptr<Command>> queue;

        int current_index = 0; // Index awaiting execution
        int latest_index = 0; //
        // bool has_new_ = false; // ???
        enum class InFlightAction : std::uint8_t { Execute, Undo };
        std::optional<size_t> in_flight_index;
        InFlightAction in_flight_action{};

    public:
        void add(CommandPtr&& command)
        {
            //discard_unexecuted(); // Might not work if multiple commands are added between executions

            // Remove current_index -> latest_index
            if (current_index < latest_index)
            {
                queue.erase(queue.begin() + current_index, queue.begin() + latest_index);
                latest_index = current_index;
            }

            queue.push_back(std::move(command));
            // has_new_ = true;
        }

        // ???
        // bool has_new() { return has_new_; }

        size_t size() { return queue.size(); }

        size_t get_current_index()
        {
            return current_index;
        }

        bool has_in_flight() const
        {
            return in_flight_index.has_value();
        }

        bool is_executed(size_t index)
        {
            return index < current_index;
        }

        std::string get_name(size_t index)
        {
            assert(index >= 0 && index < queue.size());
            return queue[index]->get_name();
        }

        int enqueued_count()
        {
            if (latest_index >= 0)
                return queue.size() - latest_index;
            return 0;
        }

        bool has_ready_commands()
        {
            if (has_in_flight())
                return false;
            return latest_index >= 0 && latest_index < queue.size();
        }

        bool has_next_command()
        {
            if (has_in_flight())
                return false;
            return current_index >= 0 && current_index < queue.size();
        }

        void discard_unexecuted()
        {
            if (!has_next_command()) return;
            queue.erase(queue.begin() + current_index, queue.end());
            latest_index = current_index;
        }

        void execute_next()
        {
            if (has_in_flight())
                return;
            if (!has_next_command())
                return;

            const auto result = queue[current_index]->execute();
            if (result == CommandStatus::InFlight)
            {
                in_flight_index = current_index;
                in_flight_action = InFlightAction::Execute;
                return;
            }

            current_index++;
            latest_index = std::max(current_index, latest_index);
        }

        void process()
        {
            while (true)
            {
                if (has_in_flight())
                {
                    const auto result = queue[*in_flight_index]->update();
                    if (result == CommandStatus::InFlight)
                        return;

                    if (in_flight_action == InFlightAction::Execute)
                    {
                        current_index++;
                        latest_index = std::max(current_index, latest_index);
                    }
                    else
                    {
                        if (current_index > 0)
                            current_index--;
                    }

                    in_flight_index.reset();
                    return;
                }

                if (!has_next_command())
                    return;

                execute_next();
                if (has_in_flight())
                    return;
            }
        }

        bool can_undo()
        {
            assert(current_index <= queue.size());
            if (has_in_flight())
                return false;
            return current_index > 0;
        }

        void undo_last()
        {
            if (has_in_flight())
                return;
            if (!can_undo())
                return;

            const size_t index = current_index - 1;
            const auto result = queue[index]->undo();
            if (result == CommandStatus::InFlight)
            {
                in_flight_index = index;
                in_flight_action = InFlightAction::Undo;
                return;
            }
            if (result == CommandStatus::Failed)
                return;
            current_index--;
        }

        void clear()
        {
            queue.clear();
            current_index = 0;
            latest_index = 0;
            in_flight_index.reset();
        }
    };

} // namespace Editor

#endif // Command_hpp
