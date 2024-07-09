#include <stdio.h>
#include <stdint.h>

#include "types.h"

// pulled from mkfs.c
// convert to intel byte order
ushort
xshort(ushort x)
{
  ushort y;
  uchar *a = (uchar*)&y;
  a[0] = x;
  a[1] = x >> 8;
  return y;
}

uint
xint(uint x)
{
  uint y;
  uchar *a = (uchar*)&y;
  a[0] = x;
  a[1] = x >> 8;
  a[2] = x >> 16;
  a[3] = x >> 24;
  return y;
}

/**
 * Iterate through bytes, lowest address to highest
 */
void print_bytes(const void *ptr, size_t size) {
    const uchar *bytes = (const uchar *)ptr;
    for (size_t i = 0; i < size; ++i) {
        printf("%02x ", bytes[i]);
    }
    printf("\n");
}

int main() {
    ushort test_short = 0x1234;
    uint test_int = 0x12345678;

    ushort result_short = xshort(test_short);
    uint result_int = xint(test_int);

    printf("Original ushort: 0x%04x\n", test_short);
    printf("Converted ushort: 0x%04x\n", result_short);
    printf("Bytes of original ushort: ");
    print_bytes(&test_short, sizeof(test_short));
    printf("Bytes of converted ushort: ");
    print_bytes(&result_short, sizeof(result_short));

    printf("\n");

    printf("Original uint: 0x%08x\n", test_int);
    printf("Converted uint: 0x%08x\n", result_int);
    printf("Bytes of original uint: ");
    print_bytes(&test_int, sizeof(test_int));
    printf("Bytes of converted uint: ");
    print_bytes(&result_int, sizeof(result_int));

    return 0;
}