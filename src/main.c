#include <stdio.h>
#include <webgpu/webgpu.h>
#include <SDL3/SDL.h>
#include "sdl3webgpu.h"

int main(void) {
  printf("Hello wgpu-native !\n");

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "Failed to initialize SDL: %s\n", SDL_GetError());
    return -1;
  }

  return 0;
}