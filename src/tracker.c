#include <mpi.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "bit_torrent.h"

/**
 * @brief Initalize swarms array, swarm contains info about file and peers
 * @param swarms swarm array
 * @param num_peers number of peers
 */
void initalize_swarms(swarm_t *swarms, int num_peers) {
	for (int i = 0; i < MAX_FILES; i++) {
		swarms[i].peers = calloc((num_peers + 1), sizeof(int));
		for (int j = 0; j < MAX_CHUNKS; j++) {
			swarms[i].peers_per_chunk[j] = calloc(num_peers, sizeof(int));
			swarms[i].file.chuck_present[j] = 0;
		}
	}
}

/**
 * @brief Update file info to swarms array, it is called when peer sends file info
 * @param swarms swarm array
 * @param num_files number of files
 * @param file_info file info to be added
 * @param sender_rank rank of peer that sent file info
 * @param num_peers number of peers
 */
void update_file_to_swarm(swarm_t *swarms, int *num_files, file_info_t *file_info, int sender_rank, int num_peers) {
	int file_index = -1;
	for (int i = 0; i < *num_files; i++) {
		if (strncmp(swarms[i].file.filename, file_info->filename, MAX_FILENAME) == 0) {
			file_index = i;
			break;
		}
	}
	if (file_index == -1) {
		file_index = *num_files;
		*num_files += 1;

		strncpy(swarms[file_index].file.filename, file_info->filename, MAX_FILENAME);
		swarms[file_index].file.chunks_count = file_info->chunks_count;
		for (int i = 0; i < file_info->chunks_count; i++) {
			if (file_info->chuck_present[i] == 1) {
				strncpy(swarms[file_index].file.hashes[i].str, file_info->hashes[i].str, HASH_SIZE);
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

/**
 * @brief Handle request for file info and peers,
 * it is called when peer requests file info and peers
 * @param status mpi status, used to get rank of peer that sent request
 * @param requested_file_name requested file name
 * @param swarms swarm array
 * @param num_peers number of peers
 * @param num_files number of files
 * @param mpi_datatypes mpi datatypes used for communication
 */
void handle_request_file_info_and_peers(MPI_Status status, char *requested_file_name,
	swarm_t *swarms, int num_peers, int num_files, mpi_datatypes_t *mpi_datatypes) {
	int file_index = -1;
	for (int i = 0; i < num_files; i++) {
		if (strncmp(swarms[i].file.filename, requested_file_name, MAX_FILENAME) == 0) {
			file_index = i;
			break;
		}
	}
	if (file_index == -1) {
		fprintf(stderr, "File %s not found\n", requested_file_name);
		return;
	}

	int recv_rank = status.MPI_SOURCE;

	int num_peers_per_chunk = (num_peers + 1) * swarms[file_index].file.chunks_count;
	int flattened_peers_per_chunk[num_peers_per_chunk];
	for (int i = 0; i < swarms[file_index].file.chunks_count; i++) {
		for (int j = 0; j < num_peers + 1; j++) {
			flattened_peers_per_chunk[i * (num_peers + 1) + j] = swarms[file_index].peers_per_chunk[i][j];
		}
	}

	MPI_Send(&swarms[file_index].file, 1, mpi_datatypes->mpi_file_info, recv_rank,
		TAG_TRACKER_FILE_INFO, MPI_COMM_WORLD);

	MPI_Send(flattened_peers_per_chunk, num_peers_per_chunk, MPI_INT, recv_rank, TAG_TRACKER_PEERS_PER_CHUNK, MPI_COMM_WORLD);
}

/**
 * @brief Handle update files info, it is called when peer sends files info
 * @param status mpi status, used to get rank of peer that sent request and count of files
 * @param files_info files info to be added
 * @param swarms swarm array
 * @param num_peers number of peers
 * @param num_files number of files
 * @param mpi_datatypes mpi datatypes used for communication
 */
void handle_update_files_info(MPI_Status status, file_info_t *files_info,
	swarm_t *swarms, int num_peers, int *num_files, mpi_datatypes_t *mpi_datatypes) {

	int num_updated_files = 0;
	MPI_Get_count(&status, mpi_datatypes->mpi_file_info, &num_updated_files);
	for (int i = 0; i < num_updated_files; i++) {
		update_file_to_swarm(swarms, num_files, &files_info[i], status.MPI_SOURCE, num_peers);
	}
}

/**
 * @brief Handle finished file, it is called when peer sends finished file
 * @param status mpi status, used to get rank of peer that sent request
 * @param finished_file_name finished file name
 * @param swarms swarm array
 * @param num_peers number of peers
 * @param num_files number of files
 * @param mpi_datatypes mpi datatypes used for communication
 */
void handle_finished_file(MPI_Status status, char *finished_file_name,
	swarm_t *swarms, int num_peers, int num_files, mpi_datatypes_t *mpi_datatypes) {
	int file_index = -1;
	for (int i = 0; i < num_files; i++) {
		if (strncmp(swarms[i].file.filename, finished_file_name, MAX_FILENAME) == 0) {
			file_index = i;
			break;
		}
	}
	if (file_index == -1) {
		fprintf(stderr, "File %s not found\n", finished_file_name);
		exit(-1);
	}

	int recv_rank = status.MPI_SOURCE;
	swarms[file_index].peers[recv_rank] = 1;
	for (int i = 0; i < swarms[file_index].file.chunks_count; i++) {
		swarms[file_index].peers_per_chunk[i][recv_rank] = 1;
	}
}

/**
 * @brief Check if all peers finished, it is called when peer sends finished all files
 * @param finished_peers finished peers array, 1 if peer finished, 0 otherwise
 * @param num_peers number of peers
 * @return 1 if all peers finished, 0 otherwise
 */
int check_if_all_peers_finished(int *finished_peers, int num_peers) {
	for (int i = 0; i < num_peers; i++) {
		if (finished_peers[i + 1] == 0)
			return 0;
	}
	return 1;
}

/**
 * @brief Tracker main function
 * @param numtasks number of peers + 1
 * @param rank rank of tracker
 * @param mpi_datatypes mpi datatypes used for communication
 */
void tracker(int numtasks, int rank, mpi_datatypes_t *mpi_datatypes) {
	int num_peers = numtasks - 1;

	int num_files = 0;
	swarm_t swarms[MAX_FILES];
	int finished_peers[num_peers + 1];
	initalize_swarms(swarms, num_peers);

	// receive files info from peers
	{
		file_info_t files_info[MAX_FILES];
		for (int i = 0; i < num_peers; i++) {
			finished_peers[i + 1] = 0;

			MPI_Status status;
			MPI_Recv(&files_info, MAX_FILES, mpi_datatypes->mpi_file_info, MPI_ANY_SOURCE,
				TAG_PEER_UPDATE_FILES_INFO, MPI_COMM_WORLD, &status);
			int count;
			int sender_rank = status.MPI_SOURCE;
			MPI_Get_count(&status, mpi_datatypes->mpi_file_info, &count);
			for (int j = 0; j < count; j++) {
				update_file_to_swarm(swarms, &num_files, &files_info[j], sender_rank, num_peers);
			}
		}
	}

	// Signal peers that tracker has received all files info
	MPI_Barrier(MPI_COMM_WORLD);
	// Main loop
	{
		MPI_Request requests[NUM_REQUEST_TYPES];
		char requested_file_name[MAX_FILENAME];
		char finished_file_name[MAX_FILENAME];
		file_info_t files_info[MAX_FILENAME];

		MPI_Irecv(requested_file_name, MAX_FILENAME, MPI_CHAR, MPI_ANY_SOURCE,
			TAG_PEER_REQUEST_FILE_INFO_AND_PEERS, MPI_COMM_WORLD, &requests[REQ_TYPE_FILE_INFO]);

		MPI_Irecv(files_info, MAX_FILES, mpi_datatypes->mpi_file_info, MPI_ANY_SOURCE,
			TAG_PEER_UPDATE_FILES_INFO, MPI_COMM_WORLD, &requests[REQ_TYPE_UPDATE_FILES]);

		MPI_Irecv(finished_file_name, MAX_FILENAME, MPI_CHAR, MPI_ANY_SOURCE,
			TAG_PEER_FINISHED_FILE, MPI_COMM_WORLD, &requests[REQ_TYPE_FINISHED_FILE]);

		MPI_Irecv(NULL, 0, MPI_INT, MPI_ANY_SOURCE,
			TAG_PEER_FINISHED_ALL_FILES, MPI_COMM_WORLD, &requests[REQ_TYPE_FINISHED_ALL]);

		while (1) {
			// Receive requests from peers
			int index;
			MPI_Status status;

			MPI_Waitany(NUM_REQUEST_TYPES, requests, &index, &status);

			switch (index) {
			case REQ_TYPE_FILE_INFO:
				handle_request_file_info_and_peers(status, requested_file_name, swarms, num_peers, num_files, mpi_datatypes);

				MPI_Irecv(requested_file_name, MAX_FILENAME, MPI_CHAR, MPI_ANY_SOURCE,
					TAG_PEER_REQUEST_FILE_INFO_AND_PEERS, MPI_COMM_WORLD, &requests[REQ_TYPE_FILE_INFO]);
				break;
			case REQ_TYPE_UPDATE_FILES:
				handle_update_files_info(status, files_info, swarms, num_peers, &num_files, mpi_datatypes);

				MPI_Irecv(files_info, MAX_FILES, mpi_datatypes->mpi_file_info, MPI_ANY_SOURCE,
					TAG_PEER_UPDATE_FILES_INFO, MPI_COMM_WORLD, &requests[REQ_TYPE_UPDATE_FILES]);
				break;
			case REQ_TYPE_FINISHED_FILE:
				handle_finished_file(status, finished_file_name, swarms, num_peers, num_files, mpi_datatypes);

				MPI_Irecv(finished_file_name, MAX_FILENAME, MPI_CHAR, MPI_ANY_SOURCE,
					TAG_PEER_FINISHED_FILE, MPI_COMM_WORLD, &requests[REQ_TYPE_FINISHED_FILE]);

				break;
			case REQ_TYPE_FINISHED_ALL:
				finished_peers[status.MPI_SOURCE] = 1;
				if (check_if_all_peers_finished(finished_peers, num_peers) == 1) {
					for (int i = 0; i < num_peers; i++) {
						MPI_Send(NULL, 0, MPI_INT, i + 1, TAG_TRACKER_END, MPI_COMM_WORLD);
					}
					return;
				}

				MPI_Irecv(NULL, 0, MPI_INT, MPI_ANY_SOURCE,
					TAG_PEER_FINISHED_ALL_FILES, MPI_COMM_WORLD, &requests[REQ_TYPE_FINISHED_ALL]);
			}
		}
	}
}