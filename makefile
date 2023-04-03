FLAGS = -g -std=c++17 -Wall -Wextra -pedantic -Iinclude#-stdlib=libc++
MAC = -std=c++17 -stdlib=libc++
LDFLAGS		=  -ldl -pthread

EXECUTABLE = k-nn
COMMIT = 1
EXTRA = data.hpp
TESTFILES = *.dat

all: $(EXECUTABLE)

$(EXECUTABLE): $(EXECUTABLE).o
	g++ $(EXECUTABLE).o -o $(EXECUTABLE) $(LDFLAGS)

$(EXECUTABLE).o: $(EXECUTABLE).cpp $(EXECUTABLE).hpp $(EXTRA)
	g++ $(FLAGS) -c $(EXECUTABLE).cpp


valgrind:
	valgrind --tool=memcheck --leak-check=yes ./$(EXECUTABLE)
clean:
	rm -f *.o $(EXECUTABLE) $(TESTFILES)
