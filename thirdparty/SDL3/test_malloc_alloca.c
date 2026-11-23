#include <malloc.h>

int main(int argc, char *argv[]) { void *ptr = _alloca(argc * (int)argv[0][0]); return ptr != (void *)0; }