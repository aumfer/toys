#include <engine.h>
engine::window window;
engine::spriteprogram _sprite;
engine::terrariumprogram terrarium;
engine::marbleprogram marble;
engine::videotexture cam= {0};
engine::fpscounter fps;
struct {
    HMM_Mat4 view = HMM_M4D(1.0);
    HMM_Vec3 eye = {0};
    HMM_Vec3 target = {1,0,0};
    HMM_Vec3 up = {0,0,1};
    HMM_Mat4 projection = HMM_M4D(1.0);
} camera;
void init(){
}
static inline HMM_Vec3 ECEFPosition(HMM_Vec2 llnorm) {
    static const HMM_Vec3 WGS84 = {6.378137,6.378137,6.356752314245};
    float lat = llnorm.X * (HMM_PI*0.5);
    float lng = llnorm.Y * (HMM_PI);
    float a = WGS84.Elements[1];
    float b = WGS84.Elements[2];
    float e2 = 1 - (b*b)/(a*a);
    float n = a / HMM_SqrtF(1-e2*(sinf(lat)*sinf(lat)));
    float h = 0;//
    HMM_Vec3 ecef;
    ecef.X = (n+h)*cosf(lat)*cosf(lng);
    ecef.Y = (n+h)*cosf(lat)*sinf(lng);
    ecef.Z = ((1-e2)*n+h)*sinf(lat);
    return ecef;
}
void run() {
    static unsigned frame;
    double mx,my;
    window(0,0,1920/2,1080/2,"terrarium");
    glEnable(GL_CULL_FACE);
    glfwGetCursorPos(window.window,&mx,&my);
    HMM_Vec2 llnorm;
    llnorm.Y = mx/window.width;
    llnorm.Y = 1 - llnorm.Y;
    llnorm.Y = llnorm.Y * 2 - 1;
    llnorm.X = my/window.height;
    llnorm.X = 1 - llnorm.X;
    llnorm.X = llnorm.X * 2 - 1;
    camera.target = ECEFPosition(llnorm);
    camera.view = HMM_LookAt_RH(camera.eye,camera.target,camera.up);
    if (glfwGetKey(window.window, GLFW_KEY_GRAVE_ACCENT)!=GLFW_PRESS) {
        float aspect = window.width/float(window.height);
        //aspect=1;
        float fov = HMM_AngleDeg(115*(1080.0/1920.0));
        //fov=0.25;
        camera.projection = HMM_Perspective_RH_NO(fov,aspect,0.001,10.0);
    } else {
        camera.projection = HMM_Orthographic_RH_NO(-1,+1,-1,+1,+1,-1);
    }
    terrarium(0,0,window.width,window.height,&camera.view.Elements[0][0],&camera.projection.Elements[0][0]);
    cam();
    _sprite(0,0,cam.width/3,cam.height/3,cam);//,0,&camera.view.Elements[0][0],&camera.projection.Elements[0][0]);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
    fps();
    fps.draw(0,0,window.width,window.height);
    ++frame;
}
int main() {
    init();
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