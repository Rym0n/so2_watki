#include <stdio.h>

#include "functions.h"

#define MAIN_SEM_NAME "SO2_GAME_SERVER_STATUS"

int main()
{

    sem_t *main_sem = sem_open(MAIN_SEM_NAME, O_CREAT, 0777, 0);

    int sem_value;
    sem_getvalue(main_sem, &sem_value);

    if (sem_value == 0)
    {
        game_create();
        sem_post(main_sem);

        run_game();

        sem_unlink(MAIN_SEM_NAME);
    }
    else
    {
        start_data_transfer(1);

        run_player();
    }

    printf("You have disconnected\n");

    return 0;
}