#include <engine.h>
engine::window window;
engine::gltfasset asset;
engine::gltfprogram program;
engine::gltfanimate animate;
engine::marbleprogram marble;
engine::fpscounter fps;
double start;
void run() {
    double mx,my;
    window(0,0,1920/2,1080/2, "gltf");
    if (!start) start = glfwGetTime();
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
        //aspect=1;
        float fov = HMM_AngleDeg(75.178177);
        //fov=0.25;
        camera.projection = HMM_Perspective_RH_NO(fov,aspect,0.01,10.0);
    } else {
        camera.projection = HMM_Orthographic_RH_NO(-aspect/2,+aspect/2,-1,+1,-10,+10);
    }
    glClear(GL_COLOR_BUFFER_BIT);
    //const char *sky = "https://cdn.eso.org/images/publicationjpg/eso0932a.jpg";
    //const char *sky = "https://cdn.eso.org/images/large/eso0932a.jpg";
    //const char *sky = "file://D:/dev/umfer-us/eso0932a.dds"; //bc1
    //const char *sky = "file://D:/dev/umfer-us/eso0932a.pvr"; //ect1
    const char *sky = "file://D:/dev/umfer-us/eso0932a_astc.pvr"; 
    marble(0,0,window.width,window.height,&camera.view.Elements[0][0],&camera.projection.Elements[0][0],sky);
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