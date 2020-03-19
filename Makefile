all: ycsb.cpp MurmurHash3.cpp CoCuckoo.cpp union_find.cpp hash.c
	g++ -std=c++11 -gdwarf-2 ycsb.cpp MurmurHash3.cpp CoCuckoo.cpp union_find.cpp hash.c  -o ./myTest -lm -lpthread

clean:
	rm myTest 
	rm -rf myTest.dSYM

