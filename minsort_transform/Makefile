CXX = g++
CXXFLAGS = -O2 -Wall -DNDEBUG
#CXXFLAGS = -g

#CXX = clang++
#CXXFLAGS = -O3 -Wall

all: minsort_transform

minsort_transform: minsort_transform.cpp esa.hxx sais.hxx
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	$(RM) *.o minsort_transform

install: all
	cp minsort_transform $(HOME)/bin/
