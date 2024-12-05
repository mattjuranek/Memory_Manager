#include "MemoryManager.h"



// Block Functions
Block::Block(int size, int startPos, bool isHole) {
    this->size = size;
    this->startPos = startPos;
    this->isHole = isHole;
}

int Block::getSize() {
    return size;
}

int Block::getStartPos() {
    return startPos;
}

bool Block::getIsHole() {
    return isHole;
}

void Block::setSize(int size) {
    this->size = size;
}

void Block::setStartPos(int startPos) {
    this->startPos = startPos;
}

void Block::setIsHole(int isHole) {
    this->isHole = isHole;
}

// Initialize vector of blocks to track allocation/deallocation of memory
vector<Block> blocks;


// MemoryManager Functions
MemoryManager::MemoryManager(unsigned wordSize, function<int(int, void *)> allocator)
{
    this->wordSize = wordSize;
    this->allocator = allocator;
}

MemoryManager::~MemoryManager()
{
    if (isInit) {
        shutdown();
    }
}

void MemoryManager::initialize(size_t sizeInWords)
{
    // Instantiate contiguous array of size (sizeInWords * wordSize) bytes
    if (sizeInWords > 65536) {
        return;
    }

    // If object already initialized, call shutdown
    if (isInit) {
        shutdown();
    }

    // Compute number of bytes
    unsigned sizeInBytes = sizeInWords * wordSize;

    // Allocate memory block with size of 8 bits (1 byte)
    memoryStart = new uint8_t[sizeInBytes];

    // Set block size as # bytes
    memoryLimit = sizeInBytes;

    // Initialize block at position 0 with size of sizeInWords. Set isHole = true since nothing allocated yet
    Block newBlock = Block(sizeInWords, 0, true);

    // Push new block to blocks vector
    blocks.push_back(newBlock);

    // Set isInit = true after initialization
    isInit = true;
}

void MemoryManager::shutdown()
{
    // If previously allocated, deallocate memory
    if (memoryStart != nullptr || !isInit) {
        delete[] static_cast<uint8_t*>(memoryStart);
        memoryStart = nullptr;
    }

    // Clear blocks vector, reset member variables
    blocks.clear();
    memoryLimit = 0;
    isInit = false;
}

void* MemoryManager::allocate(size_t sizeInBytes)
{
    // Check if no memory, size invalid, or not initialized
    if (sizeInBytes > memoryLimit || sizeInBytes == 0 || !isInit) {
        return nullptr;
    }

    // Calculate size in words and round up to whole word if needed
    size_t sizeInWords = (sizeInBytes + wordSize - 1) / wordSize;

    // Calculate total bytes from size in words
    size_t totalBytes = sizeInWords * wordSize;

    // Initialize temporary list to pass to allocator function by calling getList
    uint16_t* list = reinterpret_cast<uint16_t*>(getList());

    // Call allocator function to determine where to allocate memory
    int startPos = allocator(sizeInWords, list);

    // Delete list
    delete[] list;

    // Check if allocator function found position for block
    if (startPos != -1) {
        for (auto& block : blocks) {
            if (block.getStartPos() == startPos) {

                // Check for extra space
                if (block.getSize() > sizeInWords) {

                    // Adjust starting position of block by adding sizeInWords from startPos
                    block.setStartPos(block.getStartPos() + sizeInWords);

                    // Adjust size of block by subtracting sizeInWords from size
                    block.setSize(block.getSize() - sizeInWords);

                    // Create new block with new starting position and size
                    Block newBlock = Block(sizeInWords, startPos, false);

                    // Calculate index of new block
                    int index = &block - &blocks[0];

                    // Add new block to blocks vector at correct position
                    blocks.insert(blocks.begin() + index, newBlock);
                }

                // If no extra space, set isHole = false
                else {
                    block.setIsHole(false);
                }

                // Return ptr to starting location of newly allocated space
                return static_cast<void*>(static_cast<uint8_t*>(memoryStart) + (startPos * wordSize));
            }
        }
    }

    return nullptr;
}

