#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
int main()
{
    // char* p = "0xffff";
    //  uint64_t nValude = 0;
    //  sscanf(p, "%llx", &nValude);
    //  printf("%lld\r\n", nValude);
    const char *arr[] = {"1", "aaaa", "bbbb", "cccc"};
    printf("%ld\r\n", sizeof(arr)/sizeof(*arr));

    uint64_t num = 0;
    sscanf("FFFF", "%llx", &num);
    printf("%lld\r\n", num);

    return 0;
}