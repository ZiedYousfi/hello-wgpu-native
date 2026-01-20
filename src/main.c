#include <stdio.h>
#include <webgpu/webgpu.h>
#include <SDL3/SDL.h>
#include "sdl3webgpu.h"

int main(void) {
  printf("Hello wgpu-native !\n");

  // Init WebGPU
  WGPUInstance instance = wgpuCreateInstance(NULL);

  // Init SDL
  SDL_Init(SDL_INIT_VIDEO);
  SDL_Window *window = SDL_CreateWindow("Hello wgpu-native !", 640, 480, 0);

  // Here we create our WebGPU surface from the window!
  WGPUSurface surface = SDL_GetWGPUSurface(instance, window);

  // Wait for close
  SDL_Event event;
  int running = 1;

  while (running) {
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) {
        running = 0;
      }
    }

    SDL_Delay(16); // Simulate ~60fps
  }
  // Terminate SDL
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}