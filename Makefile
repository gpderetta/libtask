FEATURE_FLAGS= --std=c++14
#DEVEL_FLAGS= -W -Wall -O0  -fopenmp -g
DEVEL_FLAGS= -W -Wall -g  -fopenmp -g -msse4.2 -march=native

SUPPRESS=1
INCLUDE=-I.
CXX=g++-4.9
CPPFLAGS=  $(INCLUDE) 
CXXFLAGS ?= $(FEATURE_FLAGS) $(DEVEL_FLAGS)

BOOST_PO_LIB=boost_program_options
BOOST_SYS_LIB=boost_system

PROGRAMS=
STATIC_LIBRARIES=libtask 

TESTS=match_test\
	continuation_test \
	mpsc_queue_test  \
	forwarding_test \
	future_test \
	asio_test \
	signature_test \
	pipe_test \
	benchmark_test\
	variant_test\
	dynamic_test\
	any_future_test\
	q_test\

pipe_test_LIBS=boost_regex
benchmark_test_LIBS=boost_timer\
	boost_system

libtask_SOURCES=\
	event.cpp\
	task.cpp\
	waiter.cpp\

asio_test_LIBS=\
	$(BOOST_SYS_LIB)\
	task\

future_test_LIBS=\
	task\

any_future_test_LIBS=\
	task\

include Makefile.common


