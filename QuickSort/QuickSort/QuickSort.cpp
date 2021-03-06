// QuickSort.cpp: Definiert den Einstiegspunkt für die Konsolenanwendung.

#include "stdafx.h"

using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;
using std::chrono::steady_clock;
using std::queue;

using std::ofstream;

///////////////////////////////////////////// Makros ////////////////////////////////////////////
#define printInfo std::cout << "Rank: " << globalRank << "    "  << __LINE__ << std::endl;;;;
#define printText(x) std::cout << "Rank: " << globalRank << "    "  << "MSG: " << x << std::endl;

//////////////////////////////////////////// typedefs ///////////////////////////////////////////
typedef std::chrono::time_point<std::chrono::steady_clock> timerTime;

//////////////////////////////////////////// structs ////////////////////////////////////////////
///Stores tmp result for a single sorting proces. Lists contain elements LESS than or 
///equal to a pivot element and elements LARGER than that pivot element. Also stores respective
///element counts.
struct SortedLists
{
    unsigned long * less;
    unsigned long * larger;

    int * numElems;

    SortedLists(): 
        less(nullptr)
        , larger(nullptr)
        , numElems(new int [2])
    {
        numElems[0] = 0;
        numElems[1] = 0;
    }
    SortedLists(const SortedLists & other) :
        less(new unsigned long[other.numElems[0]])
        , larger(new unsigned long[other.numElems[1]])
        , numElems(new int[2])
    {
        numElems[0] = other.numElems[0];
        numElems[1] = other.numElems[1];

        memcpy(less, other.less, numElems[0] * sizeof(unsigned long));
        memcpy(larger, other.larger, numElems[1] * sizeof(unsigned long));
    }
    ~SortedLists()
    {
        if (less)
        {
            delete[] less;
            less = nullptr;
        }
        if (larger)
        {
            delete[] larger;
            larger = nullptr;
        }
    }
};

/////////////////////////////////////// global variables ////////////////////////////////////////
static int globalRank(0);
static int globalArraySize(0);

//////////////////////////////////// forward declararions ///////////////////////////////////////
///reads a program-parameter and returns the int value represented by it (as char *)
int readProgramParameters(const char * argv);

///Creates & initialises an array of size arraySize
void initArray(unsigned long * arr, const int arraySize);

///Implementation of the MPI quicksort algorithm
void quicksort(unsigned long * arr, const int arraySize, MPI_Comm comm = MPI_COMM_WORLD); 

///Calculates start position and sectionsize for an area of the array that is getting sorted
void calculateSection(const int arraySize, const MPI_Comm comm, int * start, int * sectionSize);

///Sorts a section of an Array into elements less than or equal to a pivot and larger than that 
///pivot element
SortedLists * sortSection(unsigned long * arr, int start, int sectionSize);

///collects all the tmp results 
void gatherResults(MPI_Comm comm, unsigned long * dstArray, int * totalLess, SortedLists & toSend);

///sorts element counts of less than and larger than pivot element counts into two seperate lists
void sortElementCounts(int * lessCounts, int * largerCounts, const int * totalCounts, int arraySize);

///sums up all the values in the array
int sumLess(const int * arr, const int arraySize);

/// Creates a new MPI communicator
MPI_Comm createCommunicator(MPI_Comm currentComm, int lessElements, int totalElements, bool * leftSide);

///sequentially implementation of the quicksort, used when a new communicator contains only a 
///single process
void sequentialQuickSort(unsigned long * arr, int arraySize, int recursionDepth = 0);

///sorts an array that contains less than 4 elements, using simple bubblesort
void sortThreeOrLess(unsigned long * arr, int arraySize);

///gathers finally sorted array segments at root node
void gatherFinal(unsigned long * arr, int arraySize);

///checks if the sorting was successfull
bool checkSorted(unsigned long * arr, int arraySize);

///print an array to console
void printArray(unsigned long * arr, int arraySize, MPI_Comm comm);
void printArray(int * arr, int arraySize, MPI_Comm comm);


