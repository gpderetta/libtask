#include "gl.hpp"
#include <SDL/SDL_opengl.h>
#include <GL/gl.h>
#include <GL/glu.h>

namespace eb {
namespace gl {
        
  /* function to release/destroy our resources and restoring the old
   * desktop */
~graphic_context::graphic_context() {
  /* Clean up our textures */
  glDeleteTextures( 1, &texture[0] );
  
  /* clean up the window */
  SDL_Quit( );
  
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
  gluPerspective( 45.0f, ratio, 0.1f, 200.0f );
        
  /* Make sure we're chaning the model view and not the projection */
  glMatrixMode( GL_MODELVIEW );
        
  /* Reset The View */
  glLoadIdentity( );
}

/* function to load in bitmap as a GL texture */
texture graphic_context::load_textures(const std::string& texture) {
  SDL_Surface *texture_image = {
    SDL_LoadBMP( texture.c_str() )  };
  if (texture_image)
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
  return result;
}

graphic_context::graphic_context(){
  load_texture();
            
  // enable smooth shading 
  glShadeModel( GL_SMOOTH );
  // set the background black 
  glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
  // depth buffer setup 
  glClearDepth( 1.0f );
  glDisable( GL_DEPTH_TEST );
  // enable blending 
  glEnable( GL_BLEND );
  glBlendFunc( GL_SRC_ALPHA, GL_ONE );
  glHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST );
  glHint( GL_POINT_SMOOTH_HINT, GL_NICEST );
  glEnable( GL_TEXTURE_2D );

}

void graphic_context::bind_texture(texture const& tex) {
  glBindTexture( GL_TEXTURE_2D, tex.id );
}

void
graphic_context::reset() {
  glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );         glLoadIdentity( );
}

void
graphic_context::swap_buffers() {
  /* Draw it to the screen */
  SDL_GL_SwapBuffers( );
}
        
// draw a square centered around p using the
// currently bound texture.
void draw_dot(const graphic_context&,
              const position p, 
              const color c = color(1,1,1),
              const float alpha = 1.0f,
              const float radius = 0.5f) {
  const float x = p.x();
  const float y = p.y();
  float z = 0;
  glColor4f( color.r,
             color.g,
             color.b,
             alpha );
        
  /* Build Quad From A Triangle Strip */
  glBegin( GL_TRIANGLE_STRIP );
  /* Top Right */
  glTexCoord2d( 1, 1 );
  glVertex3f( x + radius, y + radius, z );
  /* Top Left */
  glTexCoord2d( 0, 1 );
  glVertex3f( x - radius, y + radius, z );
  /* Bottom Right */
  glTexCoord2d( 1, 0 );
  glVertex3f( x + radius, y - radius, z );
  /* Bottom Left */
  glTexCoord2d( 0, 0 );
  glVertex3f( x - radius, y - radius, z );
  glEnd( );
}
        
template<typename T>
draw_scene (T const& x) {
  gl::reset();
  draw(x);
  gl::swap_buffers();
};
}
}
