#include<iostream>
#include <GL/glew.h>
#include <GL/glu.h>
#include <GL/gl.h>
#include <SDL/SDL.h>
#include "math/math.hpp"
#include "simulate.cpp"

#include <omp.h>

#define MAX_THREADS 20
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 800
#define SCREEN_BPP 32
#define TRUE 1
#define FALSE 0


#define ASSERT(condition){if(!(condition)){std::cerr<<"ASSERTION FAILED: "<<#condition<<"@"<<__FILE__<<"("<<__LINE__<<")"<<std::endl;exit(EXIT_FAILURE);}}
#define AT(i,j) ((i) + (N+2)*(j))
#define IN(i) (i>1 && i<N)
template<class T>
inline void swap(T& a, T& b){T c = a; a=b; b=c;}



extern void step_density(int N, float *x, float *x0,
			 float *u, float *v,
			 float diff, float dt, int *bnd);
extern void step_velocity(int N, float *u, float *v,
			 float *u0, float *v0,
			  float visc, float dt, int *bnd);


/*SDL globals*/
SDL_Surface *surface;
int videoFlags;
bool Running;
bool Active;
int Width = SCREEN_WIDTH;
int Height = SCREEN_HEIGHT;

/*program globals*/
static int N;
static float dt, diff, visc;
static float force, source;
static int dvel;
static bool mouse_down[3];
static int omx, omy, mx, my;

static float *u, *v, *u0, *v0;
static float *d, *d0;
static int *boundary, *perm_density;
static int size;
static bool fire_mode;





static void clean_up(){
  if(u) delete [] u; 
  if(v) delete [] v;
  if(u0) delete [] u0;
  if(v0) delete [] v0;
  if(d) delete [] d;
  if(d0) delete [] d0;
  if(boundary) delete [] boundary;
  if(perm_density) delete [] perm_density;
}

static void reset(){
  int i,j;
//#pragma omp parallel for
  for(i=0; i<size; i++){
   u[i] = v[i] = u0[i] = v0[i] = d[i] = d0[i] = 0.0;
   boundary[i] = perm_density[i] = 0;
  }
}

static void allocate(){
  u = new float[size];
  v = new float[size];
  u0 = new float[size];
  v0 = new float[size];
  d = new float[size];
  d0 = new float[size];
  boundary = new int[size];
  perm_density = new int[size];
  ASSERT(u && v && u0 && v0 && d && d0);
}


static void Quit( int return_code){
  SDL_Quit();
  exit( return_code);
}

static int initGL(void)
{
  glShadeModel(GL_SMOOTH);
  glEnable (GL_BLEND);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
  return (TRUE);
}

static int resize_window(int width, int height)
{
  Width = width;
  Height = height;
  GLfloat ratio;
  if(height == 0)
    height = 1;
  ratio = (GLfloat)width/(GLfloat)height;
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluOrtho2D(0.0, 1.0, 0.0, 1.0);
  glViewport(0,0, Width, Height);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  return(TRUE);
}


/* Drawing begins*/
static void draw_velocity(void){
  float x, y, h;
  int i,j;
  h = 1.0/N;
  glColor3f(1.0, 1.0, 1.0);
  glBegin(GL_LINES);
//#pragma omp parallel for
  for(i = 1; i<= N; i++){
    x = (i-.5f) * h;
//#pragma omp parallel for
      for(j = 1; j<=N; j++){
	y = (j-.5f)*h;
	glVertex2f(x,y);
	glVertex2f(x+u[AT(i,j)], y+v[AT(i,j)]);
      }
  }
    glEnd();
}

static void draw_boundary(void){
  float x, y, h, b00, b01, b10, b11, xm, ym;
  int i, j;
  h = 1.0f/N;
  glBegin(GL_QUADS);
//#pragma omp parallel for
  for(i = 0; i<=N; i++){
    x = (i-.5f)*h;
//#pragma omp parallel for
    for(j = 0; j<=N; j++){
	y = (j-.5f)*h;
      if(boundary[AT(i,j)]== 1){
	xm = (i+CUBE_SIZE-.5f)*h;
	ym = (j+CUBE_SIZE-.5f)*h;
	glColor3f(1.0, 0.0, 0.0);
	glVertex2f(x,y);
	glVertex2f(x,ym);
	glVertex2f(xm, ym);
	glVertex2f(xm, y);
/*
	y = (j-.5f)*h;
	b00 = boundary[AT(i,j)];
	b01 = boundary[AT(i,j+1)];
	b10 = boundary[AT(i+1,j)];
	b11 = boundary[AT(i+1,j+1)];
	glColor3f(1.0, 0.0, 0.0);
	glVertex2f(x,y);
	glVertex2f(x+h,y);
	glVertex2f(x+h, y+h);
	glVertex2f(x, y+h);
				*/
	
      }
    }
  }
  glEnd();
}

