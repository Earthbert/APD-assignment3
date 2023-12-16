#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "bit_torrent.h"

mpi_datatypes_t *create_mpi_datatypes() {
	mpi_datatypes_t *mpi_datatypes = malloc(sizeof(mpi_datatypes_t));

	// Create mpi_hash datatype
	{
		int blocklengths[] = { HASH_SIZE };
		MPI_Aint displacements[1];
		displacements[0] = offsetof(hash, str);
		MPI_Datatype types[] = { MPI_CHAR };
		MPI_Type_create_struct(1, blocklengths, displacements, types, &mpi_datatypes->mpi_hash);
		MPI_Type_commit(&mpi_datatypes->mpi_hash);
	}

	// Create mpi_file_info datatype
	{
		int blocklengths[] = { MAX_FILENAME, MAX_CHUNKS, MAX_CHUNKS, 1 };
		MPI_Aint displacements[4];
		displacements[0] = offsetof(file_info_t, filename);
		displacements[1] = offsetof(file_info_t, chuck_present);
		displacements[2] = offsetof(file_info_t, hashes);
		displacements[3] = offsetof(file_info_t, chunks_count);
		MPI_Datatype types[] = { MPI_CHAR, MPI_CHAR, mpi_datatypes->mpi_hash, MPI_INT };
		MPI_Type_create_struct(4, blocklengths, displacements, types, &mpi_datatypes->mpi_file_info);
		MPI_Type_commit(&mpi_datatypes->mpi_file_info);
	}

	// Create mpi_files_info datatype
	{
		int blocklengths[] = { 1, MAX_FILES };
		MPI_Aint displacements[2];
		displacements[0] = offsetof(mpi_files_info_t, num_files);
		displacements[1] = offsetof(mpi_files_info_t, files);
		MPI_Datatype types[] = { MPI_INT, mpi_datatypes->mpi_file_info };
		MPI_Type_create_struct(2, blocklengths, displacements, types, &mpi_datatypes->mpi_files_info);
		MPI_Type_commit(&mpi_datatypes->mpi_files_info);
	}

	return mpi_datatypes;
}

int main(int argc, char *argv[]) {
	int numtasks, rank;

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	mpi_datatypes_t *mpi_datatypes = create_mpi_datatypes();

	if (rank == TRACKER_RANK) {
		tracker(numtasks, rank, mpi_datatypes);
	} else {
		peer(numtasks, rank, mpi_datatypes);
	}

	MPI_Finalize();
}
