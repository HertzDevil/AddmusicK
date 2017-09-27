CXX = g++-7
CXXFLAGS = -Wall -pedantic -std=c++17 -O3
LDFLAGS = -lstdc++fs -ldl

SRCS = *.cpp MML/*.cpp Music/*.cpp Binary/*.cpp asardll.c ../AM405Remover/AddMusic.cpp

all: addmusick

addmusick: $(SRCS) *.h Binary/*.h MML/*.h Music/*.h Utility/*.h
	$(CXX) $(CXXFLAGS) -c $(SRCS)
	$(CXX) -o addmusick *.o $(LDFLAGS)

clean:
	rm -rf *.o ./addmusick

.PHONY: all clean

#addmusick:
