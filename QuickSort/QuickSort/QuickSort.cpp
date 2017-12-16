// QuickSort.cpp: Definiert den Einstiegspunkt für die Konsolenanwendung.
//

#include "stdafx.h"
#include "mpi.h"

#define N 1000
#define K 10

#define arrayType unsigned long

//forward declararions
void initArray(arrayType * arr, int n);
void modifyArray(arrayType * arr, int arrayLength, int rank, int commSize);

void quicksort(arrayType * arr, int length);

arrayType findMedian(arrayType * arr, int length = 3);

bool checkIsSorted(arrayType * arr, int length);
bool reduceChecks(bool * checks, int length);

//inline functions
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

int main(int argc, char* argv[])
{
	MPI_Init(&argc, &argv);

	int rank(0);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    arrayType arr[N];

	if (rank == 0)
	{
        initArray(arr, N);
        //printArray(arr, N);
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
        quicksort();
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
			return false;
	}
	return true;
}

bool reduceChecks(bool * checks, int length)
{
	bool ret(true);
	for (int i = 0; i < length; i++)
		ret = (ret && checks[i]);

	return ret;
}
