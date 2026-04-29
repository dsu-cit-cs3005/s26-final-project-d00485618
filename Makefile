# Compiler
CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -fPIC -g

# Targets
all: RobotWarz

RobotBase.o: RobotBase.cpp RobotBase.h
	$(CXX) $(CXXFLAGS) -c RobotBase.cpp

RobotWarz: RobotWarz.cpp Arena.cpp RobotBase.o
	$(CXX) $(CXXFLAGS) RobotWarz.cpp Arena.cpp RobotBase.o -ldl -o RobotWarz

clean:
	rm -f *.o RobotWarz *.so
