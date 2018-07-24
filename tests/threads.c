#include <stdio.h>
#include <pthread.h>


#define ARRAY_SIZE  25


extern void fillarray (void*);
extern void setx (int);


typedef struct args {
    void* array;
    int x;
} args;


static void* fill (void* a)
{
    args* in = (args*) a;
    setx (in->x);
    fillarray (in->array);
    return NULL;
}


int main (int argc, char** argv)
{
    int* a[ARRAY_SIZE] = { 0 };
    int* b[ARRAY_SIZE] = { 0 };

    pthread_t ta;
    pthread_t tb;

    args ta_args = {a, 10};
    args tb_args = {b, 20};

    pthread_create (&ta, NULL, fill, &ta_args);
    pthread_create (&tb, NULL, fill, &tb_args);

    pthread_join (ta, NULL);
    pthread_join (tb, NULL);

    printf ("A:\n");
    for (int i = 0; i < ARRAY_SIZE; i ++)
        printf ("%2d ", a[i]);
    printf ("\n");

    printf ("B:\n");
    for (int i = 0; i < ARRAY_SIZE; i ++)
        printf ("%2d ", b[i]);
    printf ("\n");

    return 0;
}
