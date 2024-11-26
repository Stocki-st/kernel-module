#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include "is18_ioctl.h"

#define READBUF_SIZE 32
#define PROC_FILE "/proc/is18/info"

//colours
#define KNRM "\x1B[0m"   //normal
#define KRED "\x1B[31m"  //red
#define KGRN "\x1B[32m"  //green
#define KYEL "\x1B[33m"  //yellow

void print_help(void);
int testcase_ioctrl(char* device);
int testcase_read_write_nonblocking(char* device);
int testcase_read_write_blocking(char* device);
void* writer_thread(void* args);
void* reader_thread(void* args);
int  print_file(char* filename);

struct thread_args {
    unsigned int delay_sec;
    int file;
};

int main(int argc, char** argv) {
    if (argc <= 2) {
        print_help();
        return 1;
    }

    char* device = argv[1];  // e.g.: "/dev/is18dev3";
    int test_result = 0;

    printf("%s", KYEL);
    printf("### START TESTS ###\n\n");
    printf("%s", KNRM);

    for (int i = 2; i < argc; i++) {
        printf("\narg%d = '%s' \n\n", i, argv[i]);
        if (strcmp(argv[i], "rw_blocking") == 0) {
            test_result = testcase_read_write_blocking(device);
        } else if (strcmp(argv[i], "rw_nonblocking") == 0) {
            test_result = testcase_read_write_nonblocking(device);
        } else if (strcmp(argv[i], "ioctl") == 0) {
            test_result = testcase_ioctrl(device);
        } else if (strcmp(argv[i], "all") == 0) {
            test_result = testcase_read_write_blocking(device);
            test_result += testcase_read_write_nonblocking(device);
            test_result += testcase_ioctrl(device);
        } else {
            printf("%s", KRED);
            printf("mode '%s' is not supported. this is how it works:\n", argv[i]);
            printf("%s", KNRM);
            print_help();
        }

        if (test_result) {
            printf("%s", KRED);
            printf("# Testcase '%s' exited with %d errors. :( \n", argv[i], test_result);
            printf("%s", KNRM);
        } else {
            printf("%s", KGRN);
            printf("\n # Testcase '%s' successfully finished! :)\n\n", argv[i]);
            printf("%s", KNRM);
        }
    }

    printf("%s", KYEL);
    printf("### TESTS FINISHED ###\n\n");
    printf("%s", KNRM);

    return 0;
}

/*
 * TEST Read - Write nonblocking
 */
