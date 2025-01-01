int            printf(char*, ...) __attribute__ ((format (printf, 1, 2)));
void           panic(char*) __attribute__((noreturn));
void           assert(int);