void MemoryManager::free(void* address)
{
    // Check for valid ptrs and initialization
    if (memoryStart == nullptr || address == nullptr || !isInit) {
        return;
    }

    // Calculate word offset
    size_t wordOffset = (static_cast<uint8_t*>(address) - static_cast<uint8_t*>(memoryStart)) / wordSize;

    // Iterate through blocks vector to find block to free
    for (auto currentBlock = blocks.begin(); currentBlock != blocks.end(); currentBlock++) {
        if (currentBlock->getStartPos() == wordOffset && !currentBlock->getIsHole()) {

            // Mark block as free (isHole = true)
            currentBlock->setIsHole(true);

            // Merge current block with next adjacent free block
            auto nextBlock = currentBlock + 1;
            while (nextBlock !=blocks.end() && nextBlock->getIsHole()) {

                // Merge next block into current block and set new size
                currentBlock->setSize(currentBlock->getSize() + nextBlock->getSize());

                // Erase next block
                blocks.erase(nextBlock);
            }

            // Merge current block with previous adjacent free block
            if (currentBlock != blocks.begin() && (currentBlock - 1)->getIsHole()) {
                auto prevBlock = currentBlock - 1;

                // Merge current block into previous block and set new size
                prevBlock->setSize(prevBlock->getSize() + currentBlock->getSize());

                // Erase current block
                blocks.erase(currentBlock);
            }

            return;
        }
    }
}

void MemoryManager::setAllocator(function<int(int, void *)> allocator)
{
    this->allocator = allocator;
}

int MemoryManager::dumpMemoryMap(char *filename)
{
    // Open file
    int fileDescriptor = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0777);

    // If file not open, return -1
    if (fileDescriptor == -1) {
        return -1;
    }

    string outputString;

    // Iterate through blocks in blocks vector and check if hole
    for (auto i = 0; i < blocks.size(); i++) {
        if (blocks[i].getIsHole()) {

            // Format output string as [START, LENGTH] - [START, LENGTH] ...
            string startString = to_string(blocks[i].getStartPos());
            string lengthString = to_string(blocks[i].getSize());
            outputString += "[" + startString + ", " + lengthString + "]";

            // Check if current block is not last block in blocks vector
            if (i < blocks.size() - 1) {
                outputString += " - ";
            }
        }
    }

    // Write output string to file
    if (write(fileDescriptor, outputString.c_str(), outputString.size()) == -1) {

        // If not successful, close file descriptor and return -1
        close(fileDescriptor);
        return -1;
    }

    // Close file descriptor and return 0
    close(fileDescriptor);
    return 0;
}

void* MemoryManager::getList()
{
    if (memoryStart == nullptr || !isInit) {
        return nullptr;
    }

    // Count number of holes
    int holeCount = 0;

    for (auto& block : blocks) {
        if (block.getIsHole()) {
            holeCount++;
        }
    }

    // If no holes exist, return nullptr
    if (holeCount == 0) {
        return nullptr;
    }

    // Create array of 16 bits (2 bytes) for list of holes (2 for each hole, plus one for hole count)
    uint16_t* holeArray = new uint16_t[(holeCount * 2) + 1];

    // Set holeCount in index 0
    holeArray[0] = holeCount;

    int index = 1;
    for (auto& block : blocks) {
        if (block.getIsHole()) {

            // Calculate offset and length
            uint16_t offset = block.getStartPos();
            uint16_t length = block.getSize();

            // Add offset and length to array and increment index accordingly
            holeArray[index++] = offset;
            holeArray[index++] = length;
        }
    }

    return holeArray;
}

