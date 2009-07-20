/*
 * This code was created by Jeff Molofee '99 
 * (ported to Linux/SDL by Ti Leggett '01)
 *
 * If you've found this code useful, please let me know.
 *
 * Visit Jeff at http://nehe.gamedev.net/
 * 
 * or for port-specific comments, questions, bugreports etc. 
 * email to leggett@eecs.tulane.edu
 */

#include "color.hpp"
#include "gl.hpp"
#include "swarm.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <SDL/SDL.h>
#include <vector>
namespace eb { 
/* screen width, height, and bit depth */
const size_t screen_width  = 1000;
const size_t screen_height = 1000;
const size_t screen_bpp    = 16;

/* Max number of particles */
const size_t max_particles = 1000;


int rainbow = true;    /* Toggle rainbow effect                              */

float slowdown = 2.0f; /* Slow Down Particles                                */
float xspeed;          /* Base X Speed (To Allow Keyboard Direction Of Tail) */
float yspeed;          /* Base Y Speed (To Allow Keyboard Direction Of Tail) */
float zoom = -40.0f;   /* Used To Zoom Out                                   */





static const color colors[] =
{
  color( 1.0f,  0.5f,  0.5f),
	color( 1.0f,  0.75f, 0.5f),
	color( 1.0f,  1.0f,  0.5f),
	color( 0.75f, 1.0f,  0.5f),
  color( 0.5f,  1.0f,  0.5f),
	color( 0.5f,  1.0f,  0.75f),
	color( 0.5f,  1.0f,  1.0f),
	color( 0.5f,  0.75f, 1.0f),
  color( 0.5f,  0.5f,  1.0f),
	color( 0.75f, 0.5f,  1.0f),
	color( 1.0f,  0.5f,  1.0f),
	color( 1.0f,  0.5f,  0.75f)
};

#if 0
struct particle_engine {

  particle_engine()
   : t0(), frames()
  { reset(); }

  void update_particles() {
    for ( int i = 0; i < m_particles.size(); ) {
      particle& p = m_particles[i];
      position pos = compute_position(p);
            
      for(int j = 0; j != forces.size(); ++i) {
        apply_force
          ( p,
            compute_interaction
            ( p,
              sources[j]
            )
          );
      }
      p.life -= 1;
            
      if ( p.life == 0 ) {
        swap(p, m_particles.back());
        m_particles.pop_back();
        gen_particle();
      } else {
        ++i;
      }
    }
  }

  friend void draw(particle_engine const& pe) {
    for ( int i = 0; i < pe.m_particles.size(); ++i) {
      particle& p = pe.m_particles[i];
      gl::draw_dot(position(p),
                   p.color,
                   p.life/particle::max_life);
    }
        
    compute_fps();
  }

  void compute_fps() {
        
    m_frames++;
    {
      int t = SDL_GetTicks();
      if (t - m_t0 >= 5000) {
        float seconds = (m_t - t0) / 1000.0;
        float fps = m_frames / seconds;
        printf("%d frames in %g seconds = %g FPS\n", m_frames, seconds, fps);
        m_t0 = t;
        m_frames = 0;
      }
    }
  }

  void gen_particle() {
    int icolor = (i + 1) / ( num_particles / 12 );
    float xi, yi, zi;
    vector dir =  {
      ( float )( ( rand( ) % 50 ) - 26.0f ) * 10.0f,
      ( float )( ( rand( ) % 50 ) - 25.0f ) * 10.0f
    };
    m_particles.push_back
      ( particle
        ( colors[icolor],
          make<point>(0,0),
          dir
        )
      );

  }

    
  void reset(size_t num_particles) {
    m_particles.clear();
    while(particles.size() != num_particles)
      gen_particle();
  }

