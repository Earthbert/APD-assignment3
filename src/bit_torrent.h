#ifndef BIT_TORRENT_H
#define BIT_TORRENT_H

#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_CHUNKS 100

#define REQUEST_CHUNK 1
#define SEND_CHUNK 2
#define REQUEST_FILE_INFO 3
#define SEND_FILE_INFO 4
#define SEND_NUM_FILES 5

typedef struct {
	char filename[MAX_FILENAME]; // file name
	char hashes[MAX_CHUNKS][HASH_SIZE]; // file hashes
	int chunks_count; // number of chunks
} file_info; // file info

typedef struct {
	file_info file; // file info
	int num_peers; // number of peers that have the file
	int *peers; // peers that have the file
	int **chunks; // chunks that each peer has
} swarm; // swarm info

typedef struct {
	int num_files;
	file_info files[MAX_FILES];
} mpi_struct;

typedef struct {
	int rank; // peer rank
	int num_peers; // number of peers
	int num_files; // number of files
	int num_files_to_download; // number of files to download
	file_info *files; // files info
	file_info *files_to_download; // files to download
} args; // arguments for peer threads

void peer(int numtasks, int rank);

void tracker(int numtasks, int rank);

inline MPI_Datatype create_mpi_struct() {
	int blocklengths[2] = { 1, MAX_FILES };
	MPI_Datatype types[2] = { MPI_INT, MPI_BYTE };
	MPI_Aint offsets[2];

	offsets[0] = offsetof(mpi_struct, num_files);
	offsets[1] = offsetof(mpi_struct, files);

	MPI_Datatype mpi_struct_type;
	MPI_Type_create_struct(2, blocklengths, offsets, types, &mpi_struct_type);
	MPI_Type_commit(&mpi_struct_type);

	return mpi_struct_type;
}

#endif // BIT_TORRENT_H