static void draw_density(void){
  float x, y, h, d00, d01, d10, d11;
  int i, j;
  float c00=0.0, c01=0.0, c10=0.0, c11=0.0;
  h = 1.0f/N;
  glBegin(GL_QUADS);
//#pragma omp parallel for
  for(i = 0; i <= N; i++){
    x = (i-.5f)*h;
//#pragma omp parallel for
    for(j =0; j<=N; j++){
      y = (j-.5f)*h;
      d00 = d[AT(i,j)];
      d01 = d[AT(i,j+1)];
      d10 = d[AT(i+1,j)];
      d11 = d[AT(i+1,j+1)];
      if(!fire_mode){
	glColor3f(d00, d00, d00); glVertex2f(x,y);
	glColor3f(d10, d10, d10); glVertex2f(x+h,y);
	glColor3f(d11, d11, d11); glVertex2f(x+h, y+h);
	glColor3f(d01, d01, d01); glVertex2f(x, y+h);
      }else{
	glColor3f(d00*d00, 0.0, (2-d00)*d00); glVertex2f(x,y);
	glColor3f(d10*d10, 0.0, (2-d10)*d10); glVertex2f(x+h,y);
	glColor3f(d11*d11, 0.0, (2-d11)*d11); glVertex2f(x+h, y+h);
	glColor3f(d01*d01, 0.0, (2-d01)*d01); glVertex2f(x, y+h);
      }
    }
  }
  glEnd();
}

static void draw_particle_density(void){
  float x, y, h, d00, d01, d10, d11;
  int i, j;
  h = 1.0f/N;
  glBegin(GL_QUADS);
//#pragma omp parallel for
  for(i = 0; i <= N; i++){
    x = (i-.5f)*h;
//#pragma omp parallel for
    for(j =0; j<=N; j++){
      y = (j-.5f)*h;
      d00 = d[AT(i,j)];
      glColor3f(d00, d00, d00);
      glVertex2f(x,y);
      glVertex2f(x+h,y);
      glVertex2f(x+h, y+h);
      glVertex2f(x, y+h);
    }
  }
  glEnd();
}


/* UI interaction */

static void get_from_UI(float *d, float *u, float *v){
  int i,j;
//#pragma omp parallel for

  for(i=0; i<size; i++){
    u[i] = v[i] = d[i] = 0.0f;
    //    if(particles)
      //      v[i] = -.05;
  }

  i = (int)((mx/(float)Width)*N+1);
  j = (int)(((Height-my)/(float)Height)*N+1);
//#pragma omp parallel for
  for(int i0 = 1; i0<=N;i0++){
//#pragma omp parallel for
    for(int j0 = 1; j0<=N;j0++){
      if(perm_density[AT(i0,j0)] != 0){
	d[AT(i0,j0)] = source;
      }
    }
  }

  //  if(mouse_down[1]){
  //    boundary[AT(i,j)] = 1;
  //    boundary[AT(i+1,j)] = 1;
  //    boundary[AT(i+1,j+1)] = 1;
  //    boundary[AT(i,j+1)] = 1;
  //  }
  if(!mouse_down[0] && !mouse_down[2]) return;
  
  if(i <1 || i >N || j < 1 || j > N) return;
  if(mouse_down[2]){
    u[AT(i,j)] = force *(mx-omx);
    v[AT(i,j)] = force *(omy - my);
  }
  if(mouse_down[0] && IN(i) && IN(j)){
    d[AT(i,j)] = source;
  }

  omx = mx;
  omy = my;
  return;
}

static void create_sphere(){
  float x, y, r = 10.0;
  int i0, j0;
//#pragma omp parallel for
  for(float theta = 0.0; theta <= 2*PI; theta+=.1){
    i0 = r * cos(theta) + floor(((float)mx/Width)*N+1);
    j0 = r * sin(theta) + floor(((float)(Height-my)/Height)*N+1);
    boundary[AT(i0,j0)] = 1;
    boundary[AT(i0+1,j0+1)] = 1;
    boundary[AT(i0,j0+1)] = 1;
    boundary[AT(i0+1,j0)] = 1;
  }
}

static void create_cube(){
  int i0 = floor(((float)mx/Width)*N+1);
  int j0 =  floor(((float)(Height-my)/Height)*N+1);
  boundary[AT(i0,j0)] = 1;
}

static void add_density_source(){
  int i0, j0;
  i0 = (int)((mx/(float)Width)*N+1);
  j0 = (int)(((Height-my)/(float)Height)*N+1);
  if(i0<N && i0>1 && j0<N && j0>1)
    perm_density[AT(i0,j0)] = 1;


}

enum line_type{HORIZONTAL, VERTICAL};
static void add_density_line(int type){
  int i, i0, j0;
  i0 = (int)((mx/(float)Width)*N+1);
  j0 = (int)(((Height-my)/(float)Height)*N+1);
  for(i = 1; i< N; i++){
    if(type == HORIZONTAL && IN(i0)){
      d[AT(i0, i)] = source;
    }
    if(type == VERTICAL && IN(j0)){
      d[AT(i, j0)] = source;
    }
  }
}
static void handle_keypress(const SDL_keysym *keysym){
  switch(keysym->sym){
  case SDLK_ESCAPE: Running = false; break;
  case SDLK_F1: SDL_WM_ToggleFullScreen(surface); break;
  case SDLK_r: reset(); break;
  case SDLK_v: dvel = !dvel; break;
  case SDLK_s: create_sphere(); break;
  case SDLK_q: create_cube(); break;
  case SDLK_h: add_density_source(); break;
  case SDLK_1: add_density_line(HORIZONTAL); break;
  case SDLK_2: add_density_line(VERTICAL); break;
  case SDLK_p: particles = !particles;
  case SDLK_f: fire_mode = !fire_mode;
    
  default: break;
  }
}

