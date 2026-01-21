#include "sdl3webgpu.h"
#include "webgpu.h"
#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <webgpu/webgpu.h>

// Helper to build a WGPUStringView from a C string.
static WGPUStringView wgpu_string_view(const char *text) {
  return (WGPUStringView){.data = text, .length = text ? strlen(text) : 0};
}

typedef struct {
  WGPUAdapter adapter;
  WGPURequestAdapterStatus status;
  bool completed;
} AdapterRequest;

static void on_adapter_request(WGPURequestAdapterStatus status,
                               WGPUAdapter adapter, WGPUStringView message,
                               void *userdata1, void *userdata2) {
  (void)message;
  (void)userdata2;
  AdapterRequest *request = (AdapterRequest *)userdata1;
  request->status = status;
  request->adapter = adapter;
  request->completed = true;
}

typedef struct {
  WGPUDevice device;
  WGPURequestDeviceStatus status;
  bool completed;
} DeviceRequest;

static void on_device_request(WGPURequestDeviceStatus status, WGPUDevice device,
                              WGPUStringView message, void *userdata1,
                              void *userdata2) {
  (void)message;
  (void)userdata2;
  DeviceRequest *request = (DeviceRequest *)userdata1;
  request->status = status;
  request->device = device;
  request->completed = true;
}

// Select a surface format we can render to (prefer sRGB when available).
static WGPUTextureFormat choose_surface_format(WGPUSurface surface,
                                               WGPUAdapter adapter) {
  WGPUSurfaceCapabilities caps = {0};
  if (wgpuSurfaceGetCapabilities(surface, adapter, &caps) !=
          WGPUStatus_Success ||
      caps.formatCount == 0) {
    return WGPUTextureFormat_BGRA8Unorm;
  }

  WGPUTextureFormat preferred = caps.formats[0];
  for (size_t i = 0; i < caps.formatCount; ++i) {
    if (caps.formats[i] == WGPUTextureFormat_BGRA8UnormSrgb) {
      preferred = caps.formats[i];
      break;
    }
  }

  wgpuSurfaceCapabilitiesFreeMembers(caps);
  return preferred;
}

static WGPUCompositeAlphaMode choose_alpha_mode(WGPUSurface surface,
                                                WGPUAdapter adapter) {
  WGPUSurfaceCapabilities caps = {0};
  if (wgpuSurfaceGetCapabilities(surface, adapter, &caps) !=
          WGPUStatus_Success ||
      caps.alphaModeCount == 0) {
    return WGPUCompositeAlphaMode_Opaque;
  }

  WGPUCompositeAlphaMode preferred = caps.alphaModes[0];
  wgpuSurfaceCapabilitiesFreeMembers(caps);
  return preferred;
}

static WGPUPresentMode choose_present_mode(WGPUSurface surface,
                                           WGPUAdapter adapter) {
  WGPUSurfaceCapabilities caps = {0};
  if (wgpuSurfaceGetCapabilities(surface, adapter, &caps) !=
          WGPUStatus_Success ||
      caps.presentModeCount == 0) {
    return WGPUPresentMode_Fifo;
  }

  WGPUPresentMode preferred = caps.presentModes[0];
  for (size_t i = 0; i < caps.presentModeCount; ++i) {
    if (caps.presentModes[i] == WGPUPresentMode_Fifo) {
      preferred = caps.presentModes[i];
      break;
    }
  }

  wgpuSurfaceCapabilitiesFreeMembers(caps);
  return preferred;
}

static void configure_surface(WGPUSurface surface, WGPUDevice device,
                              WGPUTextureFormat format,
                              WGPUCompositeAlphaMode alphaMode,
                              WGPUPresentMode presentMode, uint32_t width,
                              uint32_t height) {
  WGPUSurfaceConfiguration config = {0};
  config.device = device;
  config.format = format;
  config.usage = WGPUTextureUsage_RenderAttachment;
  config.width = width;
  config.height = height;
  config.viewFormatCount = 0;
  config.viewFormats = NULL;
  config.alphaMode = alphaMode;
  config.presentMode = presentMode;
  wgpuSurfaceConfigure(surface, &config);
}

