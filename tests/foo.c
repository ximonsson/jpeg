#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SIZE sizeof (int)

int main ()
{
    uint8_t foo[12];
    uint8_t* ptr = foo;
    int p = 1;

    for (int i = 0; i < 3; i ++)
    {
        memcpy (ptr, &p, SIZE);
        p ++;
        ptr += SIZE;
    }

    printf ("foo:\n");
    for (int i = 0; i < 12; i ++)
        printf ("%2d ", foo[i]);
    printf ("\n");

    ptr = foo;
    for (int i = 0; i < 3; i ++)
    {
        memcpy (&p, ptr, SIZE);
        ptr += SIZE;
        printf ("%2d ", p);
    }
    printf ("\n");

    return 0;
}
