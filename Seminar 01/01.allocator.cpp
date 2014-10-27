#include <iostream>
#include <cstdlib>
#include <list>

using namespace std;

struct Chunk{
    bool busy;
    int size;
    char* data_ptr;
};

class CPointer {
    Chunk* chunk_ptr;
public:
    CPointer(Chunk* ptr = NULL): chunk_ptr(ptr) {}
    Chunk* getChunk() { return chunk_ptr; }
    char* operator*() { return chunk_ptr->data_ptr; }
    char* operator->() { return chunk_ptr->data_ptr; }
};

class CAllocator {
    char* memory;
    list<Chunk> info;
public:
    CAllocator(int size_ = 1024);
    ~CAllocator();

    CPointer Alloc(int size_);
    void Free(CPointer ptr_);
    CPointer ReAlloc(CPointer ptr_, int size_);
    void Defrag();
};

CAllocator::CAllocator(int size_) {
    memory = (char *) malloc(size_);
    if (!memory) {
        cerr << "ERROR! There is no space for allocator of size " << size_ << endl;
        exit(1);
    }
    Chunk tmp;
    tmp.busy = false;
    tmp.size = size_;
    tmp.data_ptr = memory;
    info.push_back(tmp);
}

CAllocator::~CAllocator() {
    free(memory);
    for (auto iter = info.begin(); iter != info.end(); ++iter) 
        if (iter->busy) 
            cerr << "Data at " << hex << iter->data_ptr << dec << " not freed\n";
}

CPointer CAllocator::Alloc(int size_) {
    cout << "Allocating...";
    list<Chunk>::iterator result;
    for (result = info.begin(); result != info.end(); ++result)
        if (!result->busy && result->size >= size_)
            break;
    if (result == info.end()) {
        cout << "fail\n";
        cout << "Allocation failed. Trying to defrag.\n";
        //Defrag();
        cout << "Allocating again...";
        for (result = info.begin(); result != info.end(); ++result)
            if (!result->busy && result->size >= size_) 
                break;
    }
    if (result == info.end()) {
        cout << "fail\n";
        cerr << "Allocation failed: no space left.\n";
        exit(1);
    }
    else {
        cout << "success\n";
        if (result->size - size_ == 0) {
            result->busy = true;
            return CPointer(&(*result));
        }
        Chunk tmp;
        tmp.busy = true;
        tmp.size = size_;
        tmp.data_ptr = result->data_ptr;
        result->data_ptr = tmp.data_ptr + size_;
        result->size -= size_;
        info.insert(result, tmp);
        return CPointer(&(*--result));
    }
}


void CAllocator::Free(CPointer ptr_) {
    Chunk* tmp = ptr_.getChunk();
    for (list<Chunk>::iterator iter = info.begin(); iter != info.end(); ++iter)
        if (&(*iter) == tmp) {
            iter->busy = false;
            cout << "Free...success\n";
            return;
        }
    cout << "Wrong CPointer!\n";
}

//TODO: CPointer ReAlloc(CPointer ptr_, int size_);

/*
void Defrag() {
    for (auto iter = info.begin(); iter != info.end()-1; ++iter) {
        if (iter->busy)
            continue;
        if ((iter+1)->busy) {

        }
    }
}
*/

int main() {
    CAllocator myalloc(2048);
    CPointer a = myalloc.Alloc(1024);
    CPointer b = myalloc.Alloc(512);
    myalloc.Free(a);
    CPointer c = myalloc.Alloc(888);
    //CPointer d = myalloc.Alloc(512);
    return 0;
}
