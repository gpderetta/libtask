#include "swarm.hpp"
#include "color.hpp"
#include "forwarding.hpp"
#include <functional>
#include "controls.hpp"
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
#include "macros.hpp"
namespace eb {namespace swarm{
template<typename T>
T square(T x) { return x*x; }

real absclamp(real x, real min, real max) {
    return x >= 0
        ? std::min(std::max(x, min), max)
        : std::max(std::min(x,-min),-max)
        ;
}

template<class T>
T absclamp(T val, real min, real max) {
    real norm = norm_2(val);
    real scaled_norm = absclamp(norm, min, max);
    return val * (scaled_norm / norm) ;
}


static const int  num_charges = 3;
typedef std::array<real, num_charges> charges_t;



const int max_fps = 40; 
const int min_fps =  16;

const real desired_dt =  0.1;

struct boid;
typedef std::function<void(boid&)> action_t;
struct boid {
    boid(action_t action, 
         point position,
         vector velocity,
         color c, float mass, charges_t const& charges)
        : action(action)
        , m_life(0)
        , m_color(c)
        , m_mass(mass)   
        , m_charges(charges)
        , m_position(position)
        , m_old_force()
        , m_force()
        , m_velocity(velocity)
    {   }

    action_t action;
    real m_life;
    color m_color;
    real m_mass;
    charges_t m_charges;
    point m_position;
    vector m_old_force;
    vector m_force;
    vector m_velocity;


};

struct spring_arm {
    spring_arm(int a, int b,
               real len,
               real k,
               real c) 
        : a_index(a)
        , b_index(b)
        , arm_len(len)
        , spring_constant(k)
        , damp_factor(c) {}
    int a_index;
    int b_index;
    real arm_len;  
    real spring_constant;
    real damp_factor;
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

color random_boid_color() {
    return { my_frandom(1., 0.),
            my_frandom(1., 0.),
            my_frandom(1., 0.),
            0.5};
}

template<class T>
struct signature2tuple;

template<class A, class T, class...Args>
struct signature2tuple<T(A::*)(boid&, Args...) const> {
    typedef std::tuple<Args...> type;
};

template<class...Args>
struct mytuple {
    mytuple(Args... x)
        : tup(x...) {}

    template<class F, int... I>
    action_t do_bind(F f, gpd::details::index_tuple<I...>) const 
    { return std::bind(f, std::placeholders::_1, std::get<I>(tup)...); };
    

    std::tuple<Args...> tup;

    template<class F>
    friend auto operator%(F f, mytuple t) -> action_t
    { return t.do_bind(f, gpd::details::indices(t.tup) ); }
} ;

template<class...Args>
auto with(Args... args) -> mytuple<Args...> {
    return mytuple<Args...>(args...);
};

void nil(boid&) {}


struct state_impl {
    state_impl(int width, int height);
    unsigned long update(world&);
private:
    void init_boids(int positive = 20, int negative = 20, int neutral = 0);
    void draw_boids(world&);
    void check_limits(boid&b) const;
    void update_state();
    void verlet_step_a(real);
    void verlet_step_b(real);
    void compute_gravitational_forces();
    void compute_elastic_forces();
    void init_time();
    double get_time() const;


    struct world_t {
        world_t() 
            : constants{ 0.0001, 0.000001, -0.000001 }
            , max_speed(0.3)
            , damp(0.8)
            , viscosity(0.)
            , min_distance(0.001)
        {}

        real constants[num_charges];
        real max_speed;
        real damp; 
        real viscosity; 
        real min_distance;

        real mass_gamma(vector v) const {
            return max_speed / std::sqrt(std::max(0.00001, square(max_speed) - inner_prod(v)));
        }

    };

    world_t m_world;

    unsigned long delay;
    int width;
    int height;
    real x_max; // 1.0
    real y_max; // height/width
    real z_max;

    real dt;
    real noise;

    int rsc_call_depth;
    int rbc_call_depth;

    real draw_fps;
    real draw_time_per_frame, draw_elapsed;
    double draw_start, draw_end;
    int draw_cnt;
    int draw_sleep_count;
    int draw_delay_accum;
    int draw_nframes;

