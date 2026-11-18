#pragma once
#include <SDL3/SDL.h>
namespace input {
// Main-thread calls from the SDL window/event loop.
void handle_event(const SDL_Event& event);
void update();
void set_prompt_window(SDL_Window* window);
// Before SDL's gamepad subsystem starts (SDL_Init): SDL's own Steam Controller driver on (SDL turns it off
// by default), so a Steam Controller without Steam running is a gamepad and not only a mouse and keyboard.
// Normal priority: SDL_JOYSTICK_HIDAPI_STEAM in the environment still decides, and a game Steam starts gets
// SDL_JOYSTICK_HIDAPI=0 and the physical controllers in SDL_GAMECONTROLLER_IGNORE_DEVICES (Steam Input's pad).
void set_controller_hints();
// Android: a touch device that belongs to a connected controller (GameSir and others register a
// virtual touch screen for their buttons); its touches are not the player's fingers
bool touch_from_controller(SDL_TouchID id);
}
