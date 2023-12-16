#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <pthread.h>

#include "bit_torrent.h"

args *prepare_thread_args(int rank, int numtasks) {
	char filename[MAX_FILENAME] = "in";
	sprintf(filename, "in%d.txt", rank);

	FILE *f = fopen(filename, "r");
	if (f == NULL) {
		printf("Eroare la deschiderea fisierului %s\n", filename);
		exit(-1);
	}
	args *thread_args = malloc(sizeof(args));
	thread_args->rank = rank;
	thread_args->num_peers = numtasks - 1;

	fscanf(f, "%d", &thread_args->num_files);
	file_info *files = malloc(thread_args->num_files * sizeof(file_info));
	for (int i = 0; i < thread_args->num_files; i++) {
		fscanf(f, "%s", files[i].filename);
		fscanf(f, "%d", &files[i].chunks_count);
		for (int j = 0; j < files[i].chunks_count; j++) {
			fscanf(f, "%s", files[i].hashes[j]);
		}
	}
	thread_args->files = files;

	fscanf(f, "%d", &thread_args->num_files_to_download);
	file_info *files_to_download = malloc(thread_args->num_files_to_download * sizeof(file_info));
	for (int i = 0; i < thread_args->num_files_to_download; i++) {
		fscanf(f, "%s", files_to_download[i].filename);
		files_to_download[i].chunks_count = 0;
	}
	thread_args->files_to_download = files_to_download;

	fclose(f);
	return thread_args;
}

void *download_thread_func(void *arg) {
	int rank = *(int *)arg;

	return NULL;
}

void *upload_thread_func(void *arg) {
	int rank = *(int *)arg;

	return NULL;
}

void peer(int numtasks, int rank) {
	pthread_t download_thread;
	pthread_t upload_thread;
	void *status;
	int r;

	args *thread_args = prepare_thread_args(rank, numtasks);

	r = pthread_create((void *)thread_args, NULL, download_thread_func, (void *)&rank);
	if (r) {
		printf("Eroare la crearea thread-ului de download\n");
		exit(-1);
	}

	r = pthread_create((void *)thread_args, NULL, upload_thread_func, (void *)&rank);
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