  std::vector<particle> m_particles;
  int m_t0     = 0;
  int m_frames = 0;

};


/**
 * Container of particles.
 *  Models Updatable
 *
 * It forwards update commands to all sub nodes. Super is
 * forwarded as-is to every child node. So is 'world'.
 * 
 * Nodes that return true on update are removed.
 * 
 */ 
template<typename Particle>
struct particle_vector {
  typedef Particle particle;

  friend void swap(particle_vector& lhs, particle_vector& rhs) {
    lhs.m_particles.swap(rhs.m_particles);
  }

  template<typename Super>
  friend 
  bool update(particle_vector& self, Super& super, world& w ) {
    for(int i = 0; i != self.m_particles.size(); ++i) {
      particle& p = self.m_particles[i];
      bool dead = update(p, super, w);
      
      if ( dead ) {
        swap(p, m_particles.back());
        m_particles.pop_back();
        gen_particle();
      } else {
        ++i;
      }
    }
    return!m_particles.empty();
  }

  std::vector<particle> m_particles;
};

struct node_particle {

  point m_position;
  oolor m_color;
  vector m_speed;
  texture m_tex
  
  
  template<typename Super>
  bool update(node_particle& self, Super& super, world& w) {
    world.at(w, pos, new_state);
  }
};
#endif 





/* function to handle key press events */
void handleKeyPress( SDL_keysym *keysym, SDL_Surface *surface )
{
  switch ( keysym->sym )
	{
	case SDLK_ESCAPE:
    /* ESC key was pressed */
    exit( 0 );
    break;
	case SDLK_F1:
    /* F1 key was pressed
     * this toggles fullscreen mode
     */
    SDL_WM_ToggleFullScreen( surface );
    break;
	case SDLK_KP_PLUS:
    /* '+' key was pressed
     * this speeds up the particles
     */
    if ( slowdown > 1.0f )
      slowdown -= 0.01f;
    break;
	case SDLK_KP_MINUS:
    /* '-' key was pressed
     * this slows down the particles
     */
    if ( slowdown < 4.0f )
      slowdown += 0.01f;
	case SDLK_PAGEUP:
    /* PageUp key was pressed
     * this zooms into the scene
     */
    zoom += 0.01f;
    break;
	case SDLK_PAGEDOWN:
    /* PageDown key was pressed
     * this zooms out of the scene
     */
    zoom -= 0.01f;
    break;
	case SDLK_UP:
    /* Up arrow key was pressed
     * this increases the particles' y movement
     */
    if ( yspeed < 200.0f )
      yspeed++;
    break;
	case SDLK_DOWN:
    /* Down arrow key was pressed
     * this decreases the particles' y movement
     */
    if ( yspeed > -200.0f )
      yspeed--;
    break;
	case SDLK_RIGHT:
    /* Right arrow key was pressed
     * this increases the particles' x movement
     */
    if ( xspeed < 200.0f )
      xspeed++;
    break;
	case SDLK_LEFT:
    /* Left arrow key was pressed
     * this decreases the particles' x movement
     */
    break;
	case SDLK_KP8:
    /* NumPad 8 key was pressed
     * increase particles' y gravity
     */
    break;
	case SDLK_KP2:
    /* NumPad 2 key was pressed
     * decrease particles' y gravity
     */
    break;
	case SDLK_KP6:
    /* NumPad 6 key was pressed
     * this increases the particles' x gravity
     */
    break;
	case SDLK_KP4:
    /* NumPad 4 key was pressed
     * this decreases the particles' y gravity
     */
    break;
	case SDLK_TAB:
    /* Tab key was pressed
     * this resets the particles and makes them re-explode
     */
    //world.reset();
    break;
	case SDLK_RETURN:
    /* Return key was pressed
     * this toggles the rainbow color effect
     */
    break;
	case SDLK_SPACE:
    /* Spacebar was pressed
     * this turns off rainbow-ing and manually cycles through colors
     */
    break;
	default:
    break;
	}

  return;
}

}