void printArraySingle(unsigned long * arr, int arraySize);
void printArraySingle(int * arr, int arraySize);

///Prepares Array for next sorting iteration
void modifyArray(unsigned long * arr, int arraySize);

unsigned long findPivot(const unsigned long *arr);

/////////////////////////////////////// inline functions ////////////////////////////////////////
unsigned long inline shift_left_cyclic(unsigned long x, int j)
{
    return (x << j) | (x >> (64 - j));
}

//////////////////////////////////////// main function //////////////////////////////////////////
int main(int argc, char* argv[])
{
    MPI_Init(&argc, &argv);

    MPI_Comm_rank(MPI_COMM_WORLD, &globalRank);

    unsigned int arraySize(0);
    unsigned int sortCycles(0);
    if (argc == 3)
    {
        arraySize = readProgramParameters(argv[1]);
        sortCycles = readProgramParameters(argv[2]);
    }
    else
    {
        arraySize = 1000;
        sortCycles = 1;
    }
    globalArraySize = arraySize;

    unsigned long * arr(new unsigned long[arraySize]);

    initArray(arr, arraySize);
    
    int commRank(0);
    MPI_Comm_rank(MPI_COMM_WORLD, &commRank);

    high_resolution_clock hrClock;
    
    queue<timerTime> startTimes;
    queue<timerTime> intermediateTimes;
    queue<timerTime> endTimes;


    for (unsigned int i = 0; i < sortCycles; i++)
    {
        //sort then modify array
        startTimes.push( hrClock.now());
        quicksort(arr, arraySize);    
        
        //if (commRank == 0)
        //    checkSorted(arr, arraySize);

        intermediateTimes.push(hrClock.now());
        //MPI_Barrier(MPI_COMM_WORLD);

        modifyArray(arr, arraySize);

        endTimes.push(hrClock.now());
        MPI_Barrier(MPI_COMM_WORLD);
    }

    //calculate time intervals
    timerTime firstStart(startTimes.front());

    queue<steady_clock::duration>intermediateDurations;
    queue<steady_clock::duration>totalDurations;

    for (size_t i = 0; i < sortCycles; i++)
    {
        timerTime & startTime(startTimes.front());
        timerTime & intermediate(intermediateTimes.front());
        timerTime & endTime(endTimes.front());

        intermediateDurations.push(intermediate - startTime);
        totalDurations.push(endTime - startTime);

        startTimes.pop();
        intermediateTimes.pop();
        
        if(i != startTimes.size() - 1)
            endTimes.pop();//keep very last time in queue
    }

    steady_clock::duration totalExecutionTime(endTimes.front() - firstStart);

    //print times to a file
    ofstream writer;
    writer.open("ExecutionTimes.txt");
    writer << "### Execution Times ###" << std::endl << std::endl;

    writer << "ArraySize: " << arraySize << "   Sort Sycles: " << sortCycles << std::endl << std::endl;

    writer << "Total execution time: " << totalExecutionTime.count() << "ns" << std::endl << std::endl;
    long long avgSort(0);
    long long avgTotal(0);
    long long avgMod(0);
    for (size_t i = 0; i < sortCycles; i++)
    {
        long long interm(intermediateDurations.front().count());
        long long total(totalDurations.front().count());
        long long mod(total - interm);

        writer << "Run #" << i << ":" << std::endl;
        writer << "Sort only: " << interm << "ns   ";
        writer << "Sort & Modify: " << total << "ns   ";
        writer << "Modify only: " << mod << "ns" << std::endl << std::endl;

        avgSort += interm;
        avgTotal += total;
        avgMod += mod;
    }
    
    avgSort /= sortCycles;
    avgTotal /= sortCycles;
    avgMod /= sortCycles;

    writer << "Avg. Sorting time: " << avgSort << "ns  ";
    writer << "Avg. Total time: " << avgTotal << "ns  ";
    writer << "Avg. Modifying time: " << avgMod << "ns" << std::endl;

    writer.close();
    MPI_Finalize();
    return 0;
}

