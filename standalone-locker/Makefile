CXX = g++

CXXFLAGS = -std=c++20 -Wall -Werror

LIBS = -lpqxx -lpq lib/libcserialport.a

SOURCES = src/main.cpp
OBJECTS = $(SOURCES:.cpp=.o)

main: $(OBJECTS)
	$(CXX) $(OBJECTS) -o main.out $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) main.out

.PHONY: clean