    std::vector<boid> boids;
    std::vector<spring_arm> springs;
    ::timeval startup_time;
};


void state_impl::init_boids(int , int , int ){
    boids.clear();
    // XXX

    struct {
        action_t action;
        int count;
        color c;
        real mass;
        charges_t charges;
    } values [] = {
//#include "blobs.m"
#include "sine.m"
    } ;

    for(auto x : values)
        for(int i = 0; i < x.count; ++i)
            boids.push_back
                (boid(x.action,
                      point(my_frandom(x_max*0.99, x_max*0.01),
                            my_frandom(y_max*0.99, y_max*0.01),
                            0),
                      vector(0,0,0),
                      x.c, x.mass, x.charges))
                ;
    // springs.push_back(spring_arm(0, 1, 0.1, 8, 10));
    // springs.push_back(spring_arm(0, 2, 0.13, 8, 10));
    // springs.push_back(spring_arm(0, 3, 0.16, 8, 10));
    // springs.push_back(spring_arm(0, 4, 0.19, 8, 10));
    //springs.push_back(spring_arm(1, 3, 0.5, 10, 6));
    //springs.push_back(spring_arm(2, 3, 0.5, 10, 6));
    //  springs.push_back(spring_arm(3, 4, 0.5, 10, 6));
    //springs.push_back(spring_arm(4, 0, 0.5, 10, 6));

    // for(int i = 0; i < positive;++i) {
    //     boids.push_back(
    //         create_random_boid(
    //             positive_color(), 1., 1));
    // }

    // for(int i = 0; i < negative;++i) {
    //     boids.push_back(
    //         create_random_boid(
    //             negative_color(), 1, -1));
    
    // }

    // for(int i = 0; i < neutral;++i) {
    //     boids.push_back(
    //         create_random_boid(
    //             neutral_color(), 1, 0));  
    // }

    // for(int i = 0; i < anti_neutral;++i) {
    //     boids.push_back(
    //         create_random_boid(
    //             anti_neutral_color(), -1, 0));  
    // }

}

struct boid_parameters {
    real dt;
    real max_velocity;
    real noise;
    int change_probability;
};


void state_impl::draw_boids(world&w) {
    BOOST_FOREACH(boid& b, boids) {
        gl::draw_dot(w, b.m_position, b.m_color, 5.);
    }
}



void state_impl::check_limits(boid& b) const { 
    b.m_velocity = absclamp(b.m_velocity, 0, m_world.max_speed);
    bool bounce = false;
    /* check limits on targets */
    if (b.m_position.x < 0) {
        bounce = true;
        b.m_position.x *= -1  ;
        b.m_velocity.x *= -1  ;
    } else if (b.m_position.x >= x_max) {
        bounce = true;
        b.m_position.x = 2*x_max - b.m_position.x ;
        b.m_velocity.x *= -1  ;
    }
    if (b.m_position.y < 0) {
        bounce = true;
        b.m_position.y *= -1 ;
        b.m_velocity.y *= -1  ;
    } else if (b.m_position.y >= y_max) {
        bounce = true;
        b.m_position.y = 2*y_max - b.m_position.y;
        b.m_velocity.y *= -1  ;
    }
    if (b.m_position.z < 0) {
        bounce = true;
        b.m_position.z *= -1  ;
        b.m_velocity.z *= -1  ;
    } else if (b.m_position.z >= z_max) {
        bounce = true;
        b.m_position.z = 2*z_max - b.m_position.z ;
        b.m_velocity.z *= -1  ;
    }

    if(bounce)
        b.m_velocity *= m_world.damp;
}


#include <xmmintrin.h>


vector limit_magnitude(vector v, real max) {
    real m = norm_2(v);
    if(m > max) {
        real ratio = max/m;
        v *=ratio;
    }
    return v;
}

void state_impl::verlet_step_a(real dt) {
    for(auto &b: boids) {
        check_limits(b);
        real gamma = m_world.mass_gamma(b.m_velocity);

        b.m_position +=  b.m_velocity * dt 
            + (b.m_force * std::pow(dt, 2) ) / (2 * gamma * b.m_mass);

        b.m_old_force = b.m_force;

        check_limits(b);
        b.m_force = -b.m_velocity * m_world.viscosity ;
    }
}

void state_impl::verlet_step_b(real dt) {
    for(auto& b:  boids) {
        real gamma = m_world.mass_gamma(b.m_velocity);

        b.m_velocity 
            += (b.m_force + b.m_old_force) * dt / ( 2 * gamma * b.m_mass);
    }
}


vector distance(point a, point b) {
    return a-b; //absclamp(a-b, min_distance, 1);
}

template<class B, class T>
vector compute_gravitational_force(const B& b, const T& t, state_impl::world_t& world){
    vector dist = distance(t.m_position, b.m_position);
    real norm = norm_2(dist)  ;
    vector dir = dist / norm;
    vector force {};

    for(int i = 0; i < num_charges; ++i) {
        force += dir * std::pow(norm + world.min_distance,-2) 
            * ( world.constants[i] * t.m_charges[i] * b.m_charges[i] ) ;
    }
    
    return force;
}

void state_impl::compute_gravitational_forces() {
    // Gravitational forces
    auto buttons = get_mouse_buttons();
    struct {
        point m_position;  
        real m_charges[num_charges];

    } input  = {
        eb::get_mouse() ,
        { 50. * buttons[0] * ( buttons[1]? -1 : 1),
          0,
           1000. * buttons[2] * ( buttons[1]? -1 : 1) }
    };

    int size = (int)boids.size();
#pragma omp parallel for
    for(int i = 0; i < size; ++i) {
        boid& b = boids[i];
        for(auto& t: boids) {
            if(&t==&b) continue;
            b.m_force += compute_gravitational_force(b, t, m_world);
        }    
        b.m_force += compute_gravitational_force(b, input, m_world);
    }
}

void state_impl::compute_elastic_forces() {
    BOOST_FOREACH(spring_arm& s, springs) {
        boid& a = boids[s.a_index];
        boid& b = boids[s.b_index];
    
        vector dist = distance(a.m_position, b.m_position); // points in a direction
        real norm = norm_2(dist);
        vector dir = dist / norm;
        real stretch = norm - s.arm_len;
        // Looke Law
        a.m_force += -dir * 0.5 * stretch * s.spring_constant ;
        b.m_force +=  dir * 0.5 * stretch * s.spring_constant ;
    
        // Apply damp factor on dir component of velocity
        a.m_force +=   dir * inner_prod(a.m_velocity, -dir) * s.damp_factor ;
        b.m_force +=   -dir * inner_prod(b.m_velocity, dir) * s.damp_factor ;
        //std::cerr << stretch <<": "<< (s.damp_factor / (2 * sqrt(s.spring_constant * b.m_mass))) << "\n";
    }
}

void state_impl::update_state() { 
    dt = 0.005;

    int oldMXCSR = _mm_getcsr(); //read the old MXCSR setting
    int newMXCSR = oldMXCSR | 0x8040; // set DAZ and FZ bits
    _mm_setcsr( newMXCSR ); //write the new MXCSR setting to the MXCSR
  
    const int iterations = 10 ;
    for(int i = 0; i < iterations; ++i) {
        for(auto& b: boids) {
            b.action(b);
            //         std::cerr<<b.m_position<<":"<<b.m_velocity<<":"<<b.m_force<<"\n";
        }
        verlet_step_a(dt/iterations);
        compute_gravitational_forces();
        compute_elastic_forces();
        verlet_step_b(dt/iterations);
    }

    _mm_setcsr( oldMXCSR ); 
}

template<typename List>
bool has_more_than_one(List const& l)  {
    return !l.empty() && boost::next(l.begin()) != l.end();
}

void state_impl::init_time()
{
    gettimeofday(&startup_time, NULL);
}

double state_impl::get_time() const
{
    ::timeval t;
    real f;
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
    , y_max(real(height)/width)
    , z_max(1.0)
    , dt(0.3)
    , noise(2)
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
