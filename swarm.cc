#include "swarm.hpp"
#include "color.hpp"
#include <fstream>
#include <sys/time.h>
#include <cmath>
#include <cassert>
#include <vector>
#include <iostream>
#include <boost/foreach.hpp>
#include <boost/next_prior.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/random/uniform_real.hpp>
#include <boost/random/normal_distribution.hpp>

namespace eb {namespace swarm{
template<typename T>
T square(T x) { return x*x; }

const int max_fps = 40; 
const int min_fps =  16;

const double desired_dt =  0.1;

struct boid {
  boid(point position,
      vector velocity,
      color c)
   : pos(position)
   , vel(velocity)
   , col(c)
   , strenght(1)
   , force()
   , counter(0){}

  point pos;
  vector vel;
  color col;
  float strenght; // targets only
  vector force;   // boids only
  int counter;
};

boost::mt19937 global_rng;
boost::uniform_real<> uni;
int my_random(int top, int low = 0) {
  boost::uniform_int<> uni(low,top-1);
  boost::variate_generator<
  boost::mt19937&, boost::uniform_int<>
    > die(global_rng, uni);
  return die();                 
}

double my_frandom(double max, double min=0) {
  boost::uniform_real<> uni(min, max);
  return uni(global_rng); 
}

double my_nrandom(double avg = 0, double dev = 1) {
  boost::normal_distribution<> norm(avg, dev);
  boost::uniform_real<> uni(0,1);
  boost::variate_generator<boost::mt19937&, boost::uniform_real<> > vg(global_rng, uni);
  double r = norm(vg);
  return r;
}



color normal_color() {
  return color(my_frandom(.5, .3),
               my_frandom(.5, .3),
               my_frandom(1,  .5),
               my_frandom(0.6,0.4));
}

color target_color() {
  return color(my_frandom(1, 0.7),
               my_frandom(1., 0.7),
               my_frandom(0.5));
}
struct state_impl {
  state_impl(int width, int height);
  unsigned long update(world&);
private:
  void init_boids(int nboids =300, int ntargets = 1);
  boid create_random_boid(color c) const;
  float min_velocity() const;
  void draw_boids(world&);
  void check_limits(boid&b) const;
  void update_state();
  void random_small_change();
  void init_time();
  double get_time() const;


  unsigned long delay;
  float x_max; // 1.0
  float y_max; // height/width

  float dt;
  float target_velocity;
  float target_accelleration;
  float max_velocity;
  float min_velocity_multiplier;
  float noise;
  float change_probability;

  int rsc_call_depth;
  int rbc_call_depth;

  float draw_fps;
  float draw_time_per_frame, draw_elapsed;
  double draw_start, draw_end;
  int draw_cnt;
  int draw_sleep_count;
  int draw_delay_accum;
  int draw_nframes;

  std::vector<boid> boids;
  std::vector<boid> targets;
  ::timeval startup_time;
};

boid state_impl::create_random_boid(color c) const {
  return boid(point(my_frandom(x_max),
                    my_frandom(y_max)),
              vector(my_frandom(max_velocity/2),
                     my_frandom(max_velocity/2)),
             c);
}

void state_impl::init_boids(int nboids, int ntargets){
  boids.clear();
  targets.clear();

  while(nboids--) {
    boids.push_back(
      create_random_boid(
        normal_color()));
    
  }

  while(ntargets--) {
    targets.push_back(create_random_boid(target_color()));
  }
}

struct boid_parameters {
  float dt;
  float target_velocity;
  float target_accelleration;
  float max_velocity;
  float noise;

