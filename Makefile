FEATURE_FLAGS= --std=c++0x
DEVEL_FLAGS= -W -Wall -O0 -mfpmath=sse -march=native -ffast-math -fopenmp -g  -mno-red-zone  -fnon-call-exceptions -fasynchronous-unwind-tables 

INCLUDE=
CXX=g++-4.6
CPPFLAGS=  $(INCLUDE) 
CXXFLAGS ?= $(FEATURE_FLAGS) $(DEVEL_FLAGS)
LDFLAGS=       `sdl-config --cflags --libs`

BOOST_PO_LIB=boost_program_options
BOOST_SYS_LIB=boost_system

PROGRAMS=swarm

TESTS=match_test\
	switch_test \
	mpsc_queue_test  \
	throw_test   \
	forwarding_test \
	future_test \

swarm_SOURCES=		\
	swarm.cc\
	particle.cc\
	gl.cc\
	controls.cc\

swarm_LIBS=	\
	GL\
	GLU\

asio_test_LIBS=$(BOOST_SYS_LIB)


include Makefile.common