int testcase_read_write_nonblocking(char* device) {
    int num_of_errors = 0;
    int fd = 0;
    char read_buf[READBUF_SIZE] = {0};
    char* buf = "test";
    int buflen = strlen(buf);
    int rv = 0;
    int len = 0;
    int read_bytes = 0;
    int byte_to_read = 2;

    printf("%s", KYEL);
    printf("# Testcase rw_nonblocking\n\n");
    printf("%s", KNRM);

    printf("open %s\n", device);
    if ((fd = open(device, O_RDWR | O_NONBLOCK)) < 0) {
        perror(device);
        return 1;
    }

    // empty buffer first
    printf("empty buffer for clean test setup\n");
    rv = ioctl(fd, IS18_IOC_EMPTY_BUFFER);
    if (rv) {
        printf("clearing buffer did not work");
        ++num_of_errors;
    }

    read_bytes = read(fd, read_buf, byte_to_read);
    printf("Read %d bytes\n", read_bytes);
    if (read_bytes != 0) {
        printf("ERROR read %d bytes, but expexted 0 (buffer should be empty)\n", read_bytes);
        ++num_of_errors;
    }

    len = write(fd, buf, buflen);
    if (len == buflen) {
        printf("successfully wrote %d bytes\n", len);
    } else {
        printf("ERROR wrote %d bytes, but expected %d\n", len, buflen);
        ++num_of_errors;
    }

    read_bytes = read(fd, read_buf, byte_to_read);
    printf("Read %d bytes\n", read_bytes);
    if (read_bytes != byte_to_read) {
        printf("ERROR read %d bytes, but expexted %d)\n", read_bytes, byte_to_read);
        ++num_of_errors;
    }

    read_bytes = read(fd, read_buf, byte_to_read);
    printf("Read %d bytes\n", read_bytes);
    if (read_bytes != (len - byte_to_read)) {
        printf("ERROR read %d bytes, but expexted %d)\n", read_bytes,
               (len - byte_to_read));  // only len - byte_to_read should be left in buffer
        ++num_of_errors;
    }

    read_bytes = read(fd, read_buf, byte_to_read);
    printf("Read %d bytes\n", read_bytes);
    if (read_bytes != 0) {
        printf("ERROR read %d bytes, but expexted 0 (buffer should be empty)\n", read_bytes);
        ++num_of_errors;
    }

    // write to buffer 3 times
    len = write(fd, buf, buflen);
    if (len == buflen) {
        printf("successfully wrote %d bytes\n", len);
    } else {
        printf("ERROR wrote %d bytes, but expected %d\n", len, buflen);
        ++num_of_errors;
    }
    len = write(fd, buf, buflen);
    if (len == buflen) {
        printf("successfully wrote %d bytes\n", len);
    } else {
        printf("ERROR wrote %d bytes, but expected %d\n", len, buflen);
        ++num_of_errors;
    }
    len = write(fd, buf, buflen);
    if (len == buflen) {
        printf("successfully wrote %d bytes\n", len);
    } else {
        printf("ERROR wrote %d bytes, but expected %d\n", len, buflen);
        ++num_of_errors;
    }

    byte_to_read = READBUF_SIZE;
    read_bytes = read(fd, read_buf, byte_to_read);
    printf("Read %d bytes\n", read_bytes);
    if (read_bytes != (3 * len)) {
        printf("ERROR read %d bytes, but expexted %d\n", read_bytes, (3 * len));
        ++num_of_errors;
    }

    read_bytes = read(fd, read_buf, byte_to_read);
    printf("Read %d bytes\n", read_bytes);
    if (read_bytes != 0) {
        printf("ERROR read %d bytes, but expexted 0 (buffer should be empty)\n", read_bytes);
        ++num_of_errors;
    }

    if (close(fd)) {
        perror(device);
    }
    return num_of_errors;
}

void* writer_thread(void* args) {
    struct thread_args* arguments = (struct thread_args*)args;

    unsigned int delay = arguments->delay_sec;
    int fd = arguments->file;
    char* buf = "my test string";
    int buflen = strlen(buf);

    printf("sleep %d seconds before writing\n", delay);
    sleep(delay);

    int len = write(fd, buf, buflen);
    if (len == buflen) {
        printf("successfully wrote %d bytes\n", len);
    } else {
        printf("ERROR wrote %d bytes, but expected %d\n", len, buflen);
    }
    return NULL;
}


void* reader_thread(void* args) {
    struct thread_args* arguments = (struct thread_args*)args;

    unsigned int delay = arguments->delay_sec;
    int fd = arguments->file;
    int read_bytes = 0;
    int byte_to_read = 10;
    char read_buf[READBUF_SIZE] = {0};

    printf("sleep %d seconds before reading\n", delay);
    sleep(delay);

    read_bytes = read(fd, read_buf, byte_to_read);
    printf("Read %d bytes\n", read_bytes);
    if (read_bytes != byte_to_read) {
        printf("ERROR read %d bytes\n", read_bytes);
    }
    return NULL;
}


