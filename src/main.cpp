#include "core/Game.h"
#include <SDL_log.h>

/**
 * @brief Entry point for the Space RTS game
 * 
 * This is now a clean, professional entry point that delegates
 * all game logic to the Game class using proper RAII principles.
 */
int main(int argc, char* argv[]) {
    (void)argc; // Suppress unused parameter warning
    (void)argv; // Suppress unused parameter warning
    
    SDL_Log("=== Space RTS - Professional Edition ===");
    SDL_Log("Initializing game engine...");
    
    // Create game instance
    Core::Game game;
    
    // Initialize the game
    if (!game.Initialize()) {
        SDL_Log("Failed to initialize game engine!");
        return -1;
    }
    
    SDL_Log("Game engine initialized successfully!");
    SDL_Log("Starting main game loop...");
    
    // Run the game
    game.Run();
    
    SDL_Log("Game loop ended. Shutting down...");
    
    // Game destructor will handle cleanup automatically (RAII)
    return 0;
}
