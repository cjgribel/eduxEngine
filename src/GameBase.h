// Created by Carl Johan Gribel.
// Licensed under the MIT License. See LICENSE file for details.

#ifndef GameBase_h
#define GameBase_h
#pragma once

namespace eeng {

/**
 * @brief Abstract base class for game implementations.
 *
 * This interface defines the essential functions that any game should implement.
 * Derived classes are expected to provide concrete implementations for initializing,
 * updating, rendering, and cleaning up game resources.
 */
class GameBase
{
public:
    /**
     * @brief Initialize the game.
     *
     * This function should initialize all necessary game components.
     *
     * @return true if initialization is successful, false otherwise.
     */
    virtual bool init() = 0;

    /**
     * @brief Update the game state.
     *
     * Processes game logic and user input.
     *
     * @param time_s The current time in seconds.
     * @param deltaTime_s The time elapsed in seconds since the last update.
     * @param input Pointer to the input manager handling user input.
     */
    virtual void update(
        float time_s,
        float deltaTime_s) = 0;

    /**
     * @brief Render the game game.
     *
     * Renders the game visuals on the screen.
     *
     * @param time_s The current time in seconds.
     * @param screenWidth The width of the screen in pixels.
     * @param screenHeight The height of the screen in pixels.
     */
    virtual void render(
        float time_s,
        int windowWidth,
        int windowHeight) = 0;

    /**
     * @brief Clean up game resources.
     *
     * Releases any resources allocated during the game's lifetime.
     */
    virtual void destroy() = 0;

    /**
     * @brief Virtual destructor.
     */
    virtual ~GameBase() noexcept = default;
};

} // namespace eeng

#endif