int testcase_read_write_blocking(char* device) {
    // start writer thread with delay
    pthread_t id_writer;
    pthread_t id_reader;
    int fd = 0;
    int fd_ro = 0;
    int rv = 0;
    int num_of_errors = 0;
    int read_bytes = 0;
    int byte_to_read = 5;
    char read_buf[READBUF_SIZE] = {0};
    char* buf = "my test string";
    int buflen = strlen(buf);

    printf("%s", KYEL);
    printf("# Testcase rw_blocking\n\n");
    printf("%s", KNRM);

    printf("open %s\n", device);
    if ((fd = open(device, O_WRONLY)) < 0) {
        perror(device);
        return 1;
    }

    if ((fd_ro = open(device, O_RDONLY)) < 0) {
        perror(device);
        return 1;
    }

    // set buffer to 0
    printf("clear the buffer\n");
    rv = ioctl(fd, IS18_IOC_EMPTY_BUFFER);
    if (rv) {
        printf("clearing buffer did not work");
        ++num_of_errors;
    }

    struct thread_args arguments;
    arguments.delay_sec = 5;
    arguments.file = fd;

    pthread_create(&id_writer, NULL, writer_thread, &arguments);
    time_t start, end;

    printf("call reading and expect ~%d seconds to wait until writing\n", arguments.delay_sec);
    time(&start);
    read_bytes = read(fd_ro, read_buf, byte_to_read);
    printf("Read %d bytes\n", read_bytes);
    if (!read_bytes) {
        printf("ERROR read %d bytes\n", read_bytes);
        ++num_of_errors;
    }
    time(&end);

    double duration = difftime(end, start);

    if (duration < arguments.delay_sec) {
        printf("blocking not working - read duration: %f, but delay before writing: %d\n", duration, arguments.delay_sec);
        ++num_of_errors;
    } else {
        printf("read delay took %f seconds, expected at least %d\n", duration, arguments.delay_sec);
    }

    printf("call reading and expect nearly no delay\n");
    time(&start);
    read_bytes = read(fd_ro, read_buf, byte_to_read);
    printf("Read %d bytes\n", read_bytes);
    if (!read_bytes) {
        printf("ERROR read %d bytes\n", read_bytes);
        ++num_of_errors;
    }
    time(&end);

    duration = difftime(end, start);
    if (duration > 0.1) {
        printf("reading blocked - read duration: %f, but expected no delay\n", duration);
        ++num_of_errors;
    } else {
        printf("reading took %f seconds\n", duration);
    }

    arguments.delay_sec = 5;
    arguments.file = fd_ro;
    pthread_create(&id_reader, NULL, reader_thread, &arguments);
    time(&start);
    int len = write(fd, buf, buflen);
    if (len == buflen) {
        printf("successfully wrote %d bytes\n", len);
    } else {
        printf("ERROR wrote %d bytes, but expected %d\n", len, buflen);
        ++num_of_errors;
    }
    time(&end);

    duration = difftime(end, start);

    if (duration < arguments.delay_sec) {
        printf("blocking not working - write duration: %f, but delay before reading: %d\n", duration, arguments.delay_sec);
        ++num_of_errors;
    } else {
        printf("write delay took %f seconds, expected at least %d\n", duration, arguments.delay_sec);
    }

    pthread_join(id_writer, NULL);
    pthread_join(id_reader, NULL);

    printf("print proc:\n");
    print_file(PROC_FILE);


    if (close(fd_ro)) {
        perror(device);
        ++num_of_errors;
    }

    if (close(fd)) {
        perror(device);
        ++num_of_errors;
    }

    return num_of_errors;
}

/*
 *  TEST IOCTL
 */
