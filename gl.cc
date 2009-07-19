#include "gl.hpp"
#include <iostream>
#include <SDL/SDL.h>
#include <SDL/SDL_opengl.h>
#include <GL/gl.h>
#include <GL/glu.h>
//#include <GL/glu.h>

namespace eb {
namespace gl {
        
  /* function to release/destroy our resources and restoring the old
   * desktop */

#define GLERR()                                     \
  do {                                              \
    GLenum err = glGetError();                      \
    if(err == GL_NO_ERROR) break;                   \
    std::cerr << (char*)gluErrorString(err)<<" at "<< __FILE__<<":"<<__LINE__<<std::endl; \
  } while(true);                                    \


graphic_context::~graphic_context() {
  /* Clean up our textures */
  glDeleteTextures( 1, &tex.id );
  
}

/* function to reset our viewport after a window resize */
void
graphic_context::resize_window( int width, int height ){
  /* Protect against a divide by zero */
  if ( height == 0 )
    height = 1;
  /* Height / width ration */
  GLfloat ratio =
    ( GLfloat )width /
    ( GLfloat )height;
        
  /* Setup our viewport. */
  glViewport( 0, 0, ( GLint )width, ( GLint )height );
        
  /* change to the projection matrix and set our viewing volume. */
  glMatrixMode( GL_PROJECTION );
  glLoadIdentity( );
        
  /* Set our perspective */
  //gluPerspective( 45.0f, ratio, 0.1f, 200.0f );
  glOrtho(0, width, height, 0, -1, 1);
        
  /* Make sure we're chaning the model view and not the projection */
  glMatrixMode( GL_MODELVIEW );
        
  /* Reset The View */
  glLoadIdentity( );
  GLERR();
}

/* function to load in bitmap as a GL texture */
texture graphic_context::load_textures(const std::string& filename) {
  SDL_Surface *texture_image = {
    SDL_LoadBMP( filename.c_str() )  };
  if (texture_image == 0)
    throw "cannot load texture";

  texture result ;
  glGenTextures( 1, &result.id );
  glBindTexture( GL_TEXTURE_2D, result.id );
  glTexImage2D( GL_TEXTURE_2D, 0, 3,
                texture_image->w,
                texture_image->h, 0, GL_BGR,
                GL_UNSIGNED_BYTE,
                texture_image->pixels );

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR );
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                  GL_LINEAR );
        
  SDL_FreeSurface(texture_image);
  GLERR();
  return result;
}

graphic_context::graphic_context() {
  tex.id = -1;
  tex = load_textures();
  // enable smooth shading 
  glShadeModel( GL_SMOOTH );
  // set the background black 
  glClearColor( .0f, .0f, 0.0f, 0.0f );
  // depth buffer setup 
  glClearDepth( 1.0f );
  glDisable( GL_DEPTH_TEST );
  // enable blending 
  glEnable( GL_BLEND );
  glBlendFunc( GL_SRC_ALPHA, GL_ONE );
  glHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST );
  glHint( GL_POINT_SMOOTH_HINT, GL_NICEST );
  glEnable( GL_TEXTURE_2D );

  bind_texture(tex);
  GLERR();
}

void graphic_context::bind_texture(texture const& tex)const {
  std::cerr << tex.id << std::endl;
  glBindTexture( GL_TEXTURE_2D, tex.id );
  GLERR();
}

void
graphic_context::reset() {
  glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
  glLoadIdentity( );
  GLERR();
}

void
graphic_context::swap_buffers() {
  /* Draw it to the screen */
  SDL_GL_SwapBuffers( );
}
        
// draw a square centered around p using the
// currently bound texture.
void draw_dot(const graphic_context&ctx,
              const point p, 
              const color c,
              const float alpha,
               float radius) {

  const float x = p.x;
  const float y = p.y;
  const float z = 0;
  glColor4f( c.r,
             c.g,
             c.b,
             alpha );

  /* Build Quad From A Triangle Strip */
  glBegin( GL_TRIANGLE_STRIP );
  /* Top Right */
  glTexCoord2d( 1, 1 );
  glVertex2f( x + radius, y + radius);
  /* Top Left */
  glTexCoord2d( 0, 1 );
  glVertex2f( x - radius, y + radius );
  /* Bottom Right */
  glTexCoord2d( 1, 0 );
  glVertex2f( x + radius, y - radius );
  /* Bottom Left */
  glTexCoord2d( 0, 0 );
  glVertex2f( x - radius, y - radius );
  glEnd( );
  GLERR();
}
        
}
}
