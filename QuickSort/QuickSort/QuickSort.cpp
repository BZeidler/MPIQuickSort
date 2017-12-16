// QuickSort.cpp: Definiert den Einstiegspunkt für die Konsolenanwendung.
//

#include "stdafx.h"
#include "mpi.h"

#define N 1000
#define K 10

typedef unsigned long ulint;

//forward declararions
void initArray(ulint * arr, int n);
void modifyArray(ulint * arr, int arrayLength, int rank, int commSize);

void quicksort();

//inline functions
ulint inline shift_left_cyclic(ulint x, int i)
{
    return (x << i) | (x >> (64 - i));
}

void inline printArray(ulint * arr, int size)
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

    ulint arr[N];

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
    
    for (int i = 0; i < K; i++)
    {
        quicksort();
        modifyArray(arr, N, rank, size);
    }


	MPI_Finalize();
#ifdef _DEBUG
    int i;
    std::cin >> i;
#endif
    return 0;
}

void initArray(ulint * arr, int n)
{
    srand(123);
    for (int i = 0; i < n; i++)
        arr[i] = ((ulint)rand()) * rand() * 3 + rand();
}

void modifyArray(ulint * arr, int arrayLength, int rank, int commSize)
{
    int arrea(arrayLength / commSize);
    int start(rank * arrea);
    int stop(((rank + 1) * arrea ) - 1);
    
    if (rank == commSize - 1)
        stop = arrayLength - 1;

    for (int i = start; i < stop; i++)
        arr[i] = shift_left_cyclic(arr[i], i % 4 + 1);
}

void quicksort()
{

}
