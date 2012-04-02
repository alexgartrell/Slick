#include <stdio.h>
#include <string.h>

#include "../klibevent/circbuffer.h"

#define assert(A) {                             \
    if(!(A)) {                                  \
        printf("Failure on line %d\n", __LINE__); \
        printf("\tassertion: %s\n", #A);        \
        failures++;                             \
    }                                           \
    }

int sanity_test() {
    int failures = 0;
    CircBuffer cb;

    CircBuffer_init(&cb);

    char *p;
    assert(CircBuffer_peek(&cb, &p) == 0);

    char buffer[10];
    assert(CircBuffer_read(&cb, buffer, 10) == 0);

    char b2[] = "Hello";

    CircBuffer_write(&cb, b2, sizeof(b2));
    CircBuffer_peek(&cb, &p);

    assert(CircBuffer_peek(&cb, &p) == 6);
    assert(strcmp(b2, p) == 0);

    assert(CircBuffer_peek(&cb, &p) == 6);
    assert(strcmp(b2, p) == 0);

    CircBuffer_discard(&cb, 6);
    assert(CircBuffer_peek(&cb, &p) == 0);   

    assert(CircBuffer_read(&cb, buffer, 10) == 0);

    char c = 'a';
    while(CircBuffer_write(&cb, &c, 1) == 1)
        continue;

    assert(CircBuffer_read(&cb, &c, 1) == 1);
    assert(c == 'a');

    char b3[] = "123456789012";
    CircBuffer_init(&cb);

    int count = 0;
    while(CircBuffer_write(&cb, b3, sizeof(b3)) == sizeof(b3))
        count++;

    assert(count > 0);

    while(CircBuffer_read(&cb, buffer, sizeof(b3)) == sizeof(b3)) {
        assert(strcmp(buffer, b3) == 0);
        count--;
    }

    assert(count == 0);

    while(CircBuffer_write(&cb, b3, sizeof(b3)) == sizeof(b3))
        continue;

    assert(CircBuffer_write_peek(&cb, &p) == 0);

    assert(CircBuffer_read(&cb, buffer, 4) == 4);

    printf("%d\n", CircBuffer_write_peek(&cb, &p));
    assert(CircBuffer_write_peek(&cb, &p) == 4);

    assert(CircBuffer_write_commit(&cb, 8) == -1);
    
    assert(CircBuffer_write_commit(&cb, 2) == 0);
    
    assert(CircBuffer_write_peek(&cb, &p) == 4);    

    return failures;
}

int main(int argc, char *argv[]) {
    int rc = 0;
    if(sanity_test()) {
        printf("Sanity tests failed\n");
        rc = 1;
    }
    
	return 0;
}