void* MemoryManager::getBitmap() 
{
    // Calculate total words
    size_t totalWords = memoryLimit / wordSize;

    // Calculate total bytes and round up to whole byte if needed
    size_t totalBytes = (totalWords + 7) / 8;

    // Initialize empty byte string
    string currentByte = "";

    // Initialize vector to store byte strings
    vector<string> byteStrings;

    // Iterate through each block in blocks vector
    for (auto& block : blocks) {
        char bit;

        // If block is a hole, bit = 0
        if (block.getIsHole()) {
            bit = '0';
        } 

        // If block is allocated, bit = 1
        else {
            bit = '1';
        }

        // Add each bit to current byte
        for (int i = 0; i < block.getSize(); i++) {
            currentByte += bit;

            // Once current byte has 8 bits, push to byte strings vector and clear currentByte for new byte
            if (currentByte.size() == 8) {
                byteStrings.push_back(currentByte);
                currentByte.clear();
            }
        }
    }

    // If last byte has less than 8 bits, add 0s to the end to fill it to 8
    if (!currentByte.empty()) {
        while (currentByte.size() < 8) {
            currentByte += '0';
        }

        // Push last byte to byte strings vector
        byteStrings.push_back(currentByte);
    }

    // Set first 2 bytes to represent size of array
    uint8_t byte1 = static_cast<uint8_t>(byteStrings.size() & 0xFF);
    uint8_t byte2 = static_cast<uint8_t>((byteStrings.size() >> 8) & 0xFF);

    // Allocate memory and add first 2 bytes to bit map
    auto* bitmap = new uint8_t[byteStrings.size() + 2];
    bitmap[0] = byte1;
    bitmap[1] = byte2;

    // Convert each string in byte strings vector to byte and store in bitmap
    for (auto i = 0; i < byteStrings.size(); ++i) {
        
        // Reverse string to follow little-endian format
        reverse(byteStrings[i].begin(), byteStrings[i].end());
    
        // Convert string to integer before storing. Starts at index 2 after byte1 and byte2
        bitmap[i + 2] = static_cast<uint8_t>(stoi(byteStrings[i], nullptr, 2));
    }

    return bitmap;
}

unsigned MemoryManager::getWordSize()
{
    return wordSize;
}

void *MemoryManager::getMemoryStart()
{
    return memoryStart;
}

unsigned MemoryManager::getMemoryLimit()
{
    return memoryLimit;
}


// Allocator Functions
int bestFit(int sizeInWords, void* list)
{
    if (list == nullptr) {
        return -1;
    }
    
    uint16_t* holeArray = static_cast<uint16_t*>(list);
    int holeCount = holeArray[0];

    // Initialize word offset to -1, and minimum fit size to max. int
    int wordOffset = -1;
    int minFitSize = INT_MAX;

    // Iterate through each hole to find best fit
    for (int i = 0; i < holeCount; i++) {
        int offset = holeArray[(i * 2) + 1];
        int length = holeArray[(i * 2) + 2];

        // Check if hole fits size and is smallest suitable hole
        if (length >= sizeInWords && length < minFitSize) {

            // Update offset and length
            wordOffset = offset;
            minFitSize = length;
        }
    }

    // Return word offset, or -1 if no fit
    return wordOffset;
}

int worstFit(int sizeInWords, void* list)
{
    if (list == nullptr) {
        return -1;
    }

    uint16_t* holeArray = static_cast<uint16_t*>(list);
    int holeCount = holeArray[0];

    // Initialize word offset and maximum fit size to -1
    int wordOffset = -1;
    int maxFitSize = -1;

    // Iterate through each hole to find worst fit
    for (int i = 0; i < holeCount; i++) {
        int offset = holeArray[(i * 2) + 1];
        int length = holeArray[(i * 2) + 2];

        // Check if hole fits size and is largest suitable hole
        if (length >= sizeInWords && length > maxFitSize) {

            // Update offset and length
            wordOffset = offset;
            maxFitSize = length;
        }
    }

    // Return word offset, or -1 if no fit
    return wordOffset;
}