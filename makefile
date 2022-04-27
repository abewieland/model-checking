CXXFLAGS := -Wall -std=c++11
PROGS = msg

all: $(PROGS)

%.o: %.cc
	g++ $(CXXFLAGS) -c $^ -o $@

%: %.o simple.o
	g++ $(CXXFLAGS) $(LDLIBS) -o $@

clean:
	rm -rf *.o $(PROGS)

.PHONY: all clean

PRECIOUS: *.o
