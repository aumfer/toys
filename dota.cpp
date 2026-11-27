#include <engine.h>
engine::window window;
engine::gltfasset asset;
engine::gltfprogram program;
engine::gltfanimate animate;
engine::marbleprogram marble;
engine::fpscounter fps;
#ifndef __EMSCRIPTEN__
std::string get_sky() {
    if (GLAD_GL_OES_texture_compression_astc) return "https://umfer.us/galaxy/eso0932a_astc.pvr";
    if (GLAD_GL_OES_compressed_ETC1_RGB8_texture) return "https://umfer.us/galaxy/eso0932a.pvr";
    if (GLAD_GL_EXT_texture_compression_s3tc) return "https://umfer.us/galaxy/eso0932a.dds";
    return "https://cdn.eso.org/images/publicationjpg/eso0932a.jpg";
}
#else
std::string get_sky() {
    const char *e = (const char *)glGetString(GL_EXTENSIONS);
    if (strstr(e,"WEBGL_compressed_texture_astc")) return "https://umfer.us/galaxy/eso0932a_astc.pvr";
    if (strstr(e,"WEBGL_compressed_texture_etc")) return "https://umfer.us/galaxy/eso0932a_etc.pvr";
    if (strstr(e,"WEBGL_compressed_texture_etc1")) return "https://umfer.us/galaxy/eso0932a.pvr";
    if (strstr(e,"WEBGL_compressed_texture_s3tc")) return "https://umfer.us/galaxy/eso0932a.dds";
    return "https://cdn.eso.org/images/publicationjpg/eso0932a.jpg";
}
#endif
double start;
std::string sky;
void init() {
    start = glfwGetTime();
    sky = get_sky();
}
void run() {
    static bool _init;
    double mx,my;
    window(0,0,1920/2,1080/2, "gltf");
    if (!_init) { init(); _init = true; }
    glfwGetCursorPos(window.window,&mx,&my);
    //asset("TextureCoordinateTest.gltf","https://raw.githubusercontent.com/KhronosGroupArchives/glTF-Sample-Models/refs/heads/main/2.0/TextureCoordinateTest/glTF/");
    //asset("SimpleSkin.gltf","https://raw.githubusercontent.com/KhronosGroupArchives/glTF-Sample-Models/refs/heads/main/2.0/SimpleSkin/glTF/");
    //asset("RiggedFigure.gltf","https://raw.githubusercontent.com/KhronosGroupArchives/glTF-Sample-Models/refs/heads/main/2.0/RiggedFigure/glTF/");
    asset("antimage.gltf","file://C:/Users/antho/Desktop/antimage");
    //asset("abaddon.gltf","file://C:/Users/antho/Desktop/abaddon");
    static struct {
        HMM_Mat4 view = HMM_M4D(1.0);
        HMM_Mat4 projection = HMM_M4D(1.0);
        HMM_Vec3 eye = {0,3,5};
        HMM_Vec3 target = {0,3,0};
    } camera;
    float aspect = window.width/float(window.height);
    HMM_Mat4 model = HMM_MulM4(HMM_Translate({0,-0.5,0}),
        HMM_MulM4(
        HMM_MulM4(HMM_Rotate_RH(HMM_AngleDeg(90),{0,1,0}),HMM_Rotate_RH(HMM_AngleDeg(-90),{1,0,0})),
        HMM_Scale({0.5,0.5,0.5}))
    );
    HMM_Vec2 mousenorm = {float(mx/window.width),float(my/window.height)};
    mousenorm.Y = 1 - mousenorm.Y;
    mousenorm.X = mousenorm.X * 2 - 1;
    mousenorm.Y = mousenorm.Y * 2 - 1;
    camera.target.X = camera.eye.X+mousenorm.X;
    camera.target.Y = camera.eye.Y+mousenorm.Y;
    model = HMM_M4D(1.0);
    camera.view = HMM_LookAt_RH(camera.eye,camera.target,{0,1,0});
    if (glfwGetKey(window.window, GLFW_KEY_GRAVE_ACCENT)!=GLFW_PRESS) {
        float fov = HMM_AngleDeg(75.178177);
        camera.projection = HMM_Perspective_RH_NO(fov,aspect,0.01,10.0);
    } else {
        camera.projection = HMM_Orthographic_RH_NO(-aspect/2,+aspect/2,-1,+1,-10,+10);
    }
    glClear(GL_COLOR_BUFFER_BIT);
    marble(0,0,window.width,window.height,&camera.view.Elements[0][0],&camera.projection.Elements[0][0],sky.c_str());
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
    animate(asset,"die",0.0);
    program(0,0,window.width,window.height,asset,&camera.view.Elements[0][0],&camera.projection.Elements[0][0],&model.Elements[0][0]);
    glDisable(GL_DEPTH_TEST);
    fps();
    fps.draw(0,0,window.width,window.height);
}
int main() {
#ifndef __EMSCRIPTEN__
    do {
        run();
#if _WIN32
#if _DEBUG
        Sleep(1);
#else
        Sleep(0);
#endif
#endif
        //return -1;
    } while(!glfwWindowShouldClose(window.window));
#else
    emscripten_set_main_loop(run, 0, 1);
#endif
return 0;
}