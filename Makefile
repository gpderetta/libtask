FEATURE_FLAGS= --std=c++0x
DEVEL_FLAGS= -W -Wall -O3 -mfpmath=sse -march=native -ffast-math -fopenmp -g 

INCLUDE=
CXX=g++-4.6
CPPFLAGS=  $(INCLUDE) 
CXXFLAGS ?= $(FEATURE_FLAGS) $(DEVEL_FLAGS)
LDFLAGS=       `sdl-config --cflags --libs`

BOOST_PO_LIB=boost_program_options

PROGRAMS=swarm

TESTS=match_test\
	switch_test \
	mpsc_queue_test  \

swarm_SOURCES=		\
	swarm.cc\
	particle.cc\
	gl.cc\
	controls.cc\
	logic.cc\

swarm_LIBS=	\
	GL\
	GLU\


include Makefile.common



