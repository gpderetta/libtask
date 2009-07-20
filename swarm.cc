#include "swarm.hpp"
#include "color.hpp"
#include <sys/time.h>
#include <cmath>
#include <cassert>
#include <list>
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

struct boid;
typedef std::list<boid> boid_list;
typedef boid* boid_ptr;
typedef boid_list::iterator boid_iterator;

struct boid {
  boid(point position,
      vector velocity,
      color c)
   : pos(position)
   , vel(velocity)
   , col(c)
   , closest(closest)
   , counter(0){}

  point pos;
  vector vel;
  color col;
  boid_ptr closest;
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

double my_nrandom(double avg, double dev = 1) {
  boost::normal_distribution<> norm(avg, dev);
  boost::uniform_real<> uni(0,1);
  boost::variate_generator<boost::mt19937&, boost::uniform_real<> > vg(global_rng, uni);
  double r = norm(vg);
  std::cerr << r << "\n";
  return r;
}



color normal_color() {
  return color(my_frandom(0.5),
               my_frandom(0.5),
               my_frandom(1, 0.7),
               my_frandom(0.6,0.4));
}

color target_color() {
  return color(my_frandom(1, 0.7),
               my_frandom(1., 0.7),
               my_frandom(0.5));
}

enum schemes {
  gray_trails,
  gray_schizo,
  color_trails,
  random_trails,
  random_schizo,
  color_schizo,
  num_schemes
};


struct state_impl {
  state_impl(int width, int height);
  unsigned long update(world&);
private:
  void init_boids(int nboids = 600, int ntargets = 10);
  boid create_random_boid(color c) const;
  void pick_new_targets();
  void add_boids(int num);
  void add_targets(int num);
  float min_velocity() const;
  void draw_boids(world&);
  void check_limits(boid&b) const;
  void mutate_boids(int which) ;
  void update_state();
  void update_boid(int which);
  void random_small_change();
  void random_big_change();
  void init_time();
  double get_time() const;
  boid_iterator pick_random_target() ;
  boid_iterator pick_random_boid() ;


  unsigned long delay;
  float x_max; // 1.0
  float y_max; // height/width

  float dt;
  float target_velocity;
  float target_accelleration;
  float max_velocity;
  float max_accelleration;
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

  std::list<boid> boids;
  std::list<boid> targets;
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
  BOOST_FOREACH(boid&b, boids)
    b.closest = &*pick_random_target();
  

}

struct boid_parameters {
  float dt;
  float target_velocity;
  float target_accelleration;
  float max_velocity;
  float max_accelleration;
  float noise;

