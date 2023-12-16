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
	thread_args->rank = rank;
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

void send_info_about_files(int rank, int numtasks, mpi_datatypes_t *mpi_datatypes, file_info_t *files, int num_files) {
	mpi_files_info_t files_info;
	files_info.num_files = num_files;
	for (int i = 0; i < num_files; i++) {
		memcpy(&files_info.files[i], &files[i], sizeof(file_info_t));
	}

	MPI_Send(&files_info, 1, mpi_datatypes->mpi_files_info, TRACKER_RANK, SEND_ALL_FILES_INFO, MPI_COMM_WORLD);
}

void *download_thread_func(void *arg) {
	
	return NULL;
}

void *upload_thread_func(void *arg) {

	return NULL;
}

void peer(int numtasks, int rank, mpi_datatypes_t *mpi_datatypes) {
	pthread_t download_thread;
	pthread_t upload_thread;
	void *status;
	int r;

	args_t *thread_args = prepare_thread_args(rank, numtasks, mpi_datatypes);

	send_info_about_files(rank, numtasks, mpi_datatypes, thread_args->files, thread_args->num_files);

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

