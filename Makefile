DEVEL_FLAGS= -W -Wall -O3 -mfpmath=sse -march=pentium-m -ffast-math	-g \

INCLUDE=

CPPFLAGS=  $(INCLUDE) 
CXXFLAGS ?= $(DEVEL_FLAGS)
LDFLAGS=  	`sdl-config --cflags --libs`

BOOST_PO_LIB=boost_program_options

PROGRAMS=swarm

swarm_SOURCES=		\
	swarm.cc\
	particle.cc\
	gl.cc\

swarm_LIBS=	\
	GL\
	GLU\


include Makefile.common

