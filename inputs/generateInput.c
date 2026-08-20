#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    if(argc != 4) {
        printf("Usage: %s <rows> <cols> <input_file>\n", argv[0]);
        return 1;
    }

    FILE *input_file;
    int n,m;

    // Reading the number of rows and columns
    n = atoi(argv[1]);
    m = atoi(argv[2]);

    // Opening the input file
    if((input_file = fopen(argv[3], "w")) == NULL) {
        printf("ERROR: Could not open the input file\n");
        return 1;
    }

    // Setting the seed for the random number generator
    srand(time(NULL) ^ getpid());
    
    // Generating the input file
    fprintf(input_file, "%d %d\n", n, m);
    for(int i = 0; i < n; i++) {
        for(int j = 0; j < m; j++)
        {
            if(i == 0 || i == n-1 || j == 0 || j == m-1)
                fprintf(input_file, "%d ", 0);
            else
                fprintf(input_file, "%d ", rand() % 2);
        }

        fprintf(input_file, "\n");
    }

    // Closing the input file
    fclose(input_file);
}