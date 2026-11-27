#include <engine.h>
engine::window window;
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
std::string sky;
struct {
    HMM_Mat4 view = HMM_M4D(1.0);
    HMM_Vec3 eye = {0};
    HMM_Vec3 target = {1,0,0};
    HMM_Vec3 up = {0,0,1};
    HMM_Mat4 projection = HMM_M4D(1.0);
} camera;
static inline HMM_Vec3 SpherePosition(HMM_Vec2 llnorm) {
    float lat = llnorm.X * (HMM_PI*0.5);
    float lng = llnorm.Y * (HMM_PI);

    HMM_Vec3 p;
    p.X = cosf(lat) * cosf(lng);
    p.Y = cosf(lat) * sinf(lng);
    p.Z = sinf(lat);
    return p;
}
void init() {
#if _DEBUG
    puts((const char *)glGetString(GL_EXTENSIONS));
#endif
    sky = get_sky();
}
void run() {
    static bool _init;
    static unsigned frame;
    double mx,my;
    window(0,0,1920/2,1080/2,"galaxy");
    glfwGetCursorPos(window.window,&mx,&my);
    if (!_init) { init(); _init = true; }
    HMM_Vec2 llnorm;
    llnorm.Y = mx/window.width;
    llnorm.Y = 1 - llnorm.Y;
    llnorm.Y = llnorm.Y * 2 - 1;
    llnorm.X = my/window.height;
    llnorm.X = 1 - llnorm.X;
    llnorm.X = llnorm.X * 2 - 1;
    camera.target = SpherePosition(llnorm);
    camera.view = HMM_LookAt_RH(camera.eye,camera.target,camera.up);
    float aspect = window.width/float(window.height);
    float fov = HMM_AngleDeg(75.178177);
    camera.projection = HMM_Perspective_RH_NO(fov,aspect,0.001,10.0);
    if (glfwGetKey(window.window, GLFW_KEY_GRAVE_ACCENT)==GLFW_PRESS) {
        marble.gridColor.A = 1.0;
    } else {
        marble.gridColor.A = 0.0;
    }
    marble(0,0,window.width,window.height,&camera.view.Elements[0][0],&camera.projection.Elements[0][0],sky.c_str());
    fps();
    if (glfwGetKey(window.window, GLFW_KEY_GRAVE_ACCENT)==GLFW_PRESS) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
        fps.draw(0,0,window.width,window.height);
    }
    ++frame;
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