int testcase_ioctrl(char* device) {
    int num_of_errors = 0;
    int fd = 0;
    char read_buf[READBUF_SIZE] = {0};
    char* buf = "test";
    int buflen = strlen(buf);
    int rv = 0;
    int len = 0;
    int read_bytes = 0;
    int byte_to_read = 2;

    int read_cnt = 0;
    int write_cnt = 0;
    int fd_ro;
    int fd_wo;

    printf("%s", KYEL);
    printf("# Testcase ioctl\n\n");
    printf("%s", KNRM);

    printf("open %s\n", device);
    if ((fd = open(device, O_RDWR)) < 0) {
        perror(device);
        free(buf);
        return 1;
    }

    // set buffer to 0
    printf("clear the buffer\n");
    rv = ioctl(fd, IS18_IOC_EMPTY_BUFFER);
    if (rv) {
        printf("clearing buffer did not work");
        ++num_of_errors;
    }

    len = write(fd, buf, buflen);
    if (len == buflen) {
        printf("successfully wrote %d bytes\n", len);
    } else {
        printf("ERROR wrote %d bytes, but expected %d\n", len, buflen);
        ++num_of_errors;
    }

    rv = ioctl(fd, IS18_IOC_NUM_BUFFERED_BYTES);
    printf("ioctl IS18_IOC_NUM_BUFFERED_BYTES of %s: %d\n", device, rv);
    if (rv != len) {
        printf("ERROR number of bytes is %d bytes, but expected %d\n", rv, len);
        ++num_of_errors;
    }

    rv = ioctl(fd, IS18_IOC_READ_INDEX);
    printf("ioctl IS18_IOC_READ_INDEX of %s: %d\n", device, rv);
    if (rv) {
        printf("ERROR read index is %d, but expected 0\n", rv);
        ++num_of_errors;
    }

    rv = ioctl(fd, IS18_IOC_WRITE_INDEX);
    printf("ioctl IS18_IOC_WRITE_INDEX of %s: %d\n", device, rv);
    if (rv != len) {
        printf("ERROR write index is %d, but expected %d\n", rv, len);
        ++num_of_errors;
    }

    read_bytes = read(fd, read_buf, byte_to_read);
    printf("Read %d bytes\n", read_bytes);
    if (read_bytes != byte_to_read) {
        printf("ERROR read %d bytes, but expexted %d\n", read_bytes, byte_to_read);
        ++num_of_errors;
    }

    rv = ioctl(fd, IS18_IOC_READ_INDEX);
    printf("ioctl IS18_IOC_READ_INDEX of %s: %d\n", device, rv);
    if (rv != byte_to_read) {
        printf("ERROR read index is %d, but expected %d\n", rv, read_bytes);
        ++num_of_errors;
    }

    rv = ioctl(fd, IS18_IOC_WRITE_INDEX);
    printf("ioctl IS18_IOC_WRITE_INDEX of %s: %d\n", device, rv);
    if (rv != len) {
        printf("ERROR write index is %d, but expected %d\n", rv, len);
        ++num_of_errors;
    }

    rv = ioctl(fd, IS18_IOC_NUM_BUFFERED_BYTES);
    printf("ioctl IS18_IOC_NUM_BUFFERED_BYTES of %s: %d\n", device, rv);
    if (rv != (len - byte_to_read)) {
        printf("ERROR number of bytes is %d bytes, but expected 0\n", rv);
        ++num_of_errors;
    }

    rv = ioctl(fd, IS18_IOC_EMPTY_BUFFER);
    printf("ioctl IS18_IOC_EMPTY_BUFFER of %s: %d\n", device, rv);
    if (rv) {
        printf("clearing the buffer did not work");
        ++num_of_errors;
    }

    rv = ioctl(fd, IS18_IOC_READ_INDEX);
    printf("ioctl IS18_IOC_READ_INDEX of %s: %d\n", device, rv);
    if (rv != 0) {
        printf("ERROR write read is %d, but expected 0\n", rv);
        ++num_of_errors;
    }

    rv = ioctl(fd, IS18_IOC_WRITE_INDEX);
    printf("ioctl IS18_IOC_WRITE_INDEX of %s: %d\n", device, rv);
    if (rv) {
        printf("ERROR write write is %d, but expected 0\n", rv);
        ++num_of_errors;
    }
    rv = ioctl(fd, IS18_IOC_NUM_BUFFERED_BYTES);
    printf("ioctl IS18_IOC_NUM_BUFFERED_BYTES of %s: %d\n", device, rv);
    if (rv) {
        printf("ERROR number of bytes is %d bytes, but expected 0\n", rv);
        ++num_of_errors;
    }

    read_cnt = ioctl(fd, IS18_IOC_OPENREADCNT);
    printf("ioctl IS18_IOC_OPENREADCNT of %s: %d\n", device, rv);
    if (read_cnt < 1) {
        printf("ERROR file is open in read mode at least 1 time\n");
        ++num_of_errors;
    }

    write_cnt = ioctl(fd, IS18_IOC_OPENWRITECNT);
    printf("ioctl IS18_IOC_OPENWRITECNT of %s: %d\n", device, rv);
    if (write_cnt < 1) {
        printf("ERROR file is open in write mode at least 1 time\n");
        ++num_of_errors;
    }

    printf("open read-only %s\n", device);
    if ((fd_ro = open(device, O_RDONLY)) < 0) {
        perror(device);
        ++num_of_errors;
    }

    rv = ioctl(fd, IS18_IOC_OPENREADCNT);
    printf("ioctl IS18_IOC_OPENREADCNT of %s: %d\n", device, rv);
    if (rv != (++read_cnt)) {
        printf("ERROR file is open in read %d times \n", read_cnt);
        ++num_of_errors;
    }

    rv = ioctl(fd, IS18_IOC_OPENWRITECNT);
    printf("ioctl IS18_IOC_OPENWRITECNT of %s: %d\n", device, rv);
    if (rv != write_cnt) {
        printf("ERROR file is open in write mode %d times\n", write_cnt);
        ++num_of_errors;
    }

    printf("open write-only %s\n", device);
    if ((fd_wo = open(device, O_WRONLY)) < 0) {
        perror(device);
        ++num_of_errors;
    }

    rv = ioctl(fd, IS18_IOC_OPENREADCNT);
    printf("ioctl IS18_IOC_OPENREADCNT of %s: %d\n", device, rv);
    if (rv != read_cnt) {
        printf("ERROR file is open in read %d times \n", read_cnt);
        ++num_of_errors;
    }

    rv = ioctl(fd, IS18_IOC_OPENWRITECNT);
    printf("ioctl IS18_IOC_OPENWRITECNT of %s: %d\n", device, rv);
    if (rv != (++write_cnt)) {
        printf("ERROR file is open in write mode %d times\n", write_cnt);
        ++num_of_errors;
    }

    if (close(fd_wo)) {
        perror(device);
    }

    rv = ioctl(fd, IS18_IOC_OPENREADCNT);
    printf("ioctl IS18_IOC_OPENREADCNT of %s: %d\n", device, rv);
    if (rv != read_cnt) {
        printf("ERROR file is open in read %d times \n", read_cnt);
        ++num_of_errors;
    }

    rv = ioctl(fd, IS18_IOC_OPENWRITECNT);
    printf("ioctl IS18_IOC_OPENWRITECNT of %s: %d\n", device, rv);
    if (rv != (--write_cnt)) {
        printf("ERROR file is open in write mode %d times\n", write_cnt);
        ++num_of_errors;
    }

    if (close(fd_ro)) {
        perror(device);
    }

    rv = ioctl(fd, IS18_IOC_OPENREADCNT);
    printf("ioctl IS18_IOC_OPENREADCNT of %s: %d\n", device, rv);
    if (rv != (--read_cnt)) {
        printf("ERROR file is open in read %d times \n", read_cnt);
        ++num_of_errors;
    }

    rv = ioctl(fd, IS18_IOC_OPENWRITECNT);
    printf("ioctl IS18_IOC_OPENWRITECNT of %s: %d\n", device, rv);
    if (rv != write_cnt) {
        printf("ERROR file is open in write mode %d times\n", write_cnt);
        ++num_of_errors;
    }

    printf("print proc:\n");
    print_file(PROC_FILE);

    if (close(fd)) {
        perror(device);
    }
    return num_of_errors;
}

