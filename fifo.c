#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>

#define PER_READ 16384

struct Callback_Data {
	ssize_t written;
	ssize_t total;
};

ssize_t
get_file_size(int fd)
{
	ssize_t ret = 0;
	ssize_t cur = lseek(fd, 0, SEEK_CUR);
	ret = lseek(fd, 0, SEEK_END);
	lseek(fd, cur, SEEK_SET);

	return ret;
}

void
written(struct Callback_Data data)
{
	printf("W %ld bytes of %ld data\n", data.written, data.total);
	fflush(stdout);
}

void
write_to_dest(int pfd, int dfd, ssize_t sz)
{
	char buf[PER_READ] = {0,};
	u_int16_t ret;
	ssize_t total = 0;

	// Pipes are blocking IO. Read operation will be blocked till any data is available
	while ((ret = read(pfd, buf, PER_READ)) != -1) {
		write(dfd, buf, PER_READ);
		memset(buf, 0, PER_READ);
		total += ret;
		printf("R %ld bytes of %ld data\n", total, sz);
		fflush(stdout);

		// A callback can be added here too

		if (total == sz) {
			// End this if all data has been written
			break;
		}
	}

	close(pfd);
	close(dfd);
}

int
main(int argc, char *argv[argc])
{
	if (argc < 3) {
		printf("SRC and DEST parameters are required\n");
		return 1;
	}

	int fd;
	int dfd;
	__uint16_t ret;
	char buf[PER_READ] = {0,};
	char *src = argv[1];
	char *dest = argv[2];
	int pipes[2];
	struct Callback_Data data = {0,0};

	if ((fd = open(src, O_RDONLY)) == -1) {
		perror("Unable to open source file: ");
		return 1;
	}

	if (access( dest, F_OK ) == 0) {
		printf("File exists. Cannot overwrite files at the moment\n");
		return -1;
	}

	if ((dfd = open(dest, O_WRONLY|O_CREAT)) == -1) {
		perror("Unable to create destination: ");
		return 1;
	}

	if (pipe(pipes) == -1) {
		perror("Pipe error:");
		return 1;
	}

	data.total = get_file_size(fd);
	data.written = 0;

	if (fork() == 0) {
		// Create a child process to watch for new writes in pipe
		write_to_dest(pipes[0], dfd, data.total);
	} else {
		while ((ret = read(fd, buf, PER_READ)) != -1) {
			if (ret == 0) {
				break;
			}
			write(pipes[1], buf, ret);

			data.written += ret;
			memset(buf, 0, PER_READ);

			// A callback
			written(data);
		}

		close(fd);
		close(pipes[1]);
	}

	return 0;
}