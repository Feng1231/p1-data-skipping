##
# Data Skipping Programming Assignment
#

CXX ?= g++
CXXFLAGS := -std=c++23 -g -Wall 

INCLUDES := Parameters.hpp User.hpp FileUtils.hpp

build: main.cpp  $(INCLUDES)
	$(CXX) $(CXXFLAGS) main.cpp -o main.out

run_frequent: build
	./main.out 3 1 ../data/frequent

run_normal: build
	./main.out 1 5 ../data/normal

run_all: build
	@echo "=== frequent (f_a=3, f_s=1) ==="
	./main.out 3 1 ../data/frequent
	@echo ""
	@echo "=== normal (f_a=1, f_s=5) ==="
	./main.out 1 5 ../data/normal

generate_data:
	cd ../data && pip install numpy && python generate_all.py eval.json

clean:
	rm -f *.out
.PHONY: clean run_frequent run_normal run_all generate_data
