#include "fluid_simulator.hpp"
#include <iostream>
#include <stdlib.h>

//#ifdef OPENMP
#include <omp.h>
//#endif

#define MAX_THREADS 20

inline int FluidSimulator::AT(int i, int j){return i+(N+2)*j;}
FluidSimulator::FluidSimulator(int N, int M, float dt, float visc, float diff){
  this->N = N; this->M = M; this->dt = dt;
  this->visc = visc, this->diff = diff;
#ifdef OPENMP
  omp_set_num_treads(MAX_THREADS);
#endif
  init(); 
  reset();
}
FluidSimulator::~FluidSimulator(){clean_up();}

void FluidSimulator::clean_up(){
  delete [] U; delete [] U_0; delete [] V; delete [] V_0; delete [] D; delete [] D_0;
}

void FluidSimulator::reset(){
  for(int i = 0; i <= N+1; i++){ for(int j =0; j<=M+1;j++){
      this->U[AT(i,j)] = 0.0; this->U_0[AT(i,j)] = 0.0;//rand()%15;
      this->V[AT(i,j)] = 0.0; this->V_0[AT(i,j)] = 0.0;//rand()%15;
      this->D[AT(i,j)] = 0.0; this->D_0[AT(i,j)] = 0.0;
    }}
}

void FluidSimulator::init(){
  this->U = new float[(M+2)*(N+2)]; this->U_0 = new float[(M+2)*(N+2)];
  this->V = new float[(M+2)*(N+2)]; this->V_0 = new float[(M+2)*(N+2)];
  this->D = new float[(M+2)*(N+2)]; this->D_0 = new float[(M+2)*(N+2)];
}

void FluidSimulator::simulate(){
  //  for(int i = 0; i <= N+1; i++){ for(int j =0; j<=M+1;j++){
  //      this->U[AT(i,j)] = .001;
  //    }}
  step_velocity(U, V, U_0, V_0, visc, dt);
  step_density(D, D_0, U, V, diff, dt);
}

void FluidSimulator::step_density(float *x, float *x0,
				  float* u, float *v,
				  float diff, float dt){
  //  add_source(x, x0, dt);
  swap(x0,x); 
  diffuse(0, x, x0, diff, dt);
  swap(x0,x); 
  advect(0, x, x0, u, v, dt);
}


void FluidSimulator::add_source(float *x, float *s, float dt){
  int size = (N+2)*(M+2);
  for(int i = 0; i <size; i++) x[i] += dt*s[i];
}

void FluidSimulator::diffuse(int b, float *x, float *x0, float diff, float dt){
  int i=0,j=0,k=0;
  float a = dt*diff*N*M;
  for(k=0; k<20; k++){
#pragma omp parallel for
    for(i = 1; i<=N;i++){
#pragma omp parallel for
      for(j = 1; j<=M;j++){
	x[AT(i,j)] = (x0[AT(i,j)] + a*(x[AT(i-1,j)] + x[AT(i+1,j)] +
				       x[AT(i,j-1)] + x[AT(i,j+1)]))/(1+4*a);
      }
    }
    set_boundary(b,x);
  }
}

void FluidSimulator::advect(int b, float *d, float *d0,
			    float *u, float *v, float dt){
  int i, j, i0, j0, i1, j1;
  float x=0.0, y=0.0, s0=0.0, t0=0.0, s1=0.0, t1=0.0, dt0=0.0;

  float dt0_x = 1;
  float dt0_y = 1;
  for(i = 1; i<=N; i++){
    for(j = 1; j<=M; j++){
      x = i-dt0_x*u[AT(i,j)];
      y = j-dt0_y*v[AT(i,j)];
      if(x<0.5) x = 0.5;
      if(x>N+.5) x=N+.5;
      i0 = (int)x;
      i1 = i0+1;
      if(y<0.5)	y=0.5;
      if(y>M+.5) y= M+.5;
      j0 = (int)y;
      j1 = j0+1;
      s1 = x-i0; s0 = 1-s1;
      t1 = y-j0; t0 = 1-t1;
      d[AT(i,j)] = s0 *(t0*d0[AT(i0,j0)] + t1*d0[AT(i0,j1)]) +
	s1*(t0*d0[AT(i1,j0)]+t1*d0[AT(i1,j1)]);
    }}

  set_boundary(b,d);
}


