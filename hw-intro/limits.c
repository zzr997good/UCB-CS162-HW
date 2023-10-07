#include <stdio.h>
#include <sys/resource.h>

int main() {
    struct rlimit lim;
    if(getrlimit(RLIMIT_STACK,&lim)!=0) printf("ERROR: system call:getrlimit(RLIMIT_STACK,&lim) fails\n");
    printf("stack size: %ld\n", lim.rlim_cur);
    if(getrlimit(RLIMIT_NPROC,&lim)!=0) printf("ERROR: system call:getrlimit(RLIMIT_STACK,&lim) fails\n");
    printf("process limit: %ld\n", lim.rlim_cur);
    if(getrlimit(RLIMIT_NOFILE,&lim)!=0) printf("ERROR: system call:getrlimit(RLIMIT_STACK,&lim) fails\n");
    printf("max file descriptors: %ld\n", lim.rlim_cur);
    return 0;
}
