// QuickSort.cpp: Definiert den Einstiegspunkt für die Konsolenanwendung.
//

#include "stdafx.h"
#include "mpi.h"

#define N 1000
#define K 10

#define arrayType unsigned long

/////////////////////////// forward declararions //////////////////////////////
//initialises array, as stated by the task description
void initArray(arrayType * arr, int n);

//modifies array, as stated by the task description
void modifyArray(arrayType * arr, int arrayLength, int rank, int commSize);

//implementation of the quicksort algorithm
void quicksort(arrayType * arr, int length);

//helper function, finds median-element of the first 'length' elements in array arr
arrayType findMedian(arrayType * arr, int length = 3);
//checks if array arr has been sorted properly. returns true if sorted, false otherwise
bool checkIsSorted(arrayType * arr, int length);

//returns true if every entry in checks is true. returns false if a single value is false
bool reduceChecks(bool * checks, int length);

////////////////////////////// inline functions ///////////////////////////////
arrayType inline shift_left_cyclic(arrayType x, int i)
{
    return (x << i) | (x >> (64 - i));
}

void inline printArray(arrayType * arr, int size)
{
    for (int i = 0; i < size; i++)
    {
        std::cout << arr[i] << " ";
        if (1 % 20 == 0)
            std::cout << std::endl;
    }
        
}
/////////////////////////////// main function /////////////////////////////////
int main(int argc, char* argv[])
{
	MPI_Init(&argc, &argv);

	int rank(0);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    arrayType arr[N];

	if (rank == 0)
	{
        initArray(arr, N);
#ifdef _DEBUG
        printArray(arr, N);
#endif
	}

    MPI_Bcast(arr, N, MPI_INT32_T, 0, MPI_COMM_WORLD);

    int size;
    MPI_Comm_size(MPI_COMM_WORLD, &size);


    for (int i = 0; i < size; i++)
    {
        if (i == rank)
        {
            std::cout << i << ": " << arr[0] << std::endl;
        }
        MPI_Barrier(MPI_COMM_WORLD);
    }

#ifdef _DEBUG
	bool * checks = new bool[K];
	memset(checks, false, K);
#endif // _DEBUG

    for (int i = 0; i < K; i++)
    {
        quicksort(arr, N);
#ifdef _DEBUG
		if(rank == 0)
			checks[i] = checkIsSorted(arr, N);
#endif // _DEBUG
        modifyArray(arr, N, rank, size);
    }

#ifdef _DEBUG
	if (rank == 0)
	{
		bool total(reduceChecks(checks, K));
		if (total)
			std::cout << "Arrays sorted correctly." << std::endl;
		else
			std::cout << "Error while sorting, not sorted properly!" << std::endl;
	}
#endif // _DEBUG

	MPI_Finalize();
#ifdef _DEBUG
    int i;
    std::cin >> i;
#endif
    return 0;
}

////////////////////////// function implementations ///////////////////////////
void initArray(arrayType * arr, int n)
{
    srand(123);
    for (int i = 0; i < n; i++)
        arr[i] = ((arrayType)rand()) * rand() * 3 + rand();
}

void modifyArray(arrayType * arr, int arrayLength, int rank, int commSize)
{
    int arrea(arrayLength / commSize);
    int start(rank * arrea);
    int stop(((rank + 1) * arrea ) - 1);
    
    if (rank == commSize - 1)
        stop = arrayLength - 1;

    for (int i = start; i < stop; i++)
        arr[i] = shift_left_cyclic(arr[i], i % 4 + 1);
}

void quicksort(arrayType * arr, int length)
{
	arrayType median(findMedian(arr));
    //distribute sections to threads
    int start(0);
    int sectionSize(0);
    int iterations(0);
    
    //sort section
    int head(0);
    int tail(sectionSize - 1);

    arrayType * tmp = new arrayType[sectionSize];
    for (int i = start; i < iterations; i++)
    {
        arrayType & tmpElem(arr[i]);
        if ( tmpElem < median)
        {
            tmp[head] = tmpElem;
            head++;
        }
        else if( tmpElem > median)
        {
            tmp[tail] = tmpElem;
            tail--;
        }
    }
    for (; head < tail; head++)
    {
        tmp[head] = median;
    }
    //gather results: #elems smaller, #medians, #elems larger 
    //create communication group for next iteration
    //next iteration smaler sections
    //
}

arrayType findMedian(arrayType * arr, int length)
{
	//quickly sort elements, return element in the middle
	bool changedValues(false);
	int i(0);
	arrayType tmp(0);
	do
	{
		changedValues = false;
		for (i = 0; i < length - 1; i++)
		{
			if (arr[i] > arr[i + 1])
			{
				tmp = arr[i + 1];
				arr[i + 1] = arr[i];
				arr[i] = tmp;
				changedValues = true;
			}
		}
	} while (changedValues);
	return *(arr + 1);
}

bool checkIsSorted(arrayType * arr, int length)
{
	for (int i = 0; i < length - 1; i++)
	{
        if (arr[i] > arr[i + 1])
        {
            std::cout << "Position #" << i << "is not larger than " << i + 1 << std::endl;
            return false;
        }
			
	}
	return true;
}

bool reduceChecks(bool * checks, int length)
{
	bool ret(true);
    for (int i = 0; i < length; i++)
    {
        ret = (ret && checks[i]);
        if (!ret)
        {
            std::cout << "sort failed at run #" << i << std::endl;
        }
    }
	return ret;
}