int  print_file(char* filename)
{
    FILE *fp;

    char c;

    // Open file
    fp = fopen(filename, "r");
    if (fp == NULL)
    {
        printf("Cannot open file \n");
        return 1;

    }

    c = fgetc(fp);
    while (c != EOF)
    {
        printf ("%c", c);
        c = fgetc(fp);
    }

    fclose(fp);
    return 0;
}


void print_help() {
    printf("## is18dev_ kernel driver test ##\n\n");
    printf("this test has to be called like:\n\n");
    printf("./testapp <device-file> <mode>\n");
    printf("e.g. >\n\n");
    printf("       ./testapp /dev/is18dev1 ioctl\n\n");
    printf("the following modes/testcases are supported:\n");
    printf(" - 'ioctl': - is testing all the ioctl functionality\n");
    printf(" - 'rw_blocking': - tests reading and writing in blocking mode (multi threaded)\n");
    printf(" - 'rw_nonblocking': - tests reading and writing in non-blocking mode\n");
    printf(" - 'all': - executes all the above mentioned tests\n\n");
    printf("It's also supported to start the test with multiple testmodes, e.g. >\n\n");
    printf("       ./testapp /dev/is18dev1 ioctl rw_blocking\n\n");
    printf("\ngood luck, have fun!\n");
}