/////////////////////////////////// function implementations ////////////////////////////////////

int readProgramParameters(const char * argv)
{
    int ret(0);
    unsigned int index = 1;
    char tmp(argv[0]);
    
    if (tmp < 48 || tmp > 57)
        return -1;
    
    while (tmp != '\0')
    {
        ret *= 10;
        ret += (tmp - 48); //48 == ASCII '0'
        tmp = argv[index];
        index++;
    }
    return ret;
}

void initArray(unsigned long * arr, const int arraySize)
{
    srand(123);
    for (int i = 0; i < arraySize; i++)
        arr[i] = ((unsigned long)rand())  * rand() * 3 + rand();
}

void quicksort(unsigned long * arr, const int arraySize, MPI_Comm comm)
{
    int start(0);
    int sectionSize(0);
    calculateSection(arraySize, comm, &start, &sectionSize);

    SortedLists * intermediate(sortSection(arr, start , sectionSize));

    int totalLess(0);
    gatherResults(comm, arr, &totalLess, *intermediate);

    bool leftSide(false);
    MPI_Comm nextComm(createCommunicator(comm, totalLess, arraySize, &leftSide));

    int newCommSize(0);
    MPI_Comm_size(nextComm, &newCommSize);

    //start next sorting itreration. Process will either work in the left or the right side of the
    //array. When fionished sorting, gather all results at original root node
    if (newCommSize < 2)//only one process in communicator, finish sequentially
    {
        if (leftSide)
        {
            if (totalLess > 3)
            {
                sequentialQuickSort(arr, totalLess);
                gatherFinal(arr, totalLess);
            }
            else
            {
                sortThreeOrLess(arr, totalLess);
                gatherFinal(arr, totalLess);
            }
        }
        else
        {
            int numElems(arraySize - totalLess);
            if (numElems > 3)
            {
                sequentialQuickSort(arr + totalLess, numElems);
                gatherFinal(arr + totalLess, numElems);
            }
            else
            {
                sortThreeOrLess(arr + totalLess, numElems);
                gatherFinal(arr + totalLess, numElems);
            }
        }
    }
    else//at least 2 processes in communicator, proceed with quicksort
    {
        if (leftSide)
        {
            if (totalLess > 3)
            {
                quicksort(arr, totalLess, nextComm);
            }
            else
            {
                int newCommRank(0);
                MPI_Comm_rank(nextComm, &newCommRank);
                if (newCommRank == 0)
                {
                    sortThreeOrLess(arr, totalLess);
                    gatherFinal(arr, totalLess);
                }
                else
                {
                    gatherFinal(arr, 0);
                }
            }
        }
        else
        {
            int numElems(arraySize - totalLess);
            if (numElems > 3)
            {
                quicksort(arr + totalLess, numElems, nextComm);
            }
            else
            {
                int newCommRank(0);
                MPI_Comm_rank(nextComm, &newCommRank);
                if (newCommRank == 0)
                {
                    sortThreeOrLess(arr + totalLess, numElems);
                    gatherFinal(arr + totalLess, numElems);
                }
                else
                {
                    gatherFinal(arr + totalLess, 0);
                }
            }
        }
    }
    delete intermediate;
}

void calculateSection(const int arraySize, const MPI_Comm comm, int * start, int * sectionSize)
{
    int commSize(0);
    MPI_Comm_size(comm, &commSize);

    int commRank(0);
    MPI_Comm_rank(comm, &commRank);

    *sectionSize = arraySize / commSize;

    *start = (*sectionSize) * commRank;

    //make shure last element gets sorted as well
    if (commRank == commSize - 1)
        *sectionSize = arraySize - ((commRank) * (*sectionSize));
}

