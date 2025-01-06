#include <stddef.h>

#include "common/io.h"
#include "common/sumset.h"

int main()
{
    InputData input_data;
    input_data_read(&input_data);
    // input_data_init(&input_data, 8, 10, (int[]){0}, (int[]){1, 0});

    Solution best_solution;
    solution_init(&best_solution);

    // ...

    solution_print(&best_solution);
    return 0;
}
