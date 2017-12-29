// QuickSort.cpp: Definiert den Einstiegspunkt für die Konsolenanwendung.
//

#include "stdafx.h"
#include "mpi.h"

#define N 200
#define K 1

#define arrayType unsigned long

/////////////////////////// forward declararions //////////////////////////////
///initialises array, as stated by the task description
void initArray(arrayType * arr, int n);

///modifies array, as stated by the task description
void modifyArray(arrayType * arr, int arrayLength, int rank, int commSize);

///implementation of the quicksort algorithm
void quicksort(arrayType arr [], int length);

///creates a new communication group for smaller segments of the array
void getNewGroup(int commRank, int commRatio, int * numProcs, int * &ranks, int commSize);

///calculates the sum over all elements that are smaller than a median. (allNumSmaller only contains smaller elements)
void reduceSmaller(int commSize, int &offset, int * allNumSmaller);

///calculates the sum over all the ammounts of median elements
void reduceMedians(int commSize, int &totalNumMedians, int * allNumMedian);

///sorts a list containing all the information into 3 seperate lists. 
///new lists only contain per process informations on number of elements smaller, equal to, or larger than median elements
void sortElementSizes(int commSize, int * allNumLarger, int * totalElemCounts, int * allNumMedian, int * allNumSmaller);

///implements the very last step of the sorting algorithm, bcasts finaly sorted portion of the array. root has final result
void sequentialSort(arrayType * arr, int length);

///helper function, finds median-element of the first 'length' elements in array arr
arrayType findMedian(arrayType * arr, int length = 3);

///checks if array arr has been sorted properly. returns true if sorted, false otherwise
bool checkIsSorted(arrayType * arr, int length);

///returns true if every entry in checks is true. returns false if a single value is false
bool reduceChecks(bool * checks, int length);

////////////////////////////// global variables ///////////////////////////////
static std::stack<MPI_Comm> commStack;
static arrayType * globalArray;
static int globalSize = 0; //< only needed for debuging
static int recursionDepth = 0; //< only needed for debuging

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
	std::cout << std::endl << std::endl;
}

