#include <engine.h>

#define MATRIX_WIDTH 10
#define MATRIX_HEIGHT 20
static uint32_t _matrix[MATRIX_HEIGHT*MATRIX_WIDTH];
engine::window window;
engine::blitprogram blit;
engine::texture matrix;
static uint32_t tetromino_colors[] = {
    RGBA(127,0,127,255),
    RGBA(64,64,127,255),
    RGBA(127,0,127,255),
    RGBA(127,64,64,255),
    RGBA(0,0,255,255),
    RGBA(0,255,0,255),
    RGBA(255,0,0,255)
};
static uint32_t o_tetromino[3*3] = {
    0,tetromino_colors[0],tetromino_colors[0],
    0,tetromino_colors[0],tetromino_colors[0],
    0,0,0};
static uint32_t i_tetromino[4*4] = {
    0,0,0,0,
    tetromino_colors[1],tetromino_colors[1],tetromino_colors[1],tetromino_colors[1],
    0,0,0,0,
    0,0,0,0
};
static uint32_t t_tetromino[3*3] = {
    0,tetromino_colors[2],0,
    tetromino_colors[2],tetromino_colors[2],tetromino_colors[2],
    0,0,0
};
static uint32_t l_tetromino[3*3] = {
    0,0,tetromino_colors[3],
    tetromino_colors[3],tetromino_colors[3],tetromino_colors[3],
    0,0,0
};
static uint32_t s_tetromino[3*3] = {
    0,tetromino_colors[5],tetromino_colors[5],
    tetromino_colors[5],tetromino_colors[5],0,
    0,0,0
};
static uint32_t *_tetromino[] = {
    o_tetromino, i_tetromino, t_tetromino, l_tetromino, 0, s_tetromino, 0
};
engine::texture tetromino[sizeof(tetromino_colors)/sizeof(tetromino_colors[0])];
void run() {
    static double time;
    double t,dt;
    double mx,my;
    window(0,0,1920/2,1080/2,"tetris");
    if (!time) time = glfwGetTime();
    t = glfwGetTime();
    dt = t - time;
    time = t;
    glClear(GL_COLOR_BUFFER_BIT);
    random_color(&_matrix[rand()/(RAND_MAX/(MATRIX_HEIGHT*MATRIX_WIDTH))]);
    matrix(MATRIX_WIDTH,MATRIX_HEIGHT);
    matrix.upload(_matrix);
    static struct {
        float rotate = 0;
    } control;
    int matrixviewport[4] = {
        window.width/2-window.height/2/2,0,window.height/2,window.height
    };
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glfwGetCursorPos(window.window,&mx,&my);
    int _my = MATRIX_HEIGHT*double((matrixviewport[3]-(my-matrixviewport[1]))/matrixviewport[3]);
    int _mx = MATRIX_WIDTH*double((/*matrixviewport[2]-*/(mx-matrixviewport[0]))/matrixviewport[2]);
    if (_my>=0&&_my<MATRIX_HEIGHT&&_mx>=0&&_mx<MATRIX_WIDTH) {
        _matrix[_my*MATRIX_WIDTH+_mx] = 0;
    }
    //control.target = (HMM_V3((mx/WIDTH)*2-1,(my/HEIGHT)*2-1,1));
    if (glfwGetKey(window.window,GLFW_KEY_LEFT)==GLFW_PRESS) {
        control.rotate -= 1*dt;
    }
    if (glfwGetKey(window.window,GLFW_KEY_RIGHT)==GLFW_PRESS) {
        control.rotate += 1*dt;
    }
    blit(matrixviewport[0],matrixviewport[1],matrixviewport[2],matrixviewport[3],matrix);
    
    static unsigned _runcount = 0;
    int scale = window.height/MATRIX_HEIGHT;
    for (int i = 0; i < sizeof(tetromino)/sizeof(tetromino[0]); ++i) {
        if (!_tetromino[i]) continue;
        tetromino[i](i==1?4:3,i==1?4:3);
        if (_runcount==0) {
            tetromino[i].upload(_tetromino[i]);
        }
        blit(i*4*scale,window.height-1*4*scale,(i==1?4:3)*scale,(i==1?4:3)*scale,tetromino[i],control.rotate);
    }
    ++_runcount;
    //int t = (sin(glfwGetTime()/1)*0.5+0.5)*(sizeof(tetromino)/sizeof(tetromino[0]));
    //blit(mx,window.height-my,(t==1?4:3)*scale,(t==1?4:3)*scale,&tetromino[t]);
}

int main() {
    memset(_matrix,RGBA(255,0,0,255),sizeof(_matrix));
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