  int change_probability;
};


float state_impl::min_velocity() const
{
  return max_velocity*min_velocity_multiplier;
}

void state_impl::draw_boids(world&w) {
  BOOST_FOREACH(boid& b, boids) {
    gl::draw_dot(w, b.pos, b.col);
  }
  BOOST_FOREACH(boid& b, targets) {
    gl::draw_dot(w, b.pos, b.col);
  }
}

void state_impl::check_limits(boid& b) const { 
  /* check limits on targets */
  if (b.pos.x < 0) {
    /* bounce */
    b.pos.x = -b.pos.x;
    b.vel.x = -b.vel.x;
  } else if (b.pos.x >= x_max) {
    /* bounce */
    b.pos.x = 2*x_max-b.pos.x;
    b.vel.x = -b.vel.x;
  }
  if (b.pos.y < 0) {
    /* bounce */
    b.pos.y = -b.pos.y;
    b.vel.y = -b.vel.y;
  } else if (b.pos.y >= y_max) {
    /* bounce */
    b.pos.y = 2*y_max-b.pos.y;
    b.vel.y = -b.vel.y;
  }
}


#include <xmmintrin.h>

const float G = 0.001;
const float Gx = -0.0001;
void state_impl::update_state() {

  
  int oldMXCSR = _mm_getcsr(); //read the old MXCSR setting
  int newMXCSR = oldMXCSR | 0x8040; // set DAZ and FZ bits
  _mm_setcsr( newMXCSR ); //write the new MXCSR setting to the MXCSR
  

  
  BOOST_FOREACH(boid& b, boids) {
    b.force = vector();
    BOOST_FOREACH(boid& t, targets){
      vector dist = t.pos - b.pos;
      const float norm_sq = inner_prod(dist) ;
      float inorm = inv_sqrt(norm_sq)  ;
      vector dir = dist * inorm;
      //if(inorm > 1000) continue;
      if(inorm > 50) {
        inorm = 50;
        dir = rotate(dir,M_PI*0.5);
      }
//       const double angle = M_PI*0.50;
//       dir =  vector(dir.x*cos(angle)-dir.y*sin(angle),
//                    dir.x*sin(angle)+dir.y*cos(angle));

      b.force +=  dir * t.strenght * std::pow(inorm,2) *G  ;
      //b.force +=  dir * t.strenght * std::pow(inorm,3) *Gx ;


    }
  }


  /* update target state */
  BOOST_FOREACH(boid& b, targets) {
    double theta = my_frandom(2*M_PI);
    vector accelleration (
      target_accelleration*cos(theta),
      target_accelleration*sin(theta)
    );

    b.vel += accelleration*dt;

    /* check velocity */
    double iv = inv_sqrt(inner_prod(b.vel));
    if (iv < 1/target_velocity) {
      double ratio = target_velocity * iv;
      /* save old vel for acc computation */
      vector old_vel = b.vel;

      /* compute new velocity */
      b.vel *= ratio;
      
      /* update acceleration */
      accelleration = (b.vel-old_vel)/dt;
    }

    /* update position */
    b.pos += b.vel*dt + accelleration*0.5*square(dt);
    
    check_limits(b);
  }

  /* update boid state */
  BOOST_FOREACH(boid&b, boids) {
    //vector vnoise = (my_random(M_PI*2))*my_nrandom(0, noise*norm_2(b.force));
    vector accelleration = b.force;//  + vector(my_nrandom(0,abs(noise*b.force.x)),my_nrandom(0,abs(noise*b.force.y)));


    b.vel += accelleration*dt;
    vector old_vel = b.vel;

    double ratio = 1;
    float v = norm_2(b.vel);
    if (v > max_velocity)  {
      ratio = max_velocity/v;
      //b.vel = rotate(b.vel, (my_random(1, 0)==0?1:1)*M_PI*0.1);
    } else if (v < min_velocity())  {
      ratio = min_velocity()/v;
    }
    b.vel *= ratio;
    accelleration = (b.vel-old_vel)/dt;         
    vector acc = accelleration*0.5*square(dt);
    b.pos += b.vel*dt + acc;
    //std::cout << b.force << " "<< (b.vel*dt) << " "<<acc<<" "<<ratio<<"\n";
    check_limits(b);
  }
  _mm_setcsr( oldMXCSR ); 
}

template<typename List>
bool has_more_than_one(List const& l)  {
  return !l.empty() && boost::next(l.begin()) != l.end();
}

void mutate_parameter(float &param){
  param *= 0.75+my_frandom(0.5);
}

void state_impl::random_small_change(){
  int which = 0;

  which = my_random(5);

  if (++rsc_call_depth > 10) {
    rsc_call_depth--;
    return;
  }
  
  switch(which) {
  case 0:
    /* target acceleration */
    std::cout << "target-accelleration: "<<target_accelleration <<"->";
    mutate_parameter(target_accelleration);
    std::cout << target_accelleration<<"\n";
    break;

  case 1:
    /* velocity */
    std::cout << "max-velocity: "<<max_velocity <<"->";
    mutate_parameter(max_velocity);
    std::cout << max_velocity<<"\n";
    break;

  case 2: 
    /* target velocity */
    std::cout << "target-velocity: "<<target_velocity <<"->";
    mutate_parameter(target_velocity);
    std::cout << target_velocity<<"\n";
    break;

  case 3:
    /* noise */
    std::cout << "noise: "<<noise <<"->";
    mutate_parameter(noise);
    std::cout << noise<<"\n";
    break;

  case 4:
    /* min_velocity_multiplier */
    std::cout << "min-velocity-multiplier: "<<min_velocity_multiplier <<"->";
    mutate_parameter(min_velocity_multiplier);
    std::cout << min_velocity_multiplier<<"\n";
    break;
    
  }
  // Clamp values
  if (min_velocity_multiplier < 0.3)
    min_velocity_multiplier = 0.3;
  else if (min_velocity_multiplier > 0.9)
    min_velocity_multiplier = 0.9;
  if (noise < 0.0)
    noise = 0.0;
  if (max_velocity < 0.02)
    max_velocity = 0.02;
  if (target_velocity < 0.0)
    target_velocity = 0.0;
  if (target_accelleration > target_velocity*0.7)
    target_accelleration = target_velocity*0.7;
  if (target_accelleration > target_velocity*0.7)
    target_accelleration = target_velocity*0.7;
  if (target_accelleration < 0.005)
    target_accelleration = 0.005;

  rsc_call_depth--;
}

void state_impl::init_time()
{
  gettimeofday(&startup_time, NULL);
}

double state_impl::get_time() const
{
  ::timeval t;
  float f;
  gettimeofday(&t, NULL);
  t.tv_sec -= startup_time.tv_sec;
  f = ((double)t.tv_sec) + t.tv_usec*1e-6;
  return f;
}

state_impl::state_impl(int width, int height)
 : delay(0)
 , x_max(1.0)
 , y_max(float(height)/width)
 , dt(0.3)
 , target_velocity(0.0)
 , target_accelleration(0.02)
 , max_velocity(0.1)
 , min_velocity_multiplier(0.5)
 , noise(2)
 , change_probability(0.00)
 , rsc_call_depth(0)
 , rbc_call_depth(0)
 , draw_fps(0)
 , draw_time_per_frame(0)
 , draw_elapsed(0)
 , draw_start(0)
 , draw_end(0)
 , draw_sleep_count(0)
 , draw_delay_accum(0)
 , draw_nframes(0)
 , startup_time() {

  boost::mt19937::result_type seed;
  std::ifstream dev_random("/dev/random");
  if(!dev_random) {
    throw "could not open /dev/random";
  }
  
  dev_random.read((char*)&seed, sizeof(seed));
  if(!dev_random) {
    throw "could not read from /dev/random";
  }
  global_rng = boost::mt19937(boost::mt19937::result_type(seed));
  
  init_boids();
  init_time();
}


unsigned long
state_impl::update(world& w) {
  unsigned long this_delay = delay;

  draw_start = get_time();

  if (delay > 0) {
    draw_cnt = 2;
    dt = desired_dt/2;
  } else {
    draw_cnt = 1;
    dt = desired_dt;
  }

  for (; draw_cnt > 0; draw_cnt--) {
    update_state();
    draw_boids(w);
  }

  draw_end = get_time();
  draw_nframes++;
  if (my_frandom(1.0) < change_probability) random_small_change();
  
  if (true || draw_end > draw_start+0.5) {
    

    //if (my_frandom(1.0) < change_probability*0.3) random_big_change();
    draw_elapsed = draw_end-draw_start;
	
    draw_time_per_frame = draw_elapsed/draw_nframes - delay*1e-6;
    draw_fps = draw_nframes/draw_elapsed;

//     std::cout <<"elapsed:"<<draw_elapsed<<"\n";
//     std::cout <<"fps: "<<draw_fps <<"secs per frame: "<<draw_time_per_frame <<" delay: "<<delay<<std::endl; 

    if (draw_fps > max_fps) {
      delay = (1.0/max_fps - (draw_time_per_frame + delay*1e-6))*1e6;
    } 
    draw_start = get_time();
    draw_nframes = 0;
  }
    
  if (delay <= 10000) {
    draw_delay_accum += delay;
    if (draw_delay_accum > 10000) {
      this_delay = draw_delay_accum;
      draw_delay_accum = 0;
      draw_sleep_count = 0;
    }
    if (++draw_sleep_count > 2) {
      draw_sleep_count = 0;
      this_delay = 10000;
    }
  }

  return this_delay;
}

state::state(int w, int h)
 : pimpl(new state_impl(w,h)){}

unsigned long
state::update(world& w) {
  return pimpl->update(w);
}

state::~state() {
  delete pimpl;
}

}}
