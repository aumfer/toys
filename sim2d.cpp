#include <engine.h>

engine::window window;
template<int SIM_WIDTH=32,int SIM_HEIGHT=32>
struct sim2dprogram {
    static inline rgba8_t control = RGBA(100,200,0,128);
    engine::draw_box draw_box;
    engine::framebuffer framebuffer[2];
    engine::program program;
    engine::uniform2f inv_physicssize_uniform;
    engine::uniform1i physics_uniform;
    unsigned frame;
    inline void operator()(int x, int y, int w, int h) {
        unsigned f0,f1;
        f0 = frame&1;
        f1 = (frame+1)&1;
        program({_TEXT(
            uniform vec2 inv_physicssize;
            attribute vec2 position;
            varying vec2 uv;
            varying vec2 uv0;
            varying vec2 uv1;
            varying vec2 uv2;
            varying vec2 uv3;
            void main() {
                uv = position*0.5+0.5;
                uv0 = uv + vec2(-inv_physicssize.x,0.0);
                uv1 = uv + vec2(0.0,-inv_physicssize.y);
                uv2 = uv + vec2(inv_physicssize.x,0.0);
                uv3 = uv + vec2(0.0,inv_physicssize.y);
                gl_Position = vec4(position, 0.0, 1.0);
            }
        )},{_TEXT(
            uniform sampler2D physics;
            varying vec2 uv;
            varying vec2 uv0;
            varying vec2 uv1;
            varying vec2 uv2;
            varying vec2 uv3;
            void main() {
                const float EPS = 1.0/256.0;
                vec4 s10 = texture2D(physics,uv0)*vec4(2.0)-vec4(1.0);
                vec4 s11 = texture2D(physics,uv1)*vec4(2.0)-vec4(1.0);
                vec4 s12 = texture2D(physics,uv2)*vec4(2.0)-vec4(1.0);
                vec4 s13 = texture2D(physics,uv3)*vec4(2.0)-vec4(1.0);
                //vec4 s20 = texture2D(texture2,uv0)*vec4(2.0)-vec4(1.0);
                //vec4 s21 = texture2D(texture2,uv1)*vec4(2.0)-vec4(1.0);
                //vec4 s22 = texture2D(texture2,uv2)*vec4(2.0)-vec4(1.0);
                //vec4 s23 = texture2D(texture2,uv3)*vec4(2.0)-vec4(1.0);
                vec4 s1 = texture2D(physics,uv)*vec4(2.0)-vec4(1.0);
                //vec4 s2 = texture2D(texture2,uv)*vec4(2.0)-vec4(1.0);
                vec4 r=vec4(0.5);//=s1*vec4(0.5)+vec4(0.5);
                if (s10.x>EPS&&abs(s10.x)>abs(s10.y)) {
                    s10.x-=EPS;
                    r=s10*vec4(0.5)+vec4(0.5);
                }
                if (s11.y>EPS&&abs(s11.y)>abs(s11.x)) {
                    s11.y-=EPS;
                    r=s11*vec4(0.5)+vec4(0.5);
                }
                if (s12.x<-EPS&&abs(s12.x)>abs(s12.y)) {
                    s12.x+=EPS;
                    r=s12*vec4(0.5)+vec4(0.5);
                }
                if (s13.y<-EPS&&abs(s13.y)>abs(s13.x)) {
                    s13.y+=EPS;
                    r=s13*vec4(0.5)+vec4(0.5);
                }
                //gl_FragColor=vec4(uv,0.0,1.0);
                gl_FragColor=r;
            }
        )});
        framebuffer[0](SIM_WIDTH,SIM_HEIGHT);
        framebuffer[1](SIM_WIDTH,SIM_HEIGHT);
        framebuffer[f0].upload(&control.c,SIM_WIDTH/2,SIM_HEIGHT/2,1,1);
        inv_physicssize_uniform("inv_physicssize",program,1.0/SIM_WIDTH,1.0/SIM_HEIGHT);
        physics_uniform("physics",program,0);
        glBindTexture(GL_TEXTURE_2D,framebuffer[f0]._texture);
        glBindFramebuffer(GL_FRAMEBUFFER,framebuffer[f1]);
        draw_box(0,0,SIM_WIDTH,SIM_HEIGHT,program);
        glBindFramebuffer(GL_FRAMEBUFFER,0);
        draw_box(x,y,w,h,program);
        ++frame;
    }
};
sim2dprogram sim;
void run() {
    window(0,0,1920/2,1080/2,"sim2d");
    sim.control = RGBA(128,128,255,255);
    if (glfwGetKey(window,GLFW_KEY_LEFT)==GLFW_PRESS) {
        sim.control.r=0;
    }
    if (glfwGetKey(window,GLFW_KEY_RIGHT)==GLFW_PRESS) {
        sim.control.r=255;
    }
    if (glfwGetKey(window,GLFW_KEY_UP)==GLFW_PRESS) {
        sim.control.g=255;
    }
    if (glfwGetKey(window,GLFW_KEY_DOWN)==GLFW_PRESS) {
        sim.control.g=0;
    }
    sim(0,0,window.width,window.height);
}
int main() {
#ifndef __EMSCRIPTEN__
    do {
        run();
#if _WIN32
        //Sleep(333);
#if _DEBUG
        Sleep(1);
#else
        Sleep(0);
#endif
#else
#endif
        //return -1;
    } while(!glfwWindowShouldClose(window.window));
#else
    emscripten_set_main_loop(run, 0, 1);
#endif
return 0;
}