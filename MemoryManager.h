#include <iostream>
#include <functional>
#include <vector>
#include <climits>
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>
using namespace std;



// Block Class
class Block {
private:
    int size;
    int startPos;
    bool isHole;
public:
    Block(int size, int startPos, bool isHole);
    int getSize();
    int getStartPos();
    bool getIsHole();
    void setSize(int size);
    void setStartPos(int startPos);
    void setIsHole(int isHole);
};

// Memory Manager Class
class MemoryManager {
private:
    unsigned wordSize;
    function<int(int, void*)> allocator;
    void* memoryStart = nullptr;
    unsigned memoryLimit = 0;
    bool isInit = false;
public:
    MemoryManager(unsigned wordSize, function<int(int, void*)> allocator);
    ~MemoryManager();
    void initialize(size_t sizeInWords);
    void shutdown();
    void* allocate(size_t sizeInBytes);
    void free(void* address);
    void setAllocator(function<int(int, void*)> allocator);
    int dumpMemoryMap(char* filename);
    void* getList();
    void* getBitmap();
    unsigned getWordSize();
    void* getMemoryStart();
    unsigned getMemoryLimit();
};

// Allocator Functions
int bestFit(int sizeInWords, void* list);
int worstFit(int sizeInWords, void* list);