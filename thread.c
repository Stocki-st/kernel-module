#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

int testcase_read_write_blocking(char* device);


// Global variable:
int i = 2;

void* writer( void* delay_sec) {
    // Print value received as argument:
    printf("sleep 5s\n");
    sleep(* (int*)delay_sec);
    printf("now wake up and write\n");

    // Return reference to global variable:
    pthread_exit(&i);
}

int main(void) {
    printf("testcase_read_write_blocking\n");

    char* device = "device";
    testcase_read_write_blocking(device);

    printf("Value recevied by parent from child: ");
    return 0;
}



int testcase_read_write_blocking(char* device) {


    int writer_delay_seconds = 5;

//empty buffer

//start writer thread with delay
    pthread_t id;
    pthread_create(&id, NULL, writer, &writer_delay_seconds);
    int* ptr;
    time_t start,end;


    // Wait for foo() and retrieve value in ptr (num_of_errors);
    pthread_join(id, (void**)&ptr);

    printf("writer thread finished - reading should now work again\n");

    time(&start);
    sleep(4);
    time(&end);
//read blocking (should block now)
    double duration = difftime(end,start);

    if(duration <= writer_delay_seconds) {
        printf("blocking not working - read duration: %f, but delay before writing: %d\n", duration, writer_delay_seconds);
    } else {
        printf("took %f seconds\n", duration);
    }


    return  0;
}

