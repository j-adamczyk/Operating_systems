#ifndef main
#define main

void write_times (char*, struct timespec*, struct timespec*, struct tms*, struct tms*, FILE*)

void copy (const char*, const char*, size_t, size_t, char*);

void generate (const char*, size_t, size_t);

void sort (const char*, size_t, size_t, char*);

#endif
