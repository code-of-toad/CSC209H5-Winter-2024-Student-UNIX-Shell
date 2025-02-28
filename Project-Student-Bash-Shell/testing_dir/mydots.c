#include <stdio.h>
#include <unistd.h>


int main() {
    while (1) {
        printf(".");
        fflush(stdout);
        usleep(500000);
    }
}
