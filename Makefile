CXXFLAGS=-std=c++11 -O2 -g

bigint-test: bigint.hpp bigint-test.cpp
bigint-test.js: bigint.hpp bigint-test.cpp
	em++ -std=c++11 -o bigint-test.html bigint-test.cpp

test: bigint-test bigint-test.js
	./bigint-test
	node ./bigint-test.js

clean:
	rm bigint-test bigint-test.js bigint-test.html
