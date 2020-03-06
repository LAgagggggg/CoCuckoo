all: myTest.cpp MurmurHash3.cpp
	g++ -std=c++11 -g ./myTest.cpp ./MurmurHash3.cpp -o ./myTest

clean:
	rm myTest 
	rm -rf myTest.dSYM

