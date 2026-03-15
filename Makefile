##
# Data Skipping Programming Assignment
#

CXX ?= g++
CXXFLAGS := -std=c++23 -g -Wall 

INCLUDES := code/Parameters.hpp code/User.hpp code/FileUtils.hpp

build: code/main.cpp $(INCLUDES)
	$(CXX) $(CXXFLAGS) code/main.cpp -o code/main.out

run_frequent: build
	./code/main.out 3 1 data/frequent

run_normal: build
	./code/main.out 1 5 data/normal

run_all: build
	@echo "=== frequent (f_a=3, f_s=1) ==="
	./code/main.out 3 1 data/frequent
	@echo ""
	@echo "=== normal (f_a=1, f_s=5) ==="
	./code/main.out 1 5 data/normal

generate_data:
	cd data && pip install numpy && python generate_all.py eval.json

submit: clean
	rm -f data/frequent.* data/normal.* data/*.data data/*.query
	rm -rf data/.venv
	zip -r submission.zip . -x ".git/*" "*.dSYM/*" "*.zip"
	@echo "Created submission.zip — submit to Web Learning."

clean:
	rm -f code/main.out
	rm -rf code/*.dSYM
.PHONY: clean run_frequent run_normal run_all generate_data submit build