static void handle_mouse_motion(SDL_MouseMotionEvent motion){
  mx = motion.x;
  my = motion.y;
}

void handle_mouse_button(SDL_MouseButtonEvent button){
  omx = mx = button.x;
  omy = my = button.y;
  mouse_down[(button.button<<SDL_BUTTON_LEFT) % 3] =
    button.state == SDL_PRESSED;
}

void handle_event(){
  SDL_Event event;
  while(SDL_PollEvent(&event)){
    switch(event.type){
    case SDL_ACTIVEEVENT:
      //lost focus
      if(event.active.gain == 0)
	Active = true;
      else
	Active = true;
      break;
    case SDL_VIDEORESIZE:
      surface = SDL_SetVideoMode(event.resize.w,
				 event.resize.h,
				 SCREEN_BPP, videoFlags);
    case SDL_KEYDOWN: handle_keypress(&event.key.keysym); break;
    case SDL_MOUSEMOTION: handle_mouse_motion(event.motion); break;
    case SDL_MOUSEBUTTONDOWN: handle_mouse_button(event.button); break;
    case SDL_MOUSEBUTTONUP: handle_mouse_button(event.button); break;
    case SDL_QUIT: Running = false; break;
    default: break;
    }
  }
}

void render(){
  static GLint T0 = 0;
  static GLint Frames = 0;
  
  glClear(GL_COLOR_BUFFER_BIT);
  glLoadIdentity();
  
  if(!particles)
    draw_density();
  else
    draw_particle_density();

  draw_boundary();
  if(dvel)
    draw_velocity();

  SDL_GL_SwapBuffers();
  Frames++;
  {
    GLint t = SDL_GetTicks();
    if(t - T0 >= 5000){
      GLfloat seconds = (t-T0)/1000.0;
      GLfloat fps = Frames/seconds;
      std::cout<<Frames<<" frames in "<<seconds<<" = "<<fps<<" FPS"<<std::endl;
      T0 = t;
      Frames = 0;
    }
  }
}

static void update(){}

static void simulate(){
  get_from_UI(d0, u0, v0);
  step_velocity(N, u, v, u0, v0, visc, dt, boundary);
  step_density(N, d, d0, u, v, diff, dt, boundary);
}
static void main_loop(){
  Running = true;
  Active = true;
  while(Running){
    handle_event();
    if(Active){
      simulate();
      render();
      update();
    }
  }
}

bool initSDL(){
  const SDL_VideoInfo * videoInfo;

  if(SDL_Init(SDL_INIT_VIDEO)<0){
    std::cout<<"error starting SDL  "<<SDL_GetError()<<std::endl;
    Quit(1);
  }
  videoFlags = SDL_OPENGL;
  videoFlags |= SDL_GL_DOUBLEBUFFER;
  videoFlags |= SDL_RESIZABLE;
  if(videoInfo->hw_available)
    videoFlags|= SDL_HWSURFACE;
  else
    videoFlags|= SDL_SWSURFACE;
  if(videoInfo->blit_hw)
    videoFlags|= SDL_HWACCEL;
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  surface = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT,
			     SCREEN_BPP, videoFlags);
  if(!surface){
    std::cout<<"Video mode set failed: "<<SDL_GetError()<<std::endl;
    Quit(1);
  }
  return true;
}

static bool init()
{
  ASSERT(initSDL());
  ASSERT(initGL() == 1);
  resize_window(SCREEN_WIDTH, SCREEN_HEIGHT);

#ifdef OPENMP
  omp_set_num_treads(MAX_THREADS);
#endif
  fire_mode = false;
  N = 100;
  dt = .1f;
  //  visc = 0.0005f;

  //  force = 5.0f;
  force = 5.0f;
  source = 100.0f;
  visc = 0.0008f;
  diff = 0.000f;

  if(particles)
    source = 1.0f;
  else

  dvel = 0;
  size = (N+2)*(N+2);
  boundaries[TOP] = NO_SLIP;
  boundaries[BOTTOM] = NO_SLIP;
  boundaries[LEFT] = NO_SLIP;
  boundaries[RIGHT] = NO_SLIP;
  wall_velocity[TOP] = 0.0;
  wall_velocity[BOTTOM] = 0.0;
  wall_velocity[LEFT] = 0.0;
  wall_velocity[RIGHT] = 0.0;
  inlet_velocity[TOP] = 0.0;
  inlet_velocity[BOTTOM] = 0.0;
  inlet_velocity[RIGHT] = 0.0;
  inlet_velocity[LEFT] = 0.0;


  allocate();
  reset();
  return true;
}


int main(int argc, char **argv){
  init();
  main_loop();
  clean_up();
  return(0);
}
