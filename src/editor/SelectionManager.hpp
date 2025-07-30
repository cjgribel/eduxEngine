#include <deque>
#include <algorithm>
#include <iterator>

#ifndef SelectionManager_h
#define SelectionManager_h

namespace Editor {

    template <typename T>
    class SelectionManager
    {
    private:
        std::deque<T> items; // Stores selected items in order

    public:
        // Add an item to the selection
        void add(const T& item)
        {
            // Remove if already exists, then add to the end
            remove(item);
            items.push_back(item);
        }

        // Remove an item from the selection
        void remove(const T& item)
        {
            items.erase(
                std::remove(items.begin(), items.end(), item),
                items.end()
            );
        }

        // Check if an item is selected
        bool contains(const T& item) const
        {
            return std::find(items.begin(), items.end(), item) != items.end();
        }

        // Clear all selections
        void clear()
        {
            items.clear();
        }

        // Get the last selected item
        const T& first() const
        {
            return items.front();
        }

        // Get the last selected item
        const T& last() const
        {
            return items.back();
        }

        // Indexation
        T& at(size_t index)
        {
            return items.at(index);
        }

        // Indexation
        const T& at(size_t index) const
        {
            return items.at(index);
        }

        // Get all items except the last one
        std::deque<T> all_except_last() const
        {
            if (items.empty()) return {};
            return std::deque<T>(items.begin(), std::prev(items.end()));
        }

        // Iterate through selected items
        const std::deque<T>& get_all() const
        {
            return items;
        }

        template<class F>
        void assert_valid(const F&& is_valid) const
        {
            for (auto& item : items)
            {
                assert(is_valid(item));
            }
        }

        template<class F>
        bool remove_invalid(const F&& is_valid)
        {
            auto items_temp = items;
            items.clear();
            for (auto& item : items_temp)
            {
                if (is_valid(item)) add(item);
            }
        }

        // Get number of selected items
        size_t size() const
        {
            return items.size();
        }

        // Check if selection is empty
        bool empty() const
        {
            return items.empty();
        }
    };

} // namespace Editor
#endif