SortedLists * sortSection(unsigned long * arr, int start, int sectionSize)
{
    SortedLists * ret (new SortedLists());

    unsigned long pivot(findPivot(arr));
    unsigned long current(0);
    unsigned long * tmpArray(new unsigned long[sectionSize]);

    //sort section, starting with an offset
    arr += start;
    int head(0);
    int tail(sectionSize - 1);

    for(int i = 0; i < sectionSize; i++)
    {
        current = arr[i];
        if (current < pivot)
        {
            tmpArray[head] = current;
            head++;
        }
        else
        {
            tmpArray[tail] = current;
            tail--;
        }
    }
    // store results in ret-object
    ret->numElems[0] = head;
    ret->numElems[1] = sectionSize - head;
    if (ret->numElems[0] > 0)
    {
        ret->less = new unsigned long[ret->numElems[0]];
        memcpy(ret->less, tmpArray, ret->numElems[0] * sizeof(unsigned long));
    }
    if (ret->numElems[1] > 0)
    {
        ret->larger = new unsigned long[ret->numElems[1]];
        memcpy(ret->larger, tmpArray + ret->numElems[0], ret->numElems[1] * sizeof(unsigned long));
    }

    delete[] tmpArray;
    tmpArray = nullptr;
    return ret;
}

void gatherResults(MPI_Comm comm, unsigned long * dstArray, int * totalLess, SortedLists & toSend)
{
    int commSize(0);
    MPI_Comm_size(comm, &commSize);

    int totalNumElements(2 * commSize);
    int * elementCounts(new int[totalNumElements]);
    memset(elementCounts, 0, totalNumElements * sizeof(int));

    MPI_Allgather(toSend.numElems, 2, MPI_INT, elementCounts, 2, MPI_INT, comm);

    int * lessElementCounts(new int[commSize]);
    memset(lessElementCounts, 0, commSize * sizeof(int));

    int * largerElementCounts(new int[commSize]);
    memset(largerElementCounts, 0, commSize * sizeof(int));
    
    sortElementCounts(lessElementCounts, largerElementCounts, elementCounts, totalNumElements);
    (*totalLess) = sumLess(lessElementCounts, commSize);
    
    int * displs(new int[commSize]);
    memset(displs, 0, commSize * sizeof(int));

    int currentDispl(0);
    for (int i = 0; i < commSize; i++)
    {
        displs[i] = currentDispl;
        currentDispl += lessElementCounts[i];
    }

    MPI_Allgatherv(toSend.less, toSend.numElems[0], MPI_INT, dstArray, lessElementCounts, displs, MPI_INT, comm);
    
    currentDispl = 0;
    for (int i = 0; i < commSize; i++)
    {
        displs[i] = currentDispl;
        currentDispl += largerElementCounts[i];
    }

    MPI_Allgatherv(toSend.larger, toSend.numElems[1], MPI_INT, dstArray + (*totalLess), largerElementCounts, displs, MPI_INT, comm);

    delete[] elementCounts;
    delete[] lessElementCounts;
    delete[] largerElementCounts;
    delete[] displs;
}

void sortElementCounts(int * lessCounts, int * largerCounts, const int * totalCounts, int arraySize)
{
    int lessIndex(0);
    int largerIndex(0);
    for (int i = 0; i < arraySize; i++)
    {
        if (i % 2 == 0)
        {
            lessCounts[lessIndex] = totalCounts[i];
            lessIndex++;
        }
        else
        {
            largerCounts[largerIndex] = totalCounts[i];
            largerIndex++;
        }
    }
}

int sumLess(const int * arr, const int arraySize)
{
    int ret(0);
    for (int i = 0; i < arraySize; i++)
    {
        ret += arr[i];
    }
    return ret;
}

