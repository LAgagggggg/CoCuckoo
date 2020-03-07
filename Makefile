all: myTest.cpp MurmurHash3.cpp CoCuckoo.cpp union_find.cpp
	g++ -std=c++11 -g myTest.cpp MurmurHash3.cpp CoCuckoo.cpp union_find.cpp  -o ./myTest

clean:
	rm myTest 
	rm -rf myTest.dSYM