int main(void) {
  printf("Hello wgpu-native !\n");

  // Init WebGPU instance.
  WGPUInstanceDescriptor instanceDesc = {0};
  WGPUInstance instance = wgpuCreateInstance(&instanceDesc);

  // Init SDL (windowing).
  SDL_Init(SDL_INIT_VIDEO);
  SDL_Window *window = SDL_CreateWindow("Hello wgpu-native !", 640, 480, 0);

  // Create our WebGPU surface from the SDL window.
  WGPUSurface surface = SDL_GetWGPUSurface(instance, window);

  // Request a compatible adapter.
  AdapterRequest adapterRequest = {0};
  WGPURequestAdapterOptions adapterOptions = {
      .featureLevel = WGPUFeatureLevel_Core,
      .powerPreference = WGPUPowerPreference_HighPerformance,
      .forceFallbackAdapter = false,
      .backendType = WGPUBackendType_Undefined,
      .compatibleSurface = surface,
  };
  WGPURequestAdapterCallbackInfo adapterCallbackInfo = {
      .mode = WGPUCallbackMode_AllowProcessEvents,
      .callback = on_adapter_request,
      .userdata1 = &adapterRequest,
      .userdata2 = NULL,
  };
  wgpuInstanceRequestAdapter(instance, &adapterOptions, adapterCallbackInfo);
  while (!adapterRequest.completed) {
    wgpuInstanceProcessEvents(instance);
    SDL_Delay(1);
  }
  if (adapterRequest.status != WGPURequestAdapterStatus_Success) {
    fprintf(stderr, "Failed to acquire a WebGPU adapter.\n");
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  // Request a device from the adapter.
  DeviceRequest deviceRequest = {0};
  WGPUDeviceDescriptor deviceDesc = {0};
  deviceDesc.label = wgpu_string_view("hello-wgpu device");
  deviceDesc.defaultQueue.label = wgpu_string_view("hello-wgpu queue");

  WGPURequestDeviceCallbackInfo deviceCallbackInfo = {
      .mode = WGPUCallbackMode_AllowProcessEvents,
      .callback = on_device_request,
      .userdata1 = &deviceRequest,
      .userdata2 = NULL,
  };

  wgpuAdapterRequestDevice(adapterRequest.adapter, &deviceDesc,
                           deviceCallbackInfo);
  while (!deviceRequest.completed) {
    wgpuInstanceProcessEvents(instance);
    SDL_Delay(1);
  }
  if (deviceRequest.status != WGPURequestDeviceStatus_Success) {
    fprintf(stderr, "Failed to acquire a WebGPU device.\n");
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  WGPUDevice device = deviceRequest.device;
  WGPUQueue queue = wgpuDeviceGetQueue(device);

  // Choose surface configuration based on capabilities.
  WGPUTextureFormat surfaceFormat =
      choose_surface_format(surface, adapterRequest.adapter);
  WGPUCompositeAlphaMode alphaMode =
      choose_alpha_mode(surface, adapterRequest.adapter);
  WGPUPresentMode presentMode =
      choose_present_mode(surface, adapterRequest.adapter);

  int windowWidth = 0;
  int windowHeight = 0;
  SDL_GetWindowSize(window, &windowWidth, &windowHeight);
  configure_surface(surface, device, surfaceFormat, alphaMode, presentMode,
                    (uint32_t)windowWidth, (uint32_t)windowHeight);

  const char *shaderSource =
      "@vertex\n"
      "fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> "
      "@builtin(position) vec4f {\n"
      "    var p = vec2f(0.0, 0.0);\n"
      "    if (in_vertex_index == 0u) {\n"
      "        p = vec2f(-0.5, -0.5);\n"
      "    } else if (in_vertex_index == 1u) {\n"
      "        p = vec2f(0.5, -0.5);\n"
      "    } else {\n"
      "        p = vec2f(0.0, 0.5);\n"
      "    }\n"
      "    return vec4f(p, 0.0, 1.0);\n"
      "}\n"
      "\n"
      "@fragment\n"
      "fn fs_main() -> @location(0) vec4f {\n"
      "    return vec4f(0.0, 0.4, 1.0, 1.0);\n"
      "}\n";

  // Create a WGSL shader module.
  WGPUShaderModuleDescriptor shaderDesc = {0};
  shaderDesc.label = wgpu_string_view("triangle shader");

  WGPUShaderSourceWGSL shaderCodeDesc = {
      .chain = {.sType = WGPUSType_ShaderSourceWGSL, .next = NULL},
      .code = wgpu_string_view(shaderSource),
  };

  shaderDesc.nextInChain = &shaderCodeDesc.chain;

  WGPUShaderModule shaderModule =
      wgpuDeviceCreateShaderModule(device, &shaderDesc);

  WGPURenderPipelineDescriptor pipelineDesc = {0};
  pipelineDesc.nextInChain = NULL;

  pipelineDesc.vertex.bufferCount = 0;
  pipelineDesc.vertex.buffers = NULL;
  pipelineDesc.vertex.module = shaderModule;
  pipelineDesc.vertex.entryPoint = wgpu_string_view("vs_main");
  pipelineDesc.vertex.constantCount = 0;
  pipelineDesc.vertex.constants = NULL;

  // Each sequence of 3 vertices is considered as a triangle
  pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
  pipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;

  // The face orientation is defined by assuming that when looking
  // from the front of the face, its corner vertices are enumerated
  // in the counter-clockwise (CCW) order.
  pipelineDesc.primitive.frontFace = WGPUFrontFace_CCW;

  // But the face orientation does not matter much because we do not
  // cull (i.e. "hide") the faces pointing away from us (which is often
  // used for optimization).
  pipelineDesc.primitive.cullMode = WGPUCullMode_None;

  // We tell that the programmable fragment shader stage is described
  // by the function called 'fs_main' in the shader module.
  WGPUFragmentState fragmentState = {0};
  fragmentState.module = shaderModule;
  fragmentState.entryPoint = wgpu_string_view("fs_main");
  fragmentState.constantCount = 0;
  fragmentState.constants = NULL;
  pipelineDesc.fragment = &fragmentState;

  pipelineDesc.depthStencil = NULL; // No depth/stencil buffer for now

  WGPUBlendState blendState = {0};
  // [...] Configure color blending equation
  // Configure alpha blending equation
  blendState.alpha.srcFactor = WGPUBlendFactor_One;
  blendState.alpha.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
  blendState.alpha.operation = WGPUBlendOperation_Add;

  WGPUColorTargetState colorTarget = {0};
  colorTarget.format = surfaceFormat;
  colorTarget.blend = &blendState;
  colorTarget.writeMask = WGPUColorWriteMask_All; // We could write to only some
                                                  // of the color channels.

  blendState.color.srcFactor = WGPUBlendFactor_SrcAlpha;
  blendState.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
  blendState.color.operation = WGPUBlendOperation_Add;

  // Samples per pixel
  pipelineDesc.multisample.count = 1;
  // Default value for the mask, meaning "all bits on"
  pipelineDesc.multisample.mask = ~0u;
  // Default value as well (irrelevant for count = 1 anyways)
  pipelineDesc.multisample.alphaToCoverageEnabled = false;

  // We have only one target because our render pass has only one output color
  // attachment.
  fragmentState.targetCount = 1;
  fragmentState.targets = &colorTarget;

  WGPURenderPipeline pipeline =
      wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);

  // Wait for close
  SDL_Event event;
  int running = 1;

  while (running) {
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) {
        running = 0;
      } else if (event.type == SDL_EVENT_WINDOW_RESIZED ||
                 event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        SDL_GetWindowSize(window, &windowWidth, &windowHeight);
        if (windowWidth > 0 && windowHeight > 0) {
          configure_surface(surface, device, surfaceFormat, alphaMode,
                            presentMode, (uint32_t)windowWidth,
                            (uint32_t)windowHeight);
        }
      }
    }

    // Acquire the next swapchain texture.
    WGPUSurfaceTexture surfaceTexture = {0};
    wgpuSurfaceGetCurrentTexture(surface, &surfaceTexture);
    if (surfaceTexture.status !=
            WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
        surfaceTexture.status !=
            WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
      // Surface became invalid (resize/outdated) – reconfigure and try next
      // frame.
      SDL_Delay(16);
      continue;
    }

    // Create a view for the render pass.
    WGPUTextureView backbufferView =
        wgpuTextureCreateView(surfaceTexture.texture, NULL);

    // Command encoder for this frame.
    WGPUCommandEncoderDescriptor encoderDesc = {0};
    encoderDesc.label = wgpu_string_view("frame encoder");
    WGPUCommandEncoder encoder =
        wgpuDeviceCreateCommandEncoder(device, &encoderDesc);

    // Render pass setup.
    WGPURenderPassColorAttachment colorAttachment = {0};
    colorAttachment.view = backbufferView;
    colorAttachment.resolveTarget = NULL;
    colorAttachment.loadOp = WGPULoadOp_Clear;
    colorAttachment.storeOp = WGPUStoreOp_Store;
    colorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    colorAttachment.clearValue = (WGPUColor){0.05, 0.05, 0.08, 1.0};

    WGPURenderPassDescriptor renderPassDesc = {0};
    renderPassDesc.label = wgpu_string_view("main render pass");
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &colorAttachment;

    WGPURenderPassEncoder renderPass =
        wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);

    // Select which render pipeline to use.
    wgpuRenderPassEncoderSetPipeline(renderPass, pipeline);
    // Draw 1 instance of a 3-vertices shape.
    wgpuRenderPassEncoderDraw(renderPass, 3, 1, 0, 0);
    wgpuRenderPassEncoderEnd(renderPass);

    // Submit and present.
    WGPUCommandBufferDescriptor commandBufferDesc = {0};
    commandBufferDesc.label = wgpu_string_view("frame command buffer");
    WGPUCommandBuffer commandBuffer =
        wgpuCommandEncoderFinish(encoder, &commandBufferDesc);
    wgpuQueueSubmit(queue, 1, &commandBuffer);
    wgpuSurfacePresent(surface);

    // Cleanup per-frame objects.
    wgpuCommandBufferRelease(commandBuffer);
    wgpuRenderPassEncoderRelease(renderPass);
    wgpuCommandEncoderRelease(encoder);
    wgpuTextureViewRelease(backbufferView);
    wgpuTextureRelease(surfaceTexture.texture);

    SDL_Delay(16); // Simulate ~60fps
  }
  // Cleanup GPU objects.
  wgpuRenderPipelineRelease(pipeline);
  wgpuShaderModuleRelease(shaderModule);
  wgpuDeviceRelease(device);
  wgpuAdapterRelease(adapterRequest.adapter);
  wgpuSurfaceRelease(surface);
  wgpuInstanceRelease(instance);
  // Terminate SDL
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}