MPI_Comm createCommunicator(MPI_Comm currentComm, int lessElements, int totalElements, bool * leftSide)
{
    MPI_Comm ret;
    
    double elementRatio(((double)lessElements) / ((double)totalElements));
    int commSize(0);
    MPI_Comm_size(currentComm, &commSize);
    
    int commRatio((int)(std::round(elementRatio * commSize)));

    if (commRatio < 1)
        commRatio = 1;

    if (commRatio == commSize)
        commRatio--;
    
    MPI_Group currentGroup(0);
    MPI_Comm_group(currentComm, &currentGroup);
    
    int * ranks(nullptr);
    int n(0);
    
    int currentRank(0);
    MPI_Comm_rank(currentComm, &currentRank);
    
    if (currentRank < commRatio) //this process will work on LEFT side
    {
        ranks = new int[commRatio];
        for (int i = 0; i < commRatio; i++)
            ranks[i] = i;
        n = commRatio;
        *leftSide = true;
    }
    else //this process will work on RIGHT side
    {
        int size(commSize - commRatio);
        ranks = new int[size];
        for (int i = 0; i < size; i++)
            ranks[i] = i + commRatio;
        n = size;
        *leftSide = false;
    }

    MPI_Group newGroup(0);
    MPI_Group_incl(currentGroup, n, ranks, &newGroup);
    
    MPI_Comm_create(currentComm, newGroup, &ret);
    delete[] ranks;
    return ret;
}

void sequentialQuickSort(unsigned long * arr, int arraySize, int recursionDepth)
{

    if (recursionDepth >= 1000)
    {
        sortThreeOrLess(arr, arraySize);
        return;
    }
    //sort section, then copy sorted arrays to origin array
    SortedLists * tmp (sortSection(arr, 0 , arraySize));
    
    //sort left and right parts of the array
    if (tmp->numElems[0] > 3) // at least 3 elems required for sortSection
        sequentialQuickSort(tmp->less, tmp->numElems[0], recursionDepth + 1);
    else
        sortThreeOrLess(tmp->less, tmp->numElems[0]);

    if (tmp->numElems[1] > 3)
        sequentialQuickSort(tmp->larger, tmp->numElems[1], recursionDepth + 1);
    else
        sortThreeOrLess(tmp->larger, tmp->numElems[1]);

    if(tmp->numElems[0] > 0)
        memcpy(arr, tmp->less, tmp->numElems[0] * sizeof(unsigned long));

    if (tmp->numElems[1] > 0)
        memcpy(arr + tmp->numElems[0], tmp->larger, tmp->numElems[1] * sizeof(unsigned long));

    delete tmp;
}

void sortThreeOrLess(unsigned long * arr, int arraySize)
{
    if (arraySize < 2)
        return;

    bool changed(false);
    unsigned long tmp(0);
    do
    {
        changed = false;
        for (int i = 0; i < arraySize - 1; i++)
        {
            if (arr[i] > arr[i + 1])
            {
                tmp = arr[i];
                arr[i] = arr[i + 1];
                arr[i + 1] = tmp;
                changed = true;
            }
        }
    } while (changed);

}

void gatherFinal(unsigned long * arr, int arraySize)
{
    int commSize(0);
    MPI_Comm_size(MPI_COMM_WORLD, &commSize);
    
    if (commSize < 2)
        return;

    int * segmentSizes(new int[commSize]);
    MPI_Gather(&arraySize, 1, MPI_INT, segmentSizes, 1, MPI_INT, 0, MPI_COMM_WORLD);
    
    int * displs(new int[commSize]);
    memset(displs, 0, commSize * sizeof(int));

    unsigned long * tmp;
    
    tmp = new unsigned long[globalArraySize];
    memset(tmp, 0, globalArraySize * sizeof(unsigned long));
    
    int currentDispl(0);
    for (int i = 0; i < commSize; i++)
    {
        displs[i] = currentDispl;
        currentDispl += segmentSizes[i];
    }

    MPI_Gatherv(arr, arraySize, MPI_INT, tmp, segmentSizes, displs, MPI_INT, 0, MPI_COMM_WORLD);

    if(globalRank == 0)
        memcpy(arr, tmp, globalArraySize * sizeof(unsigned long));

    delete[] segmentSizes;
    delete[] displs;
}

bool checkSorted(unsigned long * arr, int arraySize)
{
    bool ret(true);
    std::cout << std::endl << std::endl;
    for (int i = 0; i < arraySize; i++)
    {
        if (arr[i] > arr[i + 1])
        {
            std::cout << i << ": " << arr[i] << " > " << i + 1 << ": " << arr[i + 1] << std::endl;
            ret = false;
        }
        /*if(!ret)
            std::cout << i << ": " << arr[i] << std::endl;*/
    }
    return ret;
}

