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
       color c, float mass)
   : m_life(0)
   , m_color(c)
   , m_mass(mass)
   , m_position(position)
   , m_old_accelleration()
   , m_force()
   , m_velocity(velocity)
    {}


  float m_life;
  color m_color;
  float m_mass;
  point m_position;
  vector m_old_accelleration;
  vector m_force;
  vector m_velocity;


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
  void init_boids(int nboids = 2, int ntargets = 1);
  boid create_random_boid(color c, float mass) const;
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

boid state_impl::create_random_boid(color c, float mass) const {
  return boid(point(my_frandom(x_max),
                    my_frandom(y_max)),
              vector(my_frandom(max_velocity/2),
                     my_frandom(max_velocity/2)),
	      c, mass);
}

void state_impl::init_boids(int nboids, int ntargets){
  boids.clear();
  targets.clear();

  while(nboids--) {
    boids.push_back(
      create_random_boid(
	 normal_color(), -1));
    
  }

  while(ntargets--) {
    targets.push_back(create_random_boid(target_color(), 1));
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
    gl::draw_dot(w, b.m_position, b.m_color);
  }
  BOOST_FOREACH(boid& b, targets) {
    gl::draw_dot(w, b.m_position, b.m_color);
  }
}

void state_impl::check_limits(boid& b) const { 
  /* check limits on targets */
  if (b.m_position.x < 0) {
    /* bounce */
    b.m_position.x = -b.m_position.x;
    b.m_velocity.x = -b.m_velocity.x;
  } else if (b.m_position.x >= x_max) {
    /* bounce */
    b.m_position.x = 2*x_max-b.m_position.x;
    b.m_velocity.x = -b.m_velocity.x;
  }
  if (b.m_position.y < 0) {
    /* bounce */
    b.m_position.y = -b.m_position.y;
    b.m_velocity.y = -b.m_velocity.y;
  } else if (b.m_position.y >= y_max) {
    /* bounce */
    b.m_position.y = 2*y_max-b.m_position.y;
    b.m_velocity.y = -b.m_velocity.y;
  }
}


#include <xmmintrin.h>

const float G = 0.0004;
const float Gx = -0.00000;

    vector limit_magnitude(vector v, float max) {
      float m = norm_2(v);
      if(m > max) {
	float ratio = max/m;
	v *=ratio;
      }
      return v;
    }

void state_impl::update_state() {

  
  int oldMXCSR = _mm_getcsr(); //read the old MXCSR setting
  int newMXCSR = oldMXCSR | 0x8040; // set DAZ and FZ bits
  _mm_setcsr( newMXCSR ); //write the new MXCSR setting to the MXCSR
  
  max_velocity = 0.1;

  /* update boid state -- A */
  BOOST_FOREACH(boid&b, boids) {
    vector accelleration = b.m_force / b.m_mass;

    float v = norm_2(b.m_velocity);
    b.m_position 
      += b.m_velocity*dt 
      + accelleration * 0.5 * std::pow(dt,2) ;

    b.m_velocity += accelleration * 0.5 * dt;
    b.m_old_accelleration = accelleration;
    // // if(v > max_velocity)
    // //   b.m_force += -b.m_velocity ;


  }
  
  BOOST_FOREACH(boid& b, boids) {
    BOOST_FOREACH(boid& t, targets) {
      vector dist = t.m_position - b.m_position;
      float norm_sq = std::max<float>(inner_prod(dist), 1);
      float inorm = inv_sqrt(norm_sq)  ;
      vector dir = dist * inorm;
      b.m_force +=  - dir * std::pow(inorm,2) *G * t.m_mass * b.m_mass ;
      b.m_force +=  dir * std::pow(inorm,2) *Gx * t.m_mass * b.m_mass;
    }
    //break;
    BOOST_FOREACH(boid& t, boids) {
      if(&t==&b) continue;
      vector dist = t.m_position - b.m_position;
      float norm_sq = std::max<float>(inner_prod(dist), 1);
      float inorm = inv_sqrt(norm_sq)  ;
      vector dir = dist * inorm;
      b.m_force +=  - dir * std::pow(inorm,2) *G *t.m_mass * b.m_mass;
      b.m_force +=  dir * std::pow(inorm,3) *Gx *t.m_mass * b.m_mass ;
    }

  }


  /* update target state */
  BOOST_FOREACH(boid& b, targets) {
    float theta = my_nrandom(0,2*M_PI);
    float accell = std::abs(my_nrandom(0, target_accelleration));
    vector accelleration = limit_magnitude(b.m_force,3) + 
      vector(cos(theta)*accell, 
	     sin(theta)*accell);
    float v = norm_2(b.m_velocity);
    b.m_velocity += accelleration*dt;
    vector old_vel = b.m_velocity;
    double ratio = 1;

    if (v > target_velocity)  {
      ratio = target_velocity/v;
    } 
    b.m_velocity *= ratio;
    accelleration = (b.m_velocity-old_vel)/dt;         
    vector acc = accelleration*0.5*square(dt);
    b.m_position += b.m_velocity*dt + acc;
    check_limits(b);
    b.m_force = vector();
  }

  /* update boid state -- B*/
  BOOST_FOREACH(boid&b, boids) {
    vector accelleration = b.m_force / b.m_mass;

     b.m_velocity += accelleration * 0.5 * dt;
    check_limits(b);
    b.m_force = vector();
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
  if (max_velocity < 5)
    max_velocity = 5;
  if (target_velocity < 0.0)
    target_velocity = 0.0;
  if (target_accelleration > target_velocity*0.7)
    target_accelleration = target_velocity*0.7;
  if (target_accelleration > target_velocity*0.7)
    target_accelleration = target_velocity*0.7;
  if (target_accelleration < 0.005)
    target_accelleration = 0.005;
  target_velocity = 0;
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
 , target_velocity(0.01)
 , target_accelleration(0.02)
 , max_velocity(0.036)
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
