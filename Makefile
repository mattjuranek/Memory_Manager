libMemoryManager.a: MemoryManager.o 
	g++ -std=c++17 -o MemoryManager.o -c MemoryManager.cpp
	ar cr libMemoryManager.a MemoryManager.o
clean:
	rm -f MemoryManager.o libMemoryManager.a