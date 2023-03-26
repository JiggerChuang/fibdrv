#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

/* time */
#include <time.h>

#define FIB_DEV "/dev/fibonacci"

long long get_time_ns()
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return ts.tv_nsec;
}

int main()
{
    long long sz;

    char buf[256];
    size_t buf_size = sizeof(buf) / sizeof(char);
    char write_buf[] = "testing writing";
    int offset = 100; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    FILE *data = fopen("data.txt", "w");
    if (!data) {
        perror("Failed to open data text");
        exit(2);
    }

    // for (int i = 0; i <= offset; i++) {
    //     sz = write(fd, write_buf, strlen(write_buf));
    //     printf("Writing to " FIB_DEV ", returned the sequence %lld\n", sz);
    // }

    for (int i = 0; i <= offset; i++) {
        long long ut = get_time_ns(); /* get start time */

        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, buf_size);

        ut = get_time_ns() - ut; /* get user execution time */
        long long kt =
            write(fd, write_buf, strlen(write_buf)); /* get kernel time */
        fprintf(data, "%d %lld %lld %lld\n", i, ut, kt,
                (ut - kt)); /* store profiling to data.txt */

        char *output = (char *) malloc(sz + 1);
        strncpy(output, buf, sz);
        output[sz] = '\0';

        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s.\n",
               i, output);
    }

    for (int i = offset; i >= 0; i--) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, buf_size);

        char *output = (char *) malloc(sz + 1);
        strncpy(output, buf, sz);
        output[sz] = '\0';

        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s.\n",
               i, output);
    }

    fclose(data);
    close(fd);
    return 0;
}
