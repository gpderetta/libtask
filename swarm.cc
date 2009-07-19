#include "swarm.hpp"
#include "color.hpp"
#include <sys/time.h>
#include <cmath>
#include <cassert>
#include <list>
#include <iostream>
#include <boost/foreach.hpp>
#include <boost/next_prior.hpp>

namespace eb {namespace swarm{
template<typename T>
T square(T x) { return x*x; }

const int max_fps = 40; 
const int min_fps =  16;

const double desired_dt =  0.2;

struct boid;
typedef std::list<boid> boid_list;
typedef boid* boid_ptr;
typedef boid_list::iterator boid_iterator;

struct boid {
  boid(point position,
      vector velocity,
      color c,
      boid_ptr closest= 0)
   : pos(position)
   , vel(velocity)
   , col(c)
   , closest(closest) {}

  point pos;
  vector vel;
  color col;
  boid_ptr closest;
};

double frand(double max) {
  // XXX quick hack
  return (rand() * max)  / RAND_MAX;
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
  void init_boids(int nboids = 20, int ntargets = 3);
  boid create_random_boid(color c, boid_ptr target) const;
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
  boid_iterator get_random_target() ;
  boid_iterator get_random_boid() ;


  unsigned long delay;
  size_t width;
  size_t height;
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

boid state_impl::create_random_boid(color c, boid_ptr target) const {
  return boid(point(frand(x_max),
                   frand(y_max)),
             vector(frand(max_velocity/2),
                    frand(max_velocity/2)),
             c, target);
}

void state_impl::init_boids(int nboids, int ntargets){
  boids.clear();
  targets.clear();

  while(nboids--) {
    boids.push_back(
      create_random_boid(
        color(0,0,1),0));
    
  }

  while(ntargets--) {
    targets.push_back(create_random_boid(color(1,0,0),0));
  }
  BOOST_FOREACH(boid&b, boids)
    b.closest = &*get_random_target();
  

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
    b.closest = &*get_random_target();
}

void state_impl::add_boids(int num)
{
  assert(num > 0);
  while(num--)  {
    boid b = *get_random_boid();
    b.closest = &*get_random_target();
    boids.push_back(b);
  }
}

void state_impl::add_targets(int num)
{
  assert(num > 0);

  while(num--) {
    boid b = *get_random_target();
    b.closest = &*get_random_target();
    targets.push_back(b);
  }
}

float state_impl::min_velocity() const
{
  return max_velocity*min_velocity_multiplier;
}

void state_impl::draw_boids(world&w) {
  BOOST_FOREACH(boid& b, boids) {
    gl::draw_dot(w, b.pos*vector(width, width), b.col);
  }
  BOOST_FOREACH(boid& b, targets) {
    gl::draw_dot(w, b.pos*vector(width, width), b.col);
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
  // Change target to the first 5 boids and move them to the
  // end of list
  assert(!boids.empty());
  boid_iterator i = boids.begin();
  boid_iterator last = boost::prior(boids.end());
  for (int j = 0; j < 5; ) {
    /* update closests boid
       for the boid indicated by check_index */
    assert(!boids.empty());

    boid& b = *i;

    float min_distance = inner_prod(b.closest->pos - b.pos);
    BOOST_FOREACH(boid& t, targets){
      float distance = inner_prod(t.pos-b.pos);
      if (distance < min_distance) {
        b.closest = &t;
        min_distance = distance;
      }
    }
    j--;
    boids.splice(boids.end(), boids,i++);
    if(i == last) break;
  }
  
  /* update target state */
  BOOST_FOREACH(boid& b, targets) {
    double theta = frand(2*M_PI);
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
    point target = b.closest->pos + vector(frand(noise),
                                          frand(noise));
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
      boid_iterator b = get_random_boid();
      targets.splice(targets.end(), boids, b);
    }
  } else {
    /* turn target into boid */
    if(has_more_than_one(targets)){
      boid_iterator b = get_random_target();
      boids.splice(boids.end(), targets, b);
      b->closest = &*get_random_target();

      // do not let boids follow other boids 
      BOOST_FOREACH(boid& x, boids) {
        if (x.closest == &*b) {
          x.closest = &*get_random_target();
        }
      }
    }
  }
}

void mutate_parameter(float &param){
  param *= 0.75+frand(0.5);
}

void state_impl::random_small_change(){
  int which = 0;

  which = random()%11;

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
  case 7:
    /* target to boid */
    mutate_boids(1);
    break;

  case 8:
    /* boid to target */
    mutate_boids(0);
    mutate_boids(0);
    break;

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
  if (min_velocity_multiplier < 0.03)
    min_velocity_multiplier = 0.03;
  else if (min_velocity_multiplier > 0.09)
    min_velocity_multiplier = 0.09;
  if (noise < 0.01)
    noise = 0.01;
  if (max_velocity < 0.001)
    max_velocity = 0.001;
  if (target_velocity < 0.001)
    target_velocity = 0.001;
  if (target_accelleration > target_velocity*0.7)
    target_accelleration = target_velocity*0.7;
  if (max_accelleration > max_velocity*0.7)
    max_accelleration = max_velocity*0.7;
  if (target_accelleration > target_velocity*0.7)
    target_accelleration = target_velocity*0.7;
  if (max_accelleration < 0.001)
    max_accelleration = 0.001;
  if (target_accelleration < 0.0005)
    target_accelleration = 0.0005;

  rsc_call_depth--;
}

void state_impl::random_big_change()
{
  int which = random()%4;
  
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
    boid_iterator b = get_random_target();
    b->pos += vector(frand(x_max/4)-x_max/8,
                     frand(y_max/4)-y_max/8);
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
 , width(width)
 , height(height)
 , x_max(1.0)
 , y_max(float(height)/width)
 , dt(0.3)
 , target_velocity(0.03)
 , target_accelleration(0.02)
 , max_velocity(0.05)
 , max_accelleration(0.03)
 , min_velocity_multiplier(0.5)
 , noise (0.01)
 , change_probability(0.08)
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
    for (int i = random()%5+5; i >= 0; i--) {
      random_small_change();
    }
  }
}

boid_iterator
state_impl::get_random_target()  {
  assert(!targets.empty());
  size_t i = random()% targets.size();
  return boost::next(targets.begin(), i);
};

boid_iterator
state_impl::get_random_boid() {
  assert(!boids.empty());
  size_t i = random()% boids.size();
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

  if (draw_end > draw_start+0.5) {
    if (frand(1.0) < change_probability) random_small_change();
    if (frand(1.0) < change_probability*0.3) random_big_change();
    draw_elapsed = draw_end-draw_start;
	
    draw_time_per_frame = draw_elapsed/draw_nframes - delay*1e-6;
    draw_fps = draw_nframes/draw_elapsed;

    std::cout <<"elapsed:"<<draw_elapsed<<"\n";
    std::cout <<"fps: "<<draw_fps <<"secs per frame: "<<draw_time_per_frame <<" delay: "<<delay<<std::endl; 

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
