#include <cstdio>
#include <cstdlib>

int random_letter()
{
    int random_letter = rand();
    return 'a' + random_letter % 26;
}

int main(int argc, const char* argv[])
{
    int seed;
    int size;
    sscanf(argv[1], "%d", &seed);
    sscanf(argv[2], "%d", &size);

    srand(seed);
    for (int x = 0; x < size; x++)
        printf("%c", random_letter());

    return 0;
}