/////////////////////////////// main function /////////////////////////////////
int main(int argc, char* argv[])
{
	MPI_Init(&argc, &argv);

	int rank(0);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    arrayType arr[N];

	globalArray = arr;
    globalSize = N;

	if (rank == 0)
	{
        initArray(arr, N);
//#ifdef _DEBUG
//        printArray(arr, N);
//#endif
	}

    MPI_Bcast(arr, N, MPI_INT32_T, 0, MPI_COMM_WORLD);

    int size;
    MPI_Comm_size(MPI_COMM_WORLD, &size);

#ifdef _DEBUG
	bool * checks = new bool[K];
	memset(checks, false, K);
#endif // _DEBUG

    for (int i = 0; i < K; i++)
    {
        commStack.push(MPI_COMM_WORLD);
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
        int i;
        std::cin >> i;
	}
#endif // _DEBUG

	MPI_Finalize();
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

void quicksort(arrayType arr [], int length)
{
#ifdef _DEBUG
    recursionDepth++;
    std::cout << "recursionDepth: " << recursionDepth << std::endl;
#endif // _DEBUG

    MPI_Comm & currentComm( commStack.top()) ;
	arrayType median(findMedian(arr));
    //distribute sections to threads
    int commSize(0);
    MPI_Comm_size(currentComm, &commSize);
    int sectionSize(length / commSize);
    
    int commRank(0);
    MPI_Comm_rank(currentComm, &commRank);
    int start(commRank * sectionSize);

    int iterationLimit(commRank < commSize - 1 ? sectionSize - 1 : length - 1);
    //sort section
	int head(0);
    int tail(sectionSize - 1);

    arrayType * tmp = new arrayType[sectionSize];
    for (int i = start; i < iterationLimit; i++)
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
	
	int numSmallerElems(head);
    int numMedians(tail - head);
	int numLargerElems(sectionSize - (numSmallerElems + numMedians));

    for (; head < tail; head++)
    {
        tmp[head] = median;
    }

//#ifdef _DEBUG
//    MPI_Comm_size(MPI_COMM_WORLD, &commSize);
//    for (int j(0); j < commSize; j++)
//    {
//        if (j == commRank)
//        {
//            std::cout << "numSmallerElems: " << numSmallerElems << "   numMedians: " << numMedians << "   numLargerElems: " << numLargerElems;
//			std::cout << std::endl << std::endl;
//        }
//        MPI_Barrier(MPI_COMM_WORLD);
//    }
//    
//#endif

	//gather results: #elems smaller, #medians, #elems larger 
	int sendElems[3] = { numSmallerElems, numMedians, numLargerElems };
	int * totalElemCounts = new int[3 * commSize];
	int result (MPI_Allgather(&sendElems, 3, MPI_INT32_T, totalElemCounts, 3, MPI_INT32_T, currentComm));
	
//#ifdef _DEBUG
//	if (result != MPI_SUCCESS)
//	{
//		std::cout << "First gather failed" << std::endl;
//		return;
//	}
//	else
//	{
//        for (int j(0); j < commSize; j++)
//        {
//            if (j == commRank)
//            {
//                std::cout << "Rank: " << commRank << std::endl;
//		        std::cout << "First gather succeeded" << std::endl;
//                std::cout << "totalElemCounts[" << 0 << "]: " << totalElemCounts[0];
//                for (int i = 1; i < 3 * commSize; i++)
//                {
//                    if (i % 3 == 0)
//                        std::cout << std::endl;
//                    else
//                        std::cout << "   ";
//                    std::cout << "totalElemCounts[" << i << "]: " << totalElemCounts[i];
//                }
//                std::cout << std::endl << std::endl;
//            }
//            MPI_Barrier(MPI_COMM_WORLD);
//        }
//	}
//#endif // _DEBUG

	int * allNumSmaller = new int[commSize];
	int * allNumMedian = new int[commSize];
	int * allNumLarger = new int[commSize];

	sortElementSizes(commSize, allNumLarger, totalElemCounts, allNumMedian, allNumSmaller);

	int * displs = new int[commSize];
	memset(displs, 0x0, sizeof(int) * commSize);

//#ifdef _DEBUG
//	int globalSize(0);
//	int globalCommRank(0);
//	MPI_Comm_size(MPI_COMM_WORLD, &globalSize);
//	MPI_Comm_rank(MPI_COMM_WORLD, &globalCommRank);
//	for (int i = 0; i < globalSize; i++)
//	{
//		if (globalCommRank == i)
//		{
//			std::cout << "globalSize: " << globalSize << "   globalCommRank: " << globalCommRank << std::endl;
//			std::cout << "tmp length: " << sectionSize << std::endl;
//			for (int j = 0; j < sectionSize; j++)
//			{
//				std::cout << tmp[j] << " ";
//			}
//			std::cout << std::endl << std::endl;
//			std::cout << "numSmallerElems: " << numSmallerElems << "   arr length: " << length << "   allNumSmaller: " << allNumSmaller << std::endl;
//			
//			for (int j = 0; j < commSize; j++)
//			{
//				std::cout << allNumSmaller[j] << " ";
//			}
//			
//			std::cout << std::endl << std::endl;
//			std::cout << "displs: " << displs << "   currentComm: " << currentComm << std::endl;
//		}
//		MPI_Barrier(MPI_COMM_WORLD);
//	}
//
//#endif // _DEBUG

	result = MPI_Allgatherv(tmp, numSmallerElems, MPI_INT32_T, arr, allNumSmaller, displs, MPI_INT32_T, currentComm);

//#ifdef _DEBUG
//	if (result != MPI_SUCCESS)
//	{
//		std::cout << "Second gather failed" << std::endl;
//		return;
//	}
//	else
//	{
//		std::cout << "Second gather succeeded" << std::endl;
//	}
//#endif // _DEBUG

	int globalOffset(0);
	reduceSmaller(commSize, globalOffset, allNumSmaller);

	int totalNumMedians(0);
	reduceMedians(commSize, totalNumMedians, allNumMedian);
	
	int limit(globalOffset + totalNumMedians);
	for (; globalOffset < limit; globalOffset++)
	{
		arr[globalOffset - 1] = median;
	}
	int localOffset(numSmallerElems + numMedians);

//#ifdef _DEBUG
//	for (int i = 0; i < globalSize; i++)
//	{
//		if (globalCommRank == i)
//			std::cout << "tmp + localOffset: " << tmp + localOffset << "   numLargerElems: " << numLargerElems << "   arr + (globalOffset - 1): " << arr + (globalOffset - 1) << "   allNumLarger: " << allNumLarger << "   displs: " << displs << "   currentComm : " << currentComm << std::endl;
//		MPI_Barrier(MPI_COMM_WORLD);
//	}
//
//#endif // _DEBUG

	result = MPI_Allgatherv(tmp + localOffset, numLargerElems, MPI_INT32_T, arr + (globalOffset - 1), allNumLarger, displs, MPI_INT32_T, currentComm);
	
//#ifdef _DEBUG
//	if (result != MPI_SUCCESS)
//	{
//		std::cout << "Third gather failed" << std::endl;
//		return;
//	}
//	else
//	{
//		std::cout << "Third gather succeeded" << std::endl;
//	}
//#endif // _DEBUG	

	delete[] tmp;
	tmp = nullptr;

	//create communication group for next iteration
	double elementRatio(globalOffset / sectionSize);
	int commRatio(commSize * elementRatio);
	
	if (commRatio == 0)
		commRatio = 1;

	MPI_Group currentGroup(0);
	MPI_Comm_group(currentComm, &currentGroup);

	MPI_Group newGroup(0);
	int * numProcs(new int);
	int * ranks(new int);
	
	getNewGroup(commRank, commRatio, numProcs, ranks, commSize);

	MPI_Group_incl(currentGroup, *numProcs, ranks, &newGroup);

	MPI_Comm newComm;
	MPI_Comm_create(currentComm, newGroup, &newComm);
	commStack.push(newComm);
    
	//next iteration smaler sections
	int newCommSize(0);
	MPI_Comm_size(newComm, &newCommSize);

	if (commRank < commRatio)
	{
		if (newCommSize > 1)
			quicksort(arr, globalOffset);
		else
			sequentialSort(arr, globalOffset);
	}
	else
	{
		if(newCommSize > 1)
			quicksort(arr + globalOffset, sectionSize - globalOffset);
		else
			sequentialSort(arr + globalOffset, sectionSize - globalOffset);
	}
#ifdef _DEBUG
    recursionDepth--;
    std::cout << "recursionDepth: " << recursionDepth << std::endl;
#endif // _DEBUG
}

void getNewGroup(int commRank, int commRatio, int * numProcs, int * &ranks, int commSize)
{
	if (commRank < commRatio)
	{
		*numProcs = commRatio;
		ranks = new int[*numProcs];
		for (int i = 0; i < *numProcs; i++)
		{
			ranks[i] = i;
		}
	}
	else
	{
		*numProcs = commSize - commRatio;
        if (*numProcs == 0)
            *numProcs = 1;
		ranks = new int[*numProcs];
		for (int i = 0; i < *numProcs; i++)
		{
			ranks[i] = i + commRatio;
		}
	}
}

void reduceSmaller(int commSize, int &offset, int * allNumSmaller)
{
	for (int i = 0; i < commSize; i++)
	{
		offset += allNumSmaller[i];
	}
}

void reduceMedians(int commSize, int &totalNumMedians, int * allNumMedian)
{
	for (int i = 0; i < commSize; i++)
	{
		totalNumMedians += allNumMedian[i];
	}
}

void sortElementSizes(int commSize, int * allNumLarger, int * totalElemCounts, int * allNumMedian, int * allNumSmaller)
{
	int iterSmall = 0;
	int iterMedian = 0;
	int iterLarge = 0;

	for (int i = 0; i < 3 * commSize; i++)
	{
		if (i % 3 == 0)
		{
			allNumSmaller[iterSmall] = totalElemCounts[i];
			iterSmall++;		
		}
		else if (i % 3 == 1)
		{
			allNumMedian[iterMedian] = totalElemCounts[i];
			iterMedian++;
		}
		else
		{
			allNumLarger[iterLarge] = totalElemCounts[i];
			iterLarge++;
		}
	}
}

void sequentialSort(arrayType * arr, int length)
{
	int globalRank(0);
	int commSize(0);
	
	MPI_Comm_size(MPI_COMM_WORLD, &commSize);
    MPI_Comm_rank(MPI_COMM_WORLD, &globalRank);
	
#ifdef _DEBUG
    for (int i = 0; i < commSize; i++)
    {
        if (i == globalRank)
        {
            std::cout << "Rank " << i << " entered sequentialSort." << std::endl;
        }
        MPI_Barrier(MPI_COMM_WORLD);
    }
#endif // _DEBUG

    int head(0);
	int tail(length - 1);

	arrayType median = findMedian(arr);
	arrayType * tmp = new arrayType[length];

	for (int i = 0; i < length - 1; i++)
	{
		arrayType & tmpElem(arr[i]);
		if (tmpElem < median)
		{
			tmp[head] = tmpElem;
			head++;
		}
		else if (tmpElem > median)
		{
			tmp[tail] = tmpElem;
			tail--;
		}
	}

	for (; head < tail; head++)
	{
		tmp[head] = median;
	}

	int * totalLengths = new int[commSize];

	//gather final result at process 0
	int * displs = new int[commSize];
	memset(displs, 0x0, sizeof(int) * commSize);
	MPI_Gather(&length, 1, MPI_INT32_T, totalLengths, 1, MPI_INT32_T, 0, MPI_COMM_WORLD);

#ifdef _DEBUG
    std::cout << "Rank: " << globalRank << std::endl;
    if (globalRank == 0)
    {
        std::cout << "commSize: " << commSize << std::endl;
        for (int i = 0; i < commSize; i++)
        {
            std::cout << totalLengths[i] << std::endl;
        }
    }
    MPI_Barrier(MPI_COMM_WORLD);
#endif // _DEBUG
	
    MPI_Gatherv(tmp, length, MPI_INT32_T, globalArray, totalLengths, displs, MPI_INT32_T, 0, MPI_COMM_WORLD);
#ifdef _DEBUG
    if (globalRank == 0)
    {
        printArray(globalArray, globalSize);
    }
#endif // _DEBUG

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
	return arr[1];
}

bool checkIsSorted(arrayType * arr, int length)
{
	for (int i = 0; i < length - 1; i++)
	{
        if (arr[i] > arr[i + 1])
        {
            std::cout << "Position #" << i << " is larger than " << i + 1 << std::endl;
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
