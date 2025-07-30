//
//  Command.hpp
//
//  Created by Carl Johan Gribel on 2024-10-14.
//  Copyright © 2024 Carl Johan Gribel. All rights reserved.
//

#ifndef CommandQueue_hpp
#define CommandQueue_hpp

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

    public:
        void add(CommandPtr&& command)
        {
            //remove_pending(); // Might not work if multiple commands are added between executions

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

        bool is_executed(size_t index)
        {
            return index < current_index;
        }

        std::string get_name(size_t index)
        {
            assert(index >= 0 && index < queue.size());
            return queue[index]->get_name();
        }

        int new_commands_pending()
        {
            if (latest_index >= 0)
                return queue.size() - latest_index;
            return 0;
        }

        bool has_new_commands_pending()
        {
            return latest_index >= 0 && latest_index < queue.size();
        }

        bool commands_pending()
        {
            return current_index >= 0 && current_index < queue.size();
        }

        void remove_pending()
        {
            if (!commands_pending()) return;
            queue.erase(queue.begin() + current_index, queue.end());
            latest_index = current_index;
        }

        void execute_next()
        {
            if (!commands_pending()) return;
            queue[current_index++]->execute();
            latest_index = std::max(current_index, latest_index);
        }

        void execute_pending()
        {
            while (commands_pending()) execute_next();
        }

        bool can_undo()
        {
            assert(current_index <= queue.size());
            return current_index > 0;
        }

        void undo_last()
        {
            if (!can_undo()) return;
            queue[--current_index]->undo();
        }

        void clear()
        {
            queue.clear();
            current_index = 0;
            latest_index = 0;
        }
    };

} // namespace Editor

#endif // Command_hpp
