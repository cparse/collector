EXE=tests
SRC=$(EXE).cpp catch.cpp
OBJ = $(SRC:.cpp=.o)

LD ?= ld
CXX ?= g++
CFLAGS = -std=c++11 -Wall -pedantic
DEBUG = -g #-DDEBUG

$(EXE): $(OBJ); $(CXX) $(CFLAGS) $(DEBUG) $(OBJ) -o $(EXE)
%.o: %.cpp *.h; $(CXX) $(CFLAGS) $(DEBUG) -c $< -o $@ $(DEBUG)

test: $(EXE); ./$(EXE) $(args)

check: $(EXE); valgrind --leak-check=full ./$(EXE) $(args)

simul: $(EXE); cgdb ./$(EXE) $(args)

clean: ; rm -rf $(EXE) $(OBJ)
