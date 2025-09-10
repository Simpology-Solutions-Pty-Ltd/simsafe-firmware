CXX = g++

CXXFLAGS = -std=c++20 -Wall -Werror -O0

LIBS = -lpqxx -lpq -lgpiod -lrt

DEPENDENCIES = src/main.cpp src/database.cpp src/communication.cpp
SOURCES = src/main.cpp
OBJECTS = $(SOURCES:.cpp=.o)

main: $(OBJECTS)
	$(CXX) $(OBJECTS) -o main.out $(LIBS)

src/main.o: $(DEPENDENCIES)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) main.out

.PHONY: clean