void printArray(unsigned long * arr, int arraySize, MPI_Comm comm)
{
    int commRank(0);
    MPI_Comm_rank(comm, &commRank);

    int commSize(0);
    MPI_Comm_size(comm, &commSize);

    for (int j = 0; j < commSize; j++)
    {
        if (commRank == j)
        {
            std::cout << std::endl << "Rank: " << j << std::endl;
            for (int i = 0; i < arraySize; i++)
            {
                std::cout << i << ": " << arr[i] << " ";
                if (i % 5 == 0)
                    std::cout << std::endl;
            }
        }
        MPI_Barrier(comm);
        std::cout << std::endl << "Rank: " << commRank << std::endl;
    }
}

void printArray(int * arr, int arraySize, MPI_Comm comm)
{
    int commRank(0);
    MPI_Comm_rank(comm, &commRank);
    
    int commSize(0);
    MPI_Comm_size(comm, &commSize);
    
    for (int j = 0; j < commSize; j++)
    {
        if (commRank == j)
        {
            std::cout << "Rank: " << j << std::endl;
            for (int i = 0; i < arraySize; i++)
            {
                std::cout << i << ": " << arr[i] << " ";
                if (i % 5 == 0)
                    std::cout << std::endl;
            }
        }
        MPI_Barrier(comm);
        std::cout << std::endl << "Rank: " << commRank << std::endl;
    }
}

void printArraySingle(unsigned long * arr, int arraySize)
{
    std::cout << std::endl;
    for (int i = 0; i < arraySize; i++)
    {
        std::cout << i << ": " << arr[i] << " ";
        if (i % 5 == 0)
            std::cout << std::endl;
    }

}

void printArraySingle(int * arr, int arraySize)
{
    std::cout << std::endl;
    for (int i = 0; i < arraySize; i++)
    {
        std::cout << i << ": " << arr[i] << " ";
        if (i % 5 == 0)
            std::cout << std::endl;
    }
}

void modifyArray(unsigned long * arr, int arraySize)
{
    int start(0);
    int sectionSize(0);
    calculateSection(arraySize, MPI_COMM_WORLD, &start, &sectionSize);
    
    unsigned long * tmpPtr (arr + start);

    for (int i = 0; i < sectionSize; i++)
    {
        tmpPtr[i] = shift_left_cyclic(tmpPtr[i], i % 4 + 1);
    }

    int commSize(0);
    MPI_Comm_size(MPI_COMM_WORLD, &commSize);

    int * recCounts(new int[commSize]);
    memset(recCounts, 0, commSize * sizeof(int));

    MPI_Allgather(&sectionSize, 1, MPI_INT, recCounts, 1, MPI_INT, MPI_COMM_WORLD);

    int * displs(new int[commSize]);
    memset(displs, 0, commSize * sizeof(int));

    int currentDispl(0);
    for (int i = 0; i < commSize; i++)
    {
        displs[i] = currentDispl;
        currentDispl += recCounts[i];
    }

    MPI_Allgatherv(tmpPtr, sectionSize, MPI_INT, arr, recCounts, displs, MPI_INT, MPI_COMM_WORLD);
    
    delete[]recCounts;
}

unsigned long findPivot(const unsigned long * arr)
{
    unsigned long first(arr[0]);
    unsigned long second(arr[1]);
    unsigned long third(arr[2]);

    if (first > second) 
    {
        if (first > third) // first > second && first > third
        {
            if (second > third) // first > second > third
                return second;
            else // first > third >= second
                return third;
        }
        else // third >= first > second
            return first;
    }
    else if (first > third) // second >= first > third, first is pivot
        return first;
    else // second >= first && third >= first
    {
        if (second > third) //second > third >= first
            return third;
        else // third >= second >= first
            return second;
    }
}