  int change_probability;
};

void state_impl::pick_new_targets(){
  BOOST_FOREACH(boid& b, boids)
    b.closest = &*pick_random_target();
}

void state_impl::add_boids(int num)
{
  assert(num > 0);
  while(num--)  {
    boid b = *pick_random_boid();
    b.closest = &*pick_random_target();
    boids.push_back(b);
  }
}

void state_impl::add_targets(int num)
{
  assert(num > 0);

  while(num--) {
    boid b = *pick_random_target();
    //b.closest = &*pick_random_target();
    targets.push_back(b);
  }
}

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

void state_impl::update_state() {
  BOOST_FOREACH(boid& b, boids) {
    if(b.counter > 0) {
      float min_distance = INFINITY;
      BOOST_FOREACH(boid& t, targets){
        if(&t == b.closest) continue;
        float distance = inner_prod(t.pos - b.pos);
        if (distance < min_distance ) {
          std::cerr <<"new closest\n";
          b.closest = &t;
          min_distance = distance;
        }
      }
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
    double v_squared = inner_prod(b.vel);
    if (v_squared > square(target_velocity)) {
      double ratio = target_velocity/sqrt(v_squared);
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
    point target = b.closest->pos
      + vector(my_frandom(noise, -noise),my_frandom(noise, -noise));
    double theta = atan2(target - b.pos);
    vector accelleration (
      max_accelleration*cos(theta),
      max_accelleration*sin(theta)
    );

    b.vel += accelleration*dt;

    /* check velocity */
    double v_squared = inner_prod(b.vel);
    if (v_squared > square(max_velocity)) {
      double ratio = max_velocity/sqrt(v_squared);

      /* save old vel for acc computation */
      vector old_vel = b.vel;

      /* compute new velocity */
      b.vel *= ratio;
      
      /* update acceleration */
      accelleration = (b.vel-old_vel)/dt;
    } else if (v_squared < square(min_velocity())) {
      double ratio = min_velocity()/sqrt(v_squared);

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
}

template<typename List>
bool has_more_than_one(List const& l)  {
  return !l.empty() && boost::next(l.begin()) != l.end();
}


void state_impl::mutate_boids(int which) {
  // XXX change color of mutated boid
  if (which == 0) {
    /* turn boid into target */
    if(has_more_than_one(boids)){
      boid_iterator b = pick_random_boid();
      targets.splice(targets.end(), boids, b);
      b->col = target_color();
      b->closest = 0;
    }
  } else {
    /* turn target into boid */
    if(has_more_than_one(targets)){
      boid_iterator b = pick_random_target();
      boids.splice(boids.end(), targets, b);
      b->closest = &*pick_random_target();

      // do not let boids follow other boids 
      BOOST_FOREACH(boid& x, boids) {
        if (x.closest == &*b) {
          x.closest = &*pick_random_target();
        }
      }
      b->col = normal_color();
    }
  }
}

void mutate_parameter(float &param){
  param *= 0.75+my_frandom(0.5);
}

void state_impl::random_small_change(){
  int which = 0;

  which = my_random(11);

  if (++rsc_call_depth > 10) {
    rsc_call_depth--;
    return;
  }
  
  switch(which) {
  case 0:
    /* acceleration */
    mutate_parameter(max_accelleration);
    break;

  case 1:
    /* target acceleration */
    mutate_parameter(target_accelleration);
    break;

  case 2:
    /* velocity */
    mutate_parameter(max_velocity);
    break;

  case 3: 
    /* target velocity */
    mutate_parameter(target_velocity);
    break;

  case 4:
    /* noise */
    mutate_parameter(noise);
    break;

  case 5:
    /* min_velocity_multiplier */
    mutate_parameter(min_velocity_multiplier);
    break;
    
  case 6:
    break;
  case 7: {
    /* target to boid */
    int count = (int)std::abs(my_nrandom(0,targets.size()/10));
    while(count--)
      mutate_boids(1);
    break;
  }

  case 8: {
    /* boid to target */
    int count = (int)std::abs(my_nrandom(0,boids.size()/10));
    while(count--)
      mutate_boids(0);

    break;
  }
  case 9:
    /* color scheme */
    break;

  default:
    random_small_change();
    random_small_change();
    random_small_change();
    random_small_change();
  }

  // Clamp values
  if (min_velocity_multiplier < 0.3)
    min_velocity_multiplier = 0.3;
  else if (min_velocity_multiplier > 0.9)
    min_velocity_multiplier = 0.9;
  if (noise < 0.01)
    noise = 0.01;
  if (max_velocity < 0.02)
    max_velocity = 0.02;
  if (target_velocity < 0.02)
    target_velocity = 0.02;
  if (target_accelleration > target_velocity*0.7)
    target_accelleration = target_velocity*0.7;
  if (max_accelleration > max_velocity*0.7)
    max_accelleration = max_velocity*0.7;
  if (target_accelleration > target_velocity*0.7)
    target_accelleration = target_velocity*0.7;
  if (max_accelleration < 0.01)
    max_accelleration = 0.01;
  if (target_accelleration < 0.005)
    target_accelleration = 0.005;

  rsc_call_depth--;
}

void state_impl::random_big_change()
{
  int which = my_random(4);
  
  if (++rbc_call_depth > 3) {
    rbc_call_depth--;
    return;
  }

  switch(which) {
  case 0:
    /* trail length */
    break;

  case 1:  
    /* Whee! */
    random_small_change();
    random_small_change();
    random_small_change();
    random_small_change();
    random_small_change();
    random_small_change();
    random_small_change();
    random_small_change();
    break;

  case 2:
    init_boids();
    break;
    
  case 3:
    pick_new_targets();
    break;
    
  default:
    boid_iterator b = pick_random_target();
    b->pos += vector(my_frandom(x_max/4)-x_max/8,
                     my_frandom(y_max/4)-y_max/8);
    /* updateState() will fix bounds */
    break;
  }

  rbc_call_depth--;
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
 , target_velocity(0.03)
 , target_accelleration(0.02)
 , max_velocity(0.05)
 , max_accelleration(0.03)
 , min_velocity_multiplier(0.5)
 , noise (.05)
 , change_probability(0.04)
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

  init_boids();
  init_time();

  if (change_probability > 0) {
    for (int i = my_random(10, 5); i >= 0; i--) {
      random_small_change();
    }
  }
}

boid_iterator
state_impl::pick_random_target()  {
  assert(!targets.empty());
  size_t i = my_random(targets.size());
  return boost::next(targets.begin(), i);
};

boid_iterator
state_impl::pick_random_boid() {
  assert(!boids.empty());
  size_t i = my_random(boids.size());
  return boost::next(boids.begin(), i);
};

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

  if (true || draw_end > draw_start+0.5) {
    
    if (my_frandom(1.0) < change_probability) random_small_change();
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
