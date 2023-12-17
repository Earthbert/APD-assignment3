#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <pthread.h>

#include "bit_torrent.h"

args_t *prepare_thread_args(int rank, int numtasks, mpi_datatypes_t *mpi_datatypes) {
	char filename[MAX_FILENAME] = "in";
	sprintf(filename, "in%d.txt", rank);

	FILE *f = fopen(filename, "r");
	if (f == NULL) {
		printf("Eroare la deschiderea fisierului %s\n", filename);
		exit(-1);
	}

	args_t *thread_args = malloc(sizeof(args_t));
	thread_args->num_peers = numtasks - 1;
	thread_args->mpi_datatypes = mpi_datatypes;

	fscanf(f, "%d", &thread_args->num_files);
	file_info_t *files = malloc(thread_args->num_files * sizeof(file_info_t));
	for (int i = 0; i < thread_args->num_files; i++) {
		fscanf(f, "%s", files[i].filename);
		fscanf(f, "%d", &files[i].chunks_count);
		for (int j = 0; j < files[i].chunks_count; j++) {
			fscanf(f, "%s", files[i].hashes[j].str);
			files[i].chuck_present[j] = 1;
		}
	}
	thread_args->files = files;

	fscanf(f, "%d", &thread_args->num_files_to_download);
	file_info_t *files_to_download = malloc(thread_args->num_files_to_download * sizeof(file_info_t));
	for (int i = 0; i < thread_args->num_files_to_download; i++) {
		fscanf(f, "%s", files_to_download[i].filename);
		files_to_download[i].chunks_count = 0;
		for (int j = 0; j < MAX_CHUNKS; j++) {
			files_to_download[i].chuck_present[j] = 0;
		}
	}
	thread_args->files_to_download = files_to_download;

	fclose(f);
	return thread_args;
}

void write_file_to_disk(file_info_t *file) {
	int rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	char filename[2 * MAX_FILENAME];
	sprintf(filename, "client%d_%s", rank, file->filename);

	FILE *f = fopen(filename, "w");
	if (f == NULL) {
		printf("Eroare la deschiderea fisierului %s\n", filename);
		exit(-1);
	}

	for (int i = 0; i < file->chunks_count; i++) {
		fprintf(f, "%.32s\n", file->hashes[i].str);
	}

	fclose(f);
}

void send_info_about_files(int rank, mpi_datatypes_t *mpi_datatypes, file_info_t *files, int num_files) {
	file_info_t files_info[MAX_FILENAME];
	for (int i = 0; i < num_files; i++) {
		memcpy(&files_info[i], &files[i], sizeof(file_info_t));
	}

	MPI_Send(&files_info, num_files, mpi_datatypes->mpi_file_info, TRACKER_RANK, TAG_PEER_UPDATE_FILES_INFO, MPI_COMM_WORLD);
}

