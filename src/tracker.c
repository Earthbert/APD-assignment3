#include <mpi.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "bit_torrent.h"

void initalize_swarms(swarm_t *swarms, int num_peers) {
	for (int i = 0; i < MAX_FILES; i++) {
		swarms[i].peers = calloc((num_peers + 1), sizeof(int));
		for (int j = 0; j < MAX_CHUNKS; j++) {
			swarms[i].peers_per_chunk[j] = calloc(num_peers, sizeof(int));
			swarms[i].file.chuck_present[j] = 0;
		}
	}
}

void update_file_to_swarm(swarm_t *swarms, int *num_files, file_info_t *file_info, int sender_rank, int num_peers) {
	int file_index = -1;
	for (int i = 0; i < *num_files; i++) {
		if (strcmp(swarms[i].file.filename, file_info->filename) == 0) {
			file_index = i;
			break;
		}
	}
	if (file_index == -1) {
		file_index = *num_files;
		*num_files += 1;

		strcpy(swarms[file_index].file.filename, file_info->filename);
		swarms[file_index].file.chunks_count = file_info->chunks_count;
		for (int i = 0; i < file_info->chunks_count; i++) {
			if (file_info->chuck_present[i] == 1) {
				strcpy(swarms[file_index].file.hashes[i].str, file_info->hashes[i].str);
			}
		}
	}
	swarms[file_index].peers[sender_rank] = 1;
	for (int i = 0; i < file_info->chunks_count; i++) {
		if (file_info->chuck_present[i] == 1) {
			swarms[file_index].peers_per_chunk[i][sender_rank] = 1;
		}
	}
}

void tracker(int numtasks, int rank, mpi_datatypes_t *mpi_datatypes) {
	int num_peers = numtasks - 1;

	int num_files = 0;
	swarm_t swarms[MAX_FILES];
	initalize_swarms(swarms, num_peers);

	// receive files info from peers
	for (int i = 0; i < num_peers; i++) {
		mpi_files_info_t files_info;
		MPI_Status status;
		MPI_Recv(&files_info, 1, mpi_datatypes->mpi_files_info, MPI_ANY_SOURCE,
			TAG_PEER_UPDATE_FILES_INFO, MPI_COMM_WORLD, &status);
		int sender_rank = status.MPI_SOURCE;
		for (int j = 0; j < files_info.num_files; j++) {
			update_file_to_swarm(swarms, &num_files, &files_info.files[j], sender_rank, num_peers);
		}
	}

	// Signal peers that tracker has received all files info
	MPI_Bcast(NULL, 0, MPI_INT, TRACKER_RANK, MPI_COMM_WORLD);
}