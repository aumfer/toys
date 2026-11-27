#include <engine.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>
engine::window::Init engine::window::init;
engine::textureimage::Init engine::textureimage::init;
engine::terrariumprogram::Init engine::terrariumprogram::init;
engine::gltfbuffer::Init engine::gltfbuffer::init;
engine::gltfbufferview::Init engine::gltfbufferview::init;
engine::gltftexture::Init engine::gltftexture::init;
engine::gltfmaterial::Init engine::gltfmaterial::init;
engine::gltfattribute::Init engine::gltfattribute::init;
engine::gltfprimitivetarget::Init engine::gltfprimitivetarget::init;
engine::gltfprimitive::Init engine::gltfprimitive::init;
engine::gltfmeshweight::Init engine::gltfmeshweight::init;
engine::gltfmesh::Init engine::gltfmesh::init;
engine::gltfjointref::Init engine::gltfjointref::init;
engine::gltfskin::Init engine::gltfskin::init;
engine::gltfnoderef::Init engine::gltfnoderef::init;
engine::gltfnode::Init engine::gltfnode::init;
engine::gltfanimationsampler::Init engine::gltfanimationsampler::init;
engine::gltfanimationchannel::Init engine::gltfanimationchannel::init;
engine::gltfscene::Init engine::gltfscene::init;
engine::gltfasset::Init engine::gltfasset::init;

#ifdef __EMSCRIPTEN__
EM_JS(void, em_glfwGetFramebufferSize, (GLFWwindow* window, int* width, int* height), {
  // Check for NULL pointers (0 in WASM linear memory) before writing
  if (width) {
    HEAP32[width >> 2] = Module.canvas.width;
  }
  if (height) {
    HEAP32[height >> 2] = Module.canvas.height;
  }
});
#endif