int main( int argc, char **argv ){
  using namespace eb;
  /* Flags to pass to SDL_SetVideoMode */
  int videoFlags;
  /* main loop variable */
  int done = false;
  /* used to collect events */
  SDL_Event event;
  /* this holds some info about our display */
  const SDL_VideoInfo *videoInfo;
  /* whether or not the window is active */
  int isActive = true;
    
  /* initialize SDL */
  if ( SDL_Init( SDL_INIT_VIDEO ) < 0 )	{
    fprintf( stderr, "Video initialization failed: %s\n",
             SDL_GetError( ) );
    exit( 1 );
	}

  /* Fetch the video info */
  videoInfo = SDL_GetVideoInfo( );

  if ( !videoInfo )
	{
    fprintf( stderr, "Video query failed: %s\n",
             SDL_GetError( ) );
    exit( 1 );
	}

  /* the flags to pass to SDL_SetVideoMode                            */
  videoFlags  = SDL_OPENGL;          /* Enable OpenGL in SDL          */
  videoFlags |= SDL_GL_DOUBLEBUFFER; /* Enable double buffering       */
  videoFlags |= SDL_HWPALETTE;       /* Store the palette in hardware */
  videoFlags |= SDL_RESIZABLE;       /* Enable window resizing        */

  /* This checks to see if surfaces can be stored in memory */
  if ( videoInfo->hw_available )
    videoFlags |= SDL_HWSURFACE;
  else
    videoFlags |= SDL_SWSURFACE;

  /* This checks if hardware blits can be done */
  if ( videoInfo->blit_hw )
    videoFlags |= SDL_HWACCEL;

  /* Sets up OpenGL double buffering */
  SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );

  /* get a SDL surface */
  /* This is our SDL surface */
  SDL_Surface *surface = SDL_SetVideoMode( screen_width, screen_height, screen_bpp,
                                           videoFlags );

  /* Verify there is a surface */
  if ( !surface )
	{
    fprintf( stderr,  "Video mode set failed: %s\n", SDL_GetError( ) );
    exit( 1 );
	}

  /* Enable key repeat */
  if ( ( SDL_EnableKeyRepeat( 100, SDL_DEFAULT_REPEAT_INTERVAL ) ) )
	{
    fprintf( stderr, "Setting keyboard repeat failed: %s\n",
             SDL_GetError( ) );
    exit( 1 );
	}

  gl::graphic_context gctx;

  /* Resize the initial window */
  gctx.resize_window( screen_width, screen_height );

  swarm::state my_state(screen_width, screen_height);
  /* wait for events */
  while ( !done )
	{
    /* handle the events in the queue */

    while ( SDL_PollEvent( &event ) )
		{
      switch( event.type )
			{
			case SDL_ACTIVEEVENT:
        /* Something's happend with our focus
         * If we lost focus or we are iconified, we
         * shouldn't draw the screen
         */
        if ( event.active.gain == 0 )
          isActive = false;
        else
          isActive = true;
        break;
			case SDL_VIDEORESIZE:
        /* handle resize event */
        surface = SDL_SetVideoMode( event.resize.w,
                                    event.resize.h,
                                    16, videoFlags );
        if ( !surface )
				{
          fprintf( stderr, "Could not get a surface after resize: %s\n", SDL_GetError( ) );
          exit( 1 );
				}
        gctx.resize_window( event.resize.w, event.resize.h );
        break;
			case SDL_KEYDOWN:
        /* handle key presses */
        handleKeyPress( &event.key.keysym, surface );
        break;
			case SDL_QUIT:
        /* handle quit requests */
        done = true;
        break;
			default:
        break;
			}
		}

    gctx.reset();
    /* draw the scene */
    //if ( isActive )
    unsigned long sleep = my_state.update(gctx);
    gctx.swap_buffers();
    usleep (10000);

	}

  /* clean up the window */
  SDL_Quit( );
  
    /* clean ourselves up and exit */
  exit( 0 );

  /* Should never get here */
  return( 0 );
}




