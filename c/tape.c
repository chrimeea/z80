// gcc main.c -Ofast -Wall

#include <stdio.h>
#include <string.h>
#include <math.h>

void read_block_10(FILE *f)
{
    int pause, block_size;
    char block[65536];
    fread(&pause, 1, 2, f);
    fread(&block_size, 1, 2, f);
    fread(block, 1, block_size, f);
}

void read_block_15(FILE *f)
{
    int states_per_sample, pause, used, block_size;
    char block[2097152];
    fread(&states_per_sample, 1, 2, f);
    fread(&pause, 1, 2, f);
    fread(&used, 1, 1, f);
    fread(&block_size, 1, 3, f);
    fread(block, 1, ceil(block_size / 8.0), f);
}

void load_tzx(const char *filename)
{
    char id;
    char block[65536];
    FILE *f = fopen(filename, "rb");
    fread(block, 1, 10, f);
    if (strncmp("ZXTape!\x1A", block, 8) == 0 && block[9] <= 20)
    {
        fread(&id, 1, 1, f);
        while (id != 0)
        {
            // printf("%x\n", id);
            switch(id)
            {
                case 0x10:
                    read_block_10(f);
                    break;
                case 0x15:
                    read_block_15(f);
                    break;
            }
            fread(&id, 1, 1, f);
        }
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    if (argc == 2)
    {
        load_tzx(argv[1]);
    }
}