template<class T>
inline void FluidSimulator::swap(T &a,T &b){T c = a; a = b; b = c;}

void FluidSimulator::step_velocity(float *u, float *v, float *u0, float*v0, float visc, float dt){
  add_source(u, u0, dt); add_source(v, v0,dt);
  swap(u0,u); diffuse(1, u, u0, visc, dt);
  swap(v0,v); diffuse(2, v, v0, visc, dt);
  project(u, v, u0, v0);
  swap(u0, u); swap(v0, v);
  advect(1, u, u0, u0, v0, dt); advect(2, v, v0, u0, v0, dt);
  project(u, v, u0, v0);
}

void FluidSimulator::project(float *u, float *v, float *p, float *div){
  int i, j, k;
  float dx, dy;
  dx = 1.0/N;
  dy = 1.0/M;
  for(i = 1; i<=N; i++){
    for(j=1; j<=M; j++){
      div[AT(i,j)] = -.5 * dx *(u[AT(i+1,j)] - u[AT(i-1,j)] +
				v[AT(i,j+1)] - v[AT(i, j-1)]);
      p[AT(i,j)] = 0;
    }
  }
  set_boundary(0, div); set_boundary(0, p);
  for(k =0; k<20; k++){
    for(i=1; i<=N; i++){
      for(j = 1; j <= M; j++){
	p[AT(i,j)] = (div[AT(i,j)] + p[AT(i-1,j)] + p[AT(i+1,j)]+
		      p[AT(i,j-1)] + p[AT(i,j+1)])/4.0;
      }
    }
    set_boundary(0, p);
  }
#pragma omp parallel for
  for(i = 1; i <=N; i++){
#pragma omp parallel for
    for(j = 1; j<= M; j++){
      u[AT(i,j)] -= 0.5 *(p[AT(i+1,j)] - p[AT(i-1,j)])/dx;
      v[AT(i,j)] -= 0.5*(p[AT(i,j+1)] - p[AT(i,j-1)])/dy;
    }
  }
  set_boundary(1,u); set_boundary(2,v);
}


void FluidSimulator::set_boundary(int b, float *x){
  int i, j;
#pragma omp parallel for
  for(i = 0; i<= N+1; i++){
    x[AT(i,0)] = b==2 ? -x[AT(i,1)]: x[AT(i,1)];
    x[AT(i,M+1)] = b ==2 ? -x[AT(i,M)] : x[AT(i,M)];
  }
#pragma omp parallel for
  for(j = 0; j<= M+1; j++){
    x[AT(0,j)] = b == 1 ? -x[AT(1,i)] : x[AT(1,i)];
    x[AT(N+1, j)] = b == 1 ? -x[AT(N,j)]: x[AT(N,j)];
  }
  x[AT(0,0)] = 0.5 * (x[AT(1,0)] + x[AT(0,1)]);
  x[AT(0,M+1)] = 0.5 * (x[AT(1,M+1)] + x[AT(0, M)]);
  x[AT(N+1, 0)] = 0.5 * (x[AT(N, 0)] + x[AT(N+1,1)]);
  x[AT(N+1,M+1)] = 0.5 * (x[AT(N,M+1)] + x[AT(N+1,M)]);
}

void FluidSimulator::add_density(int i, int j){
  std::cout<<"i "<<i<<" "<<"j "<<j<<std::endl;
  for(int x = -1; x <=1; x++){
    for(int y = -1; y<=1;y++){
      D[AT(i+x,j+y)] = 10.0;
    }
  }
}

void FluidSimulator::add_force(int vx, int vy, int i, int j){
  U[AT(i,j)] = 4.0;

}

