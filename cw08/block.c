#include <stdio.h>
#include <stdbool.h>

#define CEIL_DIV(a, b)     \
({                         \
    typeof(a) _a = (a);    \
    typeof(b) _b = (b);    \
    _a / _b + !!(_a % _b); \
})

int main(int argc, char **argv) {
    size_t n, m;
    sscanf(argv[1], "%zu", &n);
    sscanf(argv[2], "%zu", &m);

    for (size_t k = 0; k < m; k++) {
        printf(
            "%zu - %zu\n", 
            CEIL_DIV(k * n, m), 
            CEIL_DIV((k + 1) * n, m) - 1
        );
    }
}
