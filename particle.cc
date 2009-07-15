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
 
#include <stdio.h>
#include <stdlib.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include "SDL.h"

/* screen width, height, and bit depth */
const size_t screen_width  = 640;
const size_t screen_height = 480;
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


struct particle_engine {

    particle_engine()
        : t0(), frames()
    { reset(); }

    void update_particles() {
        for ( int i = 0; i < m_particles.size(); ) {
            particle& p = m_particles[i];
            update_position(p);
            
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
                swap(p, particles.back());
                particles.pop_back();
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






/* function to handle key press events */
void handleKeyPress( SDL_keysym *keysym, SDL_Surface *surface )
{
    switch ( keysym->sym )
	{
	case SDLK_ESCAPE:
	    /* ESC key was pressed */
	    Quit( 0 );
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
	    if ( xspeed > -200.0f )
		xspeed--;
	    break;
	case SDLK_KP8:
	    /* NumPad 8 key was pressed
	     * increase particles' y gravity
	     */
	    for ( loop = 0; loop < max_particles; loop++ )
		if ( particles[loop].yg < 1.5f )
		    particles[loop].yg += 0.01f;
	    break;
	case SDLK_KP2:
	    /* NumPad 2 key was pressed
	     * decrease particles' y gravity
	     */
	    for ( loop = 0; loop < max_particles; loop++ )
		if ( particles[loop].yg > -1.5f )
		    particles[loop].yg -= 0.01f;
	    break;
	case SDLK_KP6:
	    /* NumPad 6 key was pressed
	     * this increases the particles' x gravity
	     */
	    for ( loop = 0; loop < max_particles; loop++ )
		if ( particles[loop].xg < 1.5f )
		    particles[loop].xg += 0.01f;
	    break;
	case SDLK_KP4:
	    /* NumPad 4 key was pressed
	     * this decreases the particles' y gravity
	     */
	    for ( loop = 0; loop < max_particles; loop++ )
		if ( particles[loop].xg > -1.5f )
		    particles[loop].xg -= 0.01f;
	    break;
	case SDLK_TAB:
	    /* Tab key was pressed
	     * this resets the particles and makes them re-explode
	     */
            world.reset();
	    break;
	case SDLK_RETURN:
	    /* Return key was pressed
	     * this toggles the rainbow color effect
	     */
	    rainbow = !rainbow;
	    delay = 25;
	    break;
	case SDLK_SPACE:
	    /* Spacebar was pressed
	     * this turns off rainbow-ing and manually cycles through colors
	     */
	    rainbow = false;
	    delay = 0;
	    col = ( ++col ) % 12;
	    break;
	default:
	    break;
	}

    return;
}



int main( int argc, char **argv )
{
GLuint loop;           /* Misc Loop Variable                                 */
GLuint col = 0;        /* Current Color Selection                            */
GLuint delay;          /* Rainbow Effect Delay                               */
GLuint texture[1];     /* Storage For Our Particle Texture                   */

    
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
    if ( SDL_Init( SDL_INIT_VIDEO ) < 0 )
	{
	    fprintf( stderr, "Video initialization failed: %s\n",
		     SDL_GetError( ) );
	    Quit( 1 );
	}

    /* Fetch the video info */
    videoInfo = SDL_GetVideoInfo( );

    if ( !videoInfo )
	{
	    fprintf( stderr, "Video query failed: %s\n",
		     SDL_GetError( ) );
	    Quit( 1 );
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
	    Quit( 1 );
	}

    /* Enable key repeat */
    if ( ( SDL_EnableKeyRepeat( 100, SDL_DEFAULT_REPEAT_INTERVAL ) ) )
	{
	    fprintf( stderr, "Setting keyboard repeat failed: %s\n",
		     SDL_GetError( ) );
	    Quit( 1 );
	}

    /* initialize OpenGL */
    if ( initGL( ) == false )
	{
	    fprintf( stderr, "Could not initialize OpenGL.\n" );
	    Quit( 1 );
	}

    /* Resize the initial window */
    resizeWindow( screen_width, screen_height );

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
				    Quit( 1 );
				}
			    resizeWindow( event.resize.w, event.resize.h );
			    break;
			case SDL_KEYDOWN:
			    /* handle key presses */
			    handleKeyPress( &event.key.keysym );
			    break;
			case SDL_QUIT:
			    /* handle quit requests */
			    done = true;
			    break;
			default:
			    break;
			}
		}

	    /* If rainbow coloring is turned on, cycle the colors */
	    if ( rainbow && ( delay > 25 ) )
		col = ( ++col ) % 12;

	    /* draw the scene */
	    if ( isActive )
		drawGLScene( );

	    delay++;
	}

    /* clean ourselves up and exit */
    Quit( 0 );

    /* Should never get here */
    return( 0 );
}