void get_info_about_wanted_files(file_info_t *files_to_download, int num_files_to_download,
	int *finished_downloading, int ***peer_per_chunk, int num_peers, mpi_datatypes_t *mpi_datatypes) {

	int rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	for (int i = 0; i < num_files_to_download; i++) {
		if (finished_downloading[i])
			continue;
		MPI_Send(files_to_download[i].filename, MAX_FILENAME, MPI_CHAR, TRACKER_RANK,
			TAG_PEER_REQUEST_FILE_INFO_AND_PEERS, MPI_COMM_WORLD);
	}

	{
		int max_peers_per_chunk = (num_peers + 1) * MAX_CHUNKS;
		int flattened_peers_per_chunk[max_peers_per_chunk];

		for (int i = 0; i < num_files_to_download; i++) {
			if (finished_downloading[i])
				continue;

			// Receive file info
			file_info_t file_info;
			MPI_Recv(&file_info, 1, mpi_datatypes->mpi_file_info, TRACKER_RANK,
				TAG_TRACKER_FILE_INFO, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

			// Copy file info
			files_to_download[i].chunks_count = file_info.chunks_count;
			strncpy(files_to_download[i].filename, file_info.filename, MAX_FILENAME);
			for (int j = 0; j < files_to_download[i].chunks_count; j++) {
				strncpy(files_to_download[i].hashes[j].str, file_info.hashes[j].str, HASH_SIZE);
			}

			// Receive peers info
			MPI_Status status;
			MPI_Recv(flattened_peers_per_chunk, max_peers_per_chunk, MPI_INT,
				TRACKER_RANK, TAG_TRACKER_PEERS_PER_CHUNK, MPI_COMM_WORLD, &status);

			// Copy peers info
			int count;
			MPI_Get_count(&status, MPI_INT, &count);
			int num_chunks = count / (num_peers + 1);
			for (int j = 0; j < num_chunks; j++) {
				for (int k = 0; k < num_peers + 1; k++) {
					peer_per_chunk[i][j][k] = flattened_peers_per_chunk[j * (num_peers + 1) + k];
				}
			}
		}
	}
}

int check_if_finished_downloading_file(file_info_t file) {
	for (int i = 0; i < file.chunks_count; i++) {
		if (file.chuck_present[i] == 0)
			return 0;
	}
	return 1;
}

void download_files(file_info_t *file_to_download, int num_files_to_download,
	int *finished_downloading, int ***peer_per_chunk, int *last_peer_used,
	int num_peers, mpi_datatypes_t *mpi_datatypes, int *end_job) {

	int rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	int num_downloaded_chunks = 0;
	for (int i = 0; i < num_files_to_download; i++) {
		if (num_downloaded_chunks == UPDATE_INTERVAL)
			break;

		if (finished_downloading[i])
			continue;

		for (int j = 0; j < file_to_download[i].chunks_count; j++) {
			if (num_downloaded_chunks == UPDATE_INTERVAL)
				break;

			if (file_to_download[i].chuck_present[j])
				continue;

			// Find next peer to download from
			int peer_index = last_peer_used[i];
			while (peer_per_chunk[i][j][peer_index] == 0) {
				peer_index = (peer_index + 1) % num_peers;
			}
			last_peer_used[i] = peer_index;

			// Request chunk from peer
			peer_request_t peer_request;
			strncpy(peer_request.filename, file_to_download[i].filename, MAX_FILENAME);
			strncpy(peer_request.hash.str, file_to_download[i].hashes[j].str, HASH_SIZE);
			MPI_Send(&peer_request, 1, mpi_datatypes->mpi_peer_request,
				peer_index, TAG_PEER_REQUEST_CHUNK, MPI_COMM_WORLD);

			// Receive chunk from peer
			int received_chunk;
			MPI_Recv(&received_chunk, 1, MPI_INT, peer_index, TAG_PEER_FILE_REQ_RESPONSE,
				MPI_COMM_WORLD, MPI_STATUS_IGNORE);

			if (received_chunk) {
				file_to_download[i].chuck_present[j] = 1;
				num_downloaded_chunks++;
			}
		}
	}

	int has_all_files = 1;
	for (int i = 0; i < num_files_to_download; i++) {
		// Check if finished downloading file
		if (check_if_finished_downloading_file(file_to_download[i])) {
			finished_downloading[i] = 1;
			MPI_Send(&file_to_download[i].filename, MAX_FILENAME, MPI_CHAR, TRACKER_RANK,
				TAG_PEER_FINISHED_FILE, MPI_COMM_WORLD);
			write_file_to_disk(&file_to_download[i]);
		} else
			has_all_files = 0;
	}

	if (has_all_files) {
		*end_job = 1;
		MPI_Send(NULL, 0, MPI_INT, TRACKER_RANK, TAG_PEER_FINISHED_ALL_FILES, MPI_COMM_WORLD);
	}
}

void *download_thread_func(void *arg) {
	args_t *thread_args = (args_t *)arg;
	int end_job = 0;

	int last_peer_used[MAX_FILES] = { 0 };
	int finished_downloading[MAX_FILES] = { 0 };

	int rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	int ***peer_per_chunk = malloc(sizeof(int **) * MAX_FILES);
	for (int i = 0; i < MAX_FILES; i++) {
		peer_per_chunk[i] = malloc(sizeof(int *) * MAX_CHUNKS);
		for (int j = 0; j < MAX_CHUNKS; j++) {
			peer_per_chunk[i][j] = malloc(sizeof(int) * (thread_args->num_peers + 1));
		}
	}

	for (int i = 0; i < thread_args->num_files; i++) {
		for (int j = 0; j < thread_args->files[i].chunks_count; j++) {
			for (int k = 0; k < thread_args->num_peers + 1; k++) {
				peer_per_chunk[i][j][k] = 0;
			}
		}
	}

	while (!end_job) {
		// get info about wanted files
		get_info_about_wanted_files(thread_args->files_to_download, thread_args->num_files_to_download,
			finished_downloading, peer_per_chunk, thread_args->num_peers, thread_args->mpi_datatypes);

		// download files
		download_files(thread_args->files_to_download, thread_args->num_files_to_download,
			finished_downloading, peer_per_chunk, last_peer_used, thread_args->num_peers, thread_args->mpi_datatypes, &end_job);

		// update tracker with info about downloaded segments
		send_info_about_files(rank, thread_args->mpi_datatypes,
			thread_args->files_to_download, thread_args->num_files_to_download);
	}

	return NULL;
}

void handle_peer_chunk_request(args_t *thread_args, MPI_Status status, peer_request_t peer_request) {
	int rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	int found = 0;
	for (int i = 0; i < thread_args->num_files; i++) {
		if (strncmp(thread_args->files[i].filename, peer_request.filename, MAX_FILENAME) == 0) {
			for (int j = 0; j < thread_args->files[i].chunks_count; j++) {
				if (strncmp(thread_args->files[i].hashes[j].str,
					peer_request.hash.str, HASH_SIZE) == 0) {
					found = 1;
					break;
				}
			}
			break;
		}
	}

	for (int i = 0; i < thread_args->num_files_to_download; i++) {
		if (strncmp(thread_args->files_to_download[i].filename, peer_request.filename, MAX_FILENAME) == 0) {
			for (int j = 0; j < thread_args->files_to_download[i].chunks_count; j++) {
				if (strncmp(thread_args->files_to_download[i].hashes[j].str,
					peer_request.hash.str, HASH_SIZE) == 0 && thread_args->files_to_download[i].chuck_present[j] == 1) {
					found = 1;
					break;
				}
			}
			break;
		}
	}

	MPI_Send(&found, 1, MPI_INT, status.MPI_SOURCE, TAG_PEER_FILE_REQ_RESPONSE, MPI_COMM_WORLD);
}

void *upload_thread_func(void *arg) {
	args_t *thread_args = (args_t *)arg;
	MPI_Request request[NUM_PEER_REQUEST_TYPES];
	peer_request_t peer_request;
	MPI_Irecv(&peer_request, 1, thread_args->mpi_datatypes->mpi_peer_request, MPI_ANY_SOURCE,
		TAG_PEER_REQUEST_CHUNK, MPI_COMM_WORLD, &request[REQ_TYPE_CHUNK]);

	MPI_Irecv(NULL, 0, MPI_INT, MPI_ANY_SOURCE,
		TAG_TRACKER_END, MPI_COMM_WORLD, &request[REQ_TYPE_FINISH]);

	while (1) {
		int index;
		MPI_Status status;
		MPI_Waitany(NUM_PEER_REQUEST_TYPES, request, &index, &status);

		switch (index) {
		case REQ_TYPE_CHUNK:
			handle_peer_chunk_request(thread_args, status, peer_request);
			MPI_Irecv(&peer_request, 1, thread_args->mpi_datatypes->mpi_peer_request, MPI_ANY_SOURCE,
				TAG_PEER_REQUEST_CHUNK, MPI_COMM_WORLD, &request[REQ_TYPE_CHUNK]);
			break;
		case REQ_TYPE_FINISH:
			return NULL;
		}
	}
	return NULL;
}

void peer(int numtasks, int rank, mpi_datatypes_t *mpi_datatypes) {
	pthread_t download_thread;
	pthread_t upload_thread;
	void *status;
	int r;

	args_t *thread_args = prepare_thread_args(rank, numtasks, mpi_datatypes);

	send_info_about_files(rank, mpi_datatypes, thread_args->files, thread_args->num_files);

	MPI_Barrier(MPI_COMM_WORLD);

	r = pthread_create((void *)&download_thread, NULL, download_thread_func, thread_args);
	if (r) {
		printf("Eroare la crearea thread-ului de download\n");
		exit(-1);
	}

	r = pthread_create((void *)&upload_thread, NULL, upload_thread_func, thread_args);
	if (r) {
		printf("Eroare la crearea thread-ului de upload\n");
		exit(-1);
	}

	r = pthread_join(download_thread, &status);
	if (r) {
		printf("Eroare la asteptarea thread-ului de download\n");
		exit(-1);
	}

	r = pthread_join(upload_thread, &status);
	if (r) {
		printf("Eroare la asteptarea thread-ului de upload\n");
		exit(-1);
	}
}

