// QuickSort.cpp: Definiert den Einstiegspunkt für die Konsolenanwendung.
//

#include "stdafx.h"
#include "mpi.h"


int main(int argc, char* argv[])
{
	MPI_Init(&argc, &argv);

	int rank(0);

	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	if (rank == 0)
	{
		char hello[] {"Hello World"};
		MPI_Send(hello, _countof(hello), MPI_CHAR, 1, 0, MPI_COMM_WORLD);
	}
	else if (rank == 1)
	{
		char hello[12];

		MPI_Recv(hello, _countof(hello), MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		printf("Rank 1 received \"%s\" from rank 0", hello);
	}

	MPI_Finalize();

    return 0;
}

