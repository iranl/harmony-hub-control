#include <stdio.h>
#include <sys/statvfs.h>

int main() {
    const char *paths[] = {"/mnt/data", "/cache", "/", NULL};
    for (int i = 0; paths[i]; i++) {
        struct statvfs s;
        if (statvfs(paths[i], &s) == 0) {
            printf("%s: total %llu KB, free %llu KB (avail %llu KB)\n",
                paths[i],
                (unsigned long long)(s.f_blocks * s.f_frsize) / 1024,
                (unsigned long long)(s.f_bfree * s.f_frsize) / 1024,
                (unsigned long long)(s.f_bavail * s.f_frsize) / 1024);
        } else {
            perror(paths[i]);
        }
    }
    return 0;
}
