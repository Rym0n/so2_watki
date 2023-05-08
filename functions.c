#define _GNU_SOURCE

#include "functions.h"

char global_server_map[51 *
                       25] = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX   X  T    X    ####   T   X    t    X       X   XX X XXX XcX XXXXXXXXXXX XXX X XXX XXX XXX XXXXX   XX     X XcX          cX X X   X  t  X  T  X   X   XX XXX X XcXXX###XX XXcX X XXXXX XXXXXXXX XXXX XXX XX X X   X    ccc    Xc  X ##  X       X   T   X X XX X XX XX XXX XX XXXX X X X XXX XXX XXX XXX X X X XX t     t   X X       X     t   Xcc X   X X X ccX XX X XX   XX XXX XXXX XX XXXXX XXX XXX XXX X X X   XX X X     X   X   X    t    X   X    t    X X X X XXcXXX XXX XXX XXX XXX XXX X XXX XXXX   XXXX X X X XX       t  T  X XcccX  A    X t X X       X     X XX X XXXXXX#XX X XXX X X XXX XXX X X XXXXX XcX XXX XX X    #X   X X   X   X   X   X   X X  T  XcX X T XX X X  #X XXX XXX XXX XXX XXXtXXX X XXX XXXcX X X XX X X # X    #  X   X X  ###  X   X  cX     X X X XX c X#  X  XXXX X X X X XX#XX X XXXXX XXXXXXX X X XX X X#   t  X   X X XcccX     X   X X       ##X   XX XXXXX XXX X XXX XXXXXXX XXX XXX X XXXXX X ##XXX XX X#      X X   T X     X    cc X   X   X X ##    XX X XXXXX X XX   XX X XXX XXXXX XXX X X XXX#XXXXX XX###X     X     t   X  ###X## X     X X ccX###### XX XXX XXX XXXXXX XXXXXXXXXX#X XXXX XX XXX X#    # XX   X  t   ccccc      #########     T   X    ##   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
char players_map[51 * 25] = {0};

Game *global_server = NULL;
Players_Data *global_player = NULL;

Game *game_create() {

    global_server = (Game *) calloc(1, sizeof(Game));
    if (global_server == NULL)
        return NULL;


    sem_init(&global_server->sem_to_update, 0, 1);


    global_server->join_request.fd = shm_open("LOBBY_SEM", O_CREAT | O_RDWR, 0600);
    ftruncate(global_server->join_request.fd, sizeof(Lobby));
    global_server->join_request.data = (Lobby *) mmap(NULL, sizeof(Lobby), PROT_READ | PROT_WRITE,
                                                      MAP_SHARED,
                                                      global_server->join_request.fd, 0);

    Lobby *request = (Lobby *) global_server->join_request.data;
    sem_init(&request->sem, 1, 0);
    sem_init(&request->sem2, 1, 0);
    sem_init(&request->sem3, 1, 0);


    pthread_t join_thread;
    global_server->join_request.thread_bool = true;
    pthread_create(&join_thread, NULL, join_function, NULL);
    global_server->join_request.thread_pointer = join_thread;

    pthread_t timer_thread;
    global_server->timer.thread_bool = true;
    pthread_create(&timer_thread, NULL, clock_function, NULL);
    global_server->timer.thread_pointer = timer_thread;


    return global_server;
}




void *join_function(void *var) {

    Lobby_helper *thread_helper = &global_server->join_request;
    Lobby *request = (Lobby *) thread_helper->data;

    while (thread_helper->thread_bool) {

        sem_post(&request->sem);
        sem_wait(&request->sem2);

        if (thread_helper->thread_bool == false)
            break;

        if (global_server->players >= 2) {
            request->number = -1;
        }
        else{
            uint player_index = (request->type == 2) ? 2 : 0;
            for (; player_index < 3; ++player_index) {
                if (global_server->players_and_bots[player_index] == NULL)
                    break;
            }

            shared_memory_player(request, player_index + 1);

            global_server->players++;

            set_position(player_index + 1);


            pthread_t new_thread;
            global_server->players_and_bots[player_index]->thread_bool = true;
            pthread_create(&new_thread, NULL, player_function, NULL);
            global_server->players_and_bots[player_index]->thread_pointer = new_thread;

            request->PID = getpid();
            request->number = player_index + 1;
        }


        sem_post(&request->sem3);
        sem_wait(&request->sem2);

    }
    thread_helper->thread_bool = false;

    return NULL;
}

void *clock_function(void *var) {
    Timer *thread_helper = &global_server->timer;

    while (thread_helper->thread_bool) {
        sleep(1);

        cleaner();

        for (int i = 1; i <= 3; ++i)
            if (global_server->players_and_bots[i - 1] != NULL)
                send_map(i);

        new_round();
    }

    thread_helper->thread_bool = false;

    return NULL;
}

int shared_memory_player(Lobby *lobby_data, uint number) {

    if (number > 3) {

        return -1;
    }

    if (global_server->players_and_bots[number - 1] != NULL) {

        return -1;
    }

    global_server->players_and_bots[number - 1] = (Threads_For_Player_Timer_Lobby *) calloc(1,
                                                                                            sizeof(Threads_For_Player_Timer_Lobby));
    if (global_server->players_and_bots[number - 1] == NULL) {

        return -2;
    }

    int shm_player = -1;
    if (lobby_data->type != 2 && number >= 1 && number <= 2) {
        if (number == 1) shm_player = shm_open("GRACZ_1", O_CREAT | O_RDWR, 0600);
        else if (number == 2) shm_player = shm_open("GRACZ_2", O_CREAT | O_RDWR, 0600);
    } else if (lobby_data->type == 2 && number > 2) {
        if (number == 2 + 1) shm_player = shm_open("BOT", O_CREAT | O_RDWR, 0600);
    }
    if (shm_player == -1) {
        free(global_server->players_and_bots[number - 1]);
        global_server->players_and_bots[number - 1] = NULL;

        return -3;
    }

    ftruncate(shm_player, sizeof(Players_Data));
    Players_Data *player = (Players_Data *) mmap(NULL, sizeof(Players_Data), PROT_READ | PROT_WRITE, MAP_SHARED,
                                                 shm_player,
                                                 0);
    memset(player, 0, sizeof(Players_Data));

    player->number = number;

    player->PID = lobby_data->pid_join;
    player->game_server_PID = getpid();
    player->is_this_new_player = true;
    player->type_player = lobby_data->type;

    uint x, y;
    random_point_player(&x, &y, number);

    player->spanwer_posiition_x = player->player_x = x;
    player->spanwer_posiition_y = player->player_y = y;

    player->frezze = 1;
    player->klawisz = ZERO;

    sem_init(&player->game_sem, 1, 0);
    sem_init(&player->new_data_sem, 1, 1);
    sem_init(&player->player_sem, 1, 0);

    global_server->players_and_bots[number - 1]->data = player;
    global_server->players_and_bots[number - 1]->fd = shm_player;

    send_map(number);

    return 0;
}


void *player_function(void *var) {
    Threads_For_Player_Timer_Lobby *thread_helper;
    Players_Data *player;


    for (int i = 0; i < 3; ++i) {
        if (global_server->players_and_bots[i] == NULL) continue;

        thread_helper = global_server->players_and_bots[i];
        player = (Players_Data *) thread_helper->data;

        if (player->is_this_new_player) {
            player->is_this_new_player = false;
            break;
        }
    }

    while (thread_helper->thread_bool && kill(player->PID, 0) != -1) {
        sem_wait(&player->player_sem);
        if (thread_helper->thread_bool == false)
            break;


        int test = player_press_key(player, player->klawisz);
        if (test)
            player->klawisz = ZERO;
        else
            player->klawisz = AKCEPTACJA;


        sem_post(&player->game_sem);


    }

    player->type_player = 3;

    return NULL;
}

int player_press_key(Players_Data *player, enum klawisze key) {
    if (key == ZERO)
        return 1;

    if (key == WYJSCIE) {
        player->type_player = 3;
        return -1;
    }

    if (player->frezze > 0)
        return 1;

    uint32_t x = player->player_x;
    uint32_t y = player->player_y;

    remove_player_from_map(player->number);
    switch(key){
        case DOL : player->player_y++;
                    break;
        case GORA : player->player_y--;
            break;
        case PRAWO : player->player_x++;
            break;
        case LEWO : player->player_x--;
            break;
    }

    int test = player_rules(player);
    if (test) {
        player->player_x = x;
        player->player_y = y;
        set_position(player->number);

        return 1;
    }

    set_position(player->number);

    player->frezze++;

    return 0;
}

int spawn_player(uint number) {
    if (global_server->players_and_bots[number - 1] == NULL)
        return 1;


    Players_Data *player = (Players_Data *) global_server->players_and_bots[number - 1]->data;

    player->deaths++;
    player->frezze = 1;
    player->player_x = player->spanwer_posiition_x;
    player->player_y = player->spanwer_posiition_y;


    return 0;
}

int send_map(uint number) {
    if (global_server->players_and_bots[number - 1] == NULL)
        return 1;


    Players_Data *player = (Players_Data *) global_server->players_and_bots[number - 1]->data;

    int x = (int) player->player_x - (5 / 2);
    int y = (int) player->player_y - (5 / 2);

    copy_map(x, y, player->player_map_fov);

    return 0;
}

int cleaner() { //garbage collector


    for (int i = 1; i < 3; ++i) {
        if (global_server->players_and_bots[i - 1] == NULL)
            continue;

        Players_Data *player = (Players_Data *) global_server->players_and_bots[i - 1]->data;

        if (player->type_player == 3 || kill(player->PID, 0) == -1) //wysylam sygnal do pida i jezeli jest -1 to proces nieistnieje
            delete_player(i);
    }


    return 0;
}

int player_rules(Players_Data *player) {


    uint32_t x = player->player_x;
    uint32_t y = player->player_y;

    char object = global_server_map[y * 51 + x];
    char object_entity = players_map[y * 51 + x];

    if (object == 'X') {

        return 1;
    }

    if (object == '#') player->frezze++;

    if ((object != ' ' && object != '#') || object_entity != '\0') {
        int test = 1;
        if (player->type_player == 1)
            test = manage_player(player);
        else if (player->type_player == 2)
            test = enemy_rules(player);

        if (test) {

            return test;
        }
    }

    set_position(player->number);
    send_map(player->number);


    return 0;
}

int manage_player(Players_Data *player) {
    uint32_t x = player->player_x;
    uint32_t y = player->player_y;

    char object = global_server_map[y * 51 + x];
    char object_entity = players_map[y * 51 + x];

    if (object == 'A') {
        player->fireplace_bank += player->coins;
        player->coins = 0;

        return 0;
    }
    if (object == 'c' || object == 't' || object == 'T' || object < 0) {
        if (object == 'c') player->coins += 1;
        else if (object == 't') player->coins += 5;
        else if (object == 'T') player->coins += 50;
        else player->coins += get_death_points(-((int) object));

        remove_from_map(x, y);

        return 0;
    }
    if (object_entity == 1 || object_entity == 2) {
        remove_player_from_map(object_entity);
        remove_player_from_map(player->number);

        add_points(player->number, object_entity);

        spawn_player(player->number);
        spawn_player(object_entity);

        set_position(object_entity);
        send_map(object_entity);

        return 0;
    }
    if (object_entity > 2) {
        remove_player_from_map(player->number);

        add_points(player->number, 0);

        spawn_player(player->number);

        set_position(player->number);
        send_map(player->number);

        return 0;
    }

    return 1;
}

int enemy_rules(Players_Data *player) {
    uint32_t x = player->player_x;
    uint32_t y = player->player_y;

    char object_entity = players_map[y * 51 + x];

    if (object_entity > 2)
        return 1;

    if (object_entity > 0) {
        remove_player_from_map(object_entity);

        add_points(object_entity, 0);

        spawn_player(object_entity);
        send_map(object_entity);

        return 0;
    }

    return 1;
}

int set_position(uint number) {
    if (global_server->players_and_bots[number - 1] == NULL)
        return 1;


    Players_Data *player = (Players_Data *) global_server->players_and_bots[number - 1]->data;

    players_map[player->player_y * 51 + player->player_x] = (char) number;


    return 0;
}

int remove_from_map(uint32_t x, uint32_t y) {


    global_server_map[y * 51 + x] = ' ';


    return 0;
}

int remove_player_from_map(uint number) {
    if (global_server->players_and_bots[number - 1] == NULL)
        return 1;


    Players_Data *player = (Players_Data *) global_server->players_and_bots[number - 1]->data;

    players_map[player->player_y * 51 + player->player_x] = 0;


    return 0;
}

int add_points(uint p1, uint p2) {


    Players_Data empty;
    empty.coins = 0;

    Players_Data *entityA = &empty;
    Players_Data *entityB = &empty;

    if (p1 == 1 || p1 == 2)
        entityA = (Players_Data *) global_server->players_and_bots[p1 - 1]->data;

    if (p2 == 1 || p2 == 2)
        entityB = (Players_Data *) global_server->players_and_bots[p2 - 1]->data;

    if (entityA->coins == 0 && entityB->coins == 0) {

        return 1;
    }

    int loot_index = 1;
    for (; loot_index < 10; ++loot_index)
        if (global_server->dropped_loots[loot_index].ilosc_pieniedzy == 0)
            break;

    global_server->dropped_loots[loot_index].ilosc_pieniedzy = entityA->coins + entityB->coins;
    entityA->coins = 0;
    entityB->coins = 0;

    global_server->dropped_loots[loot_index].x = entityA->player_x;
    global_server->dropped_loots[loot_index].y = entityA->player_y;

    global_server_map[entityA->player_y * 51 + entityA->player_x] = (char) (-loot_index);


    return 0;
}

int get_death_points(int loot_number) {


    unsigned int amount = global_server->dropped_loots[loot_number].ilosc_pieniedzy;

    global_server_map[global_server->dropped_loots[loot_number].y * 51 +
                      global_server->dropped_loots[loot_number].x] = ' ';

    global_server->dropped_loots[loot_number].ilosc_pieniedzy = 0;


    return (int) amount;
}

void new_round() {
    global_server->rounds++;
    sem_post(&global_server->sem_to_update);

    for (int i = 0; i < 3; ++i) {
        if (global_server->players_and_bots[i] == NULL)
            continue;

        Players_Data *player = (Players_Data *) global_server->players_and_bots[i]->data;

        if (player->frezze > 0)
            player->frezze--;

        player->round_number = global_server->rounds;
        sem_post(&player->new_data_sem);
    }
}

int delete_player(uint number) {
    Threads_For_Player_Timer_Lobby *toDelete = global_server->players_and_bots[number - 1];

    Players_Data *player = (Players_Data *) toDelete->data;
    player->type_player = 3;


    if (toDelete->thread_bool == true) {
        toDelete->thread_bool = false;
        sem_post(&player->player_sem);
        pthread_join(toDelete->thread_pointer, NULL);
    }

    if (player->player_running == true) {
        player->player_running = false;
        sem_post(&player->new_data_sem);
    }

    if (player->coins > 0) add_points(number, 0);
    remove_player_from_map(number);

    sem_destroy(&player->game_sem);
    sem_destroy(&player->player_sem);
    sem_destroy(&player->new_data_sem);

    memset(player, 0, sizeof(Players_Data));

    munmap(player, sizeof(Players_Data *));
    close(toDelete->fd);


    if (number == 1) {
        shm_unlink("GRACZ_1");
        global_server->players--;
    }
    if (number == 2) {
        shm_unlink("GRACZ_2");
        global_server->players--;
    }
    if (number == 3) { shm_unlink("BOT"); }

    global_server->players_and_bots[number - 1] = NULL;


    return 0;
}

// player

void start_data_transfer(int type) {
    int join_fd = shm_open("LOBBY_SEM", O_RDWR, 0600);
    if (join_fd == -1)
        return;

    ftruncate(join_fd, sizeof(Lobby));
    Lobby *join_shm = (Lobby *) mmap(NULL, sizeof(Lobby), PROT_WRITE | PROT_READ, MAP_SHARED, join_fd,
                                     0);

    sem_wait(&join_shm->sem);

    join_shm->type = type;
    join_shm->pid_join = getpid();

    sem_post(&join_shm->sem2);
    sem_wait(&join_shm->sem3);

    if (join_shm->number == -1) {
        sem_post(&join_shm->sem2);
        return;
    }

    int number = join_shm->number;

    sem_post(&join_shm->sem2);

    munmap(join_shm, sizeof(Lobby));
    close(join_fd);


    int entity_fd = -1;
    if (type != 2 && number >= 1 && number <= 2) {
        if (number == 1) entity_fd = shm_open("GRACZ_1", O_CREAT | O_RDWR, 0600);
        else if (number == 2) entity_fd = shm_open("GRACZ_2", O_CREAT | O_RDWR, 0600);
    } else if (type == 2 && number > 2) {
        if (number == 2 + 1) entity_fd = shm_open("BOT", O_CREAT | O_RDWR, 0600);
    }

    ftruncate(join_fd, sizeof(Players_Data));
    global_player = (Players_Data *) mmap(NULL, sizeof(Players_Data), PROT_WRITE | PROT_READ, MAP_SHARED, entity_fd,
                                          0);

    global_player->fd = entity_fd;
}

void stop_data_transfer(Players_Data *player) {
    int fd = player->fd;

    sem_post(&player->new_data_sem);
    munmap(player, sizeof(Players_Data));
    close(fd);
}

void *enemy_function(void *var) {

    Threads_For_Player_Timer_Lobby *thread_helper = (Threads_For_Player_Timer_Lobby *) var;

//    Players_Data *player = start_data_transfer(2);

    int join_fd = shm_open("LOBBY_SEM", O_RDWR, 0600);
    ftruncate(join_fd, sizeof(Lobby));
    Lobby *join_shm = (Lobby *) mmap(NULL, sizeof(Lobby), PROT_WRITE | PROT_READ, MAP_SHARED, join_fd,
                                     0);

    sem_wait(&join_shm->sem);

    join_shm->type = 2;
    join_shm->pid_join = getpid();

    sem_post(&join_shm->sem2);
    sem_wait(&join_shm->sem3);

    sem_post(&join_shm->sem2);

    munmap(join_shm, sizeof(Lobby));
    close(join_fd);

    int entity_fd = shm_open("BOT", O_CREAT | O_RDWR, 0600);

    ftruncate(join_fd, sizeof(Players_Data));
    Players_Data *player = (Players_Data *) mmap(NULL, sizeof(Players_Data), PROT_WRITE | PROT_READ, MAP_SHARED,
                                                 entity_fd,
                                                 0);

    player->fd = entity_fd;

    thread_helper->data = player;

    int x = -1, y = -1;

    while (thread_helper->thread_bool) {
        sem_wait(&player->new_data_sem);

        if (thread_helper->thread_bool == false)
            break;

        int test = enemy_search(player, &x, &y);

        if (test == 0)
            eneemy_move(player, x, y);
    }

    player->type_player = 3;
    thread_helper->thread_bool = false;

    return NULL;
}

Threads_For_Player_Timer_Lobby *add_enenmy() {
    Threads_For_Player_Timer_Lobby *beast = calloc(1, sizeof(Threads_For_Player_Timer_Lobby));
    if (beast == NULL)
        return NULL;

    pthread_t thread;
    beast->thread_bool = true;

    pthread_create(&thread, NULL, enemy_function, beast);

    beast->thread_pointer = thread;

    return beast;
}

void delete_enenemy(Threads_For_Player_Timer_Lobby *thread_helper) {
    if (thread_helper == NULL)
        return;

    Players_Data *beast = (Players_Data *) thread_helper->data;

    thread_helper->thread_bool = false;
    sem_post(&beast->new_data_sem);
    beast->type_player = 3;
    pthread_join(thread_helper->thread_pointer, NULL);

    stop_data_transfer(beast);

    free(thread_helper);
}

int enemy_search(Players_Data *player, int *x, int *y) {
    char *map = player->player_map_fov;

    int size_x = 5;

    int enemy_pos_x = 2;
    int enemy_pos_y = 2;

    int dx[8] = {0, 0, 1, -1, -1, -1, 1, 1};
    int dy[8] = {-1, 1, 0, 0, -1, 1, -1, 1};

    for (int i = 0; i < 8; ++i) {
        for (int j = 1; j <= 2; ++j) {
            int new_x = enemy_pos_x + j * dx[i];
            int new_y = enemy_pos_y + j * dy[i];

            if (new_x < 0 || new_x >= size_x || map[new_y * size_x + new_x] == '#') {
                break;
            }

            char temp = map[new_y * size_x + new_x];
            if (temp > 0 && temp <= 2) {
                *x = new_x;
                *y = new_y;
                return 0;
            }
        }
    }

    return 1;
}


int eneemy_move(Players_Data *player, int x, int y) {
    int cur_x = 2;
    int cur_y = 2;
    int dx = x - cur_x;
    int dy = y - cur_y;
    int horizontal = dx > 0 ? PRAWO : LEWO;
    int vertical = dy > 0 ? DOL : GORA;

    if (dx == 0) return enenmy_send(player, vertical);
    if (dy == 0) return enenmy_send(player, horizontal);

    if (dx * dy > 0) {
        if (enenmy_send(player, horizontal))
            return enenmy_send(player, vertical);
    } else {
        if (enenmy_send(player, vertical))
            return enenmy_send(player, horizontal);
    }

    return 0;
}

int enenmy_send(Players_Data *player, enum klawisze key) {
    player->klawisz = key;

    sem_post(&player->player_sem);
    sem_wait(&player->game_sem);

    if (player->klawisz == AKCEPTACJA) return 0;

    return 1;
}

// maintain

void run_game() {
    setlocale(LC_ALL, "");

    initscr();
    cbreak();
    noecho();

    WINDOW *map_window = newwin(25, 51, 0, 0);
    WINDOW *info = newwin(100, 200, 0, 51 + 5);

    Threads_For_Player_Timer_Lobby *beast = add_enenmy();
    pthread_t keyboard_thread;
    pthread_create(&keyboard_thread, NULL, read_keyboard, NULL);
    int *n;
    bool server_run = true;

    while (server_run) {
        sem_wait(&global_server->sem_to_update);

        werase(map_window);
        werase(info);

        map_draw_for_game(0, 0, map_window);
        draw_info_for_game(info, global_server);

        wrefresh(info);
        wrefresh(map_window);


        if (pthread_tryjoin_np(keyboard_thread, (void *) &n) == 0) {
            if (*n == 'q' || *n == 'Q') {
                server_run = false;
            } else if (*n == 'b' || *n == 'B') {
                if (!beast) {
                    beast = add_enenmy();
                }
            } else if ((*n == 'c' || *n == 't' || *n == 'T')) {
                add_point(*n);
            }

            free(n);

            if (server_run)
                pthread_create(&keyboard_thread, NULL, read_keyboard, NULL);
        }

    }

    delete_enenemy(beast);

    endwin();

    for (int i = 0; i < 3; ++i) {
        if (global_server->players_and_bots[i] != NULL) {
            delete_player(((Players_Data *) global_server->players_and_bots[i]->data)->number);
        }
    }

    if (global_server->timer.thread_bool) {
        global_server->timer.thread_bool = false;
        pthread_join(global_server->timer.thread_pointer, NULL);
    }


    Lobby *request = (Lobby *) global_server->join_request.data;

    if (global_server->join_request.thread_bool) {
        global_server->join_request.thread_bool = false;
        sem_post(&request->sem2);
        pthread_join(global_server->join_request.thread_pointer, NULL);
    }

    sem_destroy(&request->sem);
    sem_destroy(&request->sem2);
    sem_destroy(&request->sem3);
    munmap(global_server->join_request.data, sizeof(Lobby));
    close(global_server->join_request.fd);
    shm_unlink("LOBBY_SEM");

    sem_destroy(&global_server->sem_to_update);


    free(global_server);
}

void run_player() {
    pid_t server_pid = global_player->game_server_PID;

    setlocale(LC_ALL, "");

    initscr();
    cbreak();
    noecho();

    WINDOW *map_window = newwin(5, 5, 0, 0);
    WINDOW *info = newwin(100, 200, 0, 5 + 30);

    pthread_t keyboard_thread;
    pthread_create(&keyboard_thread, NULL, read_keyboard, NULL);
    int *pressed_key;

    while (1) {

        if (kill(server_pid, 0) == -1) {
            pthread_join(keyboard_thread, (void *) &pressed_key);
            free(pressed_key);
            break;
        }

//        sem_wait(&global_player->new_data_sem);

        if (kill(server_pid, 0) == -1) {
            pthread_join(keyboard_thread, (void *) &pressed_key);
            free(pressed_key);
            break;
        }

        werase(map_window);
        werase(info);
        players_map_draw(0, 0, map_window, global_player->player_map_fov);
        draw_info_for_player(info, global_player);
        wrefresh(map_window);
        wrefresh(info);

        if (pthread_tryjoin_np(keyboard_thread, (void *) &pressed_key) == 0) {
            enum klawisze key = key_value(*pressed_key);

            if (kill(server_pid, 0) == -1) {
                free(pressed_key);
                break;
            }

            enenmy_send(global_player, key);
            if (*pressed_key == 'q') {
                free(pressed_key);
                break;
            }

            free(pressed_key);

            pthread_create(&keyboard_thread, NULL, read_keyboard, NULL);
        }
    }

    endwin();
    int fd = global_player->fd;
    munmap(global_player, sizeof(Players_Data));
    close(fd);
}

void *read_keyboard(void *var) {
    wint_t *c = calloc(1, sizeof(wint_t));

    char *ptr = (char *) c;

    ptr[0] = getwc(stdin);

    if (ptr[0] == 0x1B) {
        ptr[1] = getwc(stdin);
        ptr[2] = getwc(stdin);
    }

    return c;
}

void *player_keyboard(void *var) {
    while (global_player->player_running) {
        wint_t *c = calloc(1, sizeof(wint_t));

        char *ptr = (char *) c;

        ptr[0] = getwc(stdin);

        if (ptr[0] == 0x1B) {
            ptr[1] = getwc(stdin);
            ptr[2] = getwc(stdin);
        }

        int *key_code = (int *) c;

        enum klawisze key = key_value(*key_code);
        enenmy_send(global_player, key);

        if (*key_code == 'q' || *key_code == 'Q') {
            global_player->player_running = false;
        }

        free(key_code);
    }
}

enum klawisze key_value(int key) {
    char *ptr = (char *) &key;
    char res;

    if (ptr[0] == 0x1B) res = ptr[2];
    else res = ptr[0];

    switch (res) {
        case 0x44:
            return LEWO;
        case 0x43:
            return PRAWO;
        case 0x41:
            return GORA;
        case 0x42:
            return DOL;

        case 'q':
        case 'Q':
            return WYJSCIE;

        default:
            return ZERO;
    }
}

void map_draw(int y, int x, WINDOW *window, const char *map) {
    for (int i = 0; i < 25; ++i) {
        for (int j = 0; j < 51; ++j) {

            char object = *(map + i * 51 + j);
            if (object == '\0') continue;

            if (object == 1 || object == 2) {
                mvwprintw(window, i + y, j + x, "%c", object + '0');
                continue;
            }
            if (object == 3) {
                mvwprintw(window, i + y, j + x, "%c", '*');
                continue;
            }
            if (object < 0) {
                mvwprintw(window, i + y, j + x, "%c", 'D');
                continue;
            }

            mvwprintw(window, i + y, j + x, "%c", object);
        }
    }
}

void map_draw_for_game(int y, int x, WINDOW *window) {
    for (int i = 0; i < 25; ++i) {
        for (int j = 0; j < 51; ++j) {

            char map_char = *(global_server_map + i * 51 + j);
            char player_char = *(players_map + i * 51 + j);

            if (map_char < 0) {
                mvwprintw(window, i + y, j + x, "%c", 'D');
            } else if (map_char > 0)
                mvwprintw(window, i + y, j + x, "%c", map_char);

            if (player_char == 1 || player_char == 2) {
                mvwprintw(window, i + y, j + x, "%c", player_char + '0');
            }
            if (player_char == 3) {
                mvwprintw(window, i + y, j + x, "%c", '*');
            }
        }
    }
}

void players_map_draw(int y, int x, WINDOW *window, const char *map) {
    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 5; ++j) {

            char object = *(map + i * 5 + j);
            if (object == '\0') continue;

            if (object == 1 || object == 2) {
                mvwprintw(window, i + y, j + x, "%c", object + '0');
                continue;
            }
            if (object == 3) {
                mvwprintw(window, i + y, j + x, "%c", '*');
                continue;
            }
            if (object < 0) {
                mvwprintw(window, i + y, j + x, "%c", 'D');
                continue;
            }

            mvwprintw(window, i + y, j + x, "%c", object);
        }
    }
}

void draw_info_for_game(WINDOW *playerInfo, Game *server) {

    mvwprintw(playerInfo, 1, 28, "Server's PID: %d", getpid());
    mvwprintw(playerInfo, 2, 28, "Campsite X/Y: %d/%d", 24, 12);
    mvwprintw(playerInfo, 3, 28, "Round number: %d", global_server->rounds);

    mvwprintw(playerInfo, 5, 28, "Parameter:   ");
    mvwprintw(playerInfo, 6, 28, "PID:          ");
    mvwprintw(playerInfo, 7, 28, "Type:         ");
    mvwprintw(playerInfo, 8, 28, "Curr X/Y:     ");
    mvwprintw(playerInfo, 9, 28, "Deaths:       ");

    mvwprintw(playerInfo, 11, 28, "Coins ");
    mvwprintw(playerInfo, 12, 32, "carried ");
    mvwprintw(playerInfo, 13, 32, "brought ");

    mvwprintw(playerInfo, 16, 28, "Legend: ");
    mvwprintw(playerInfo, 17, 29, "1234 - players");
    mvwprintw(playerInfo, 18, 29, "X    - wall");
    mvwprintw(playerInfo, 19, 29, "#    - bushes (slow down)");
    mvwprintw(playerInfo, 20, 29, "*    - wild beast");
    mvwprintw(playerInfo, 21, 29, "c    - one coin");
    mvwprintw(playerInfo, 22, 29, "t    - treasure (10 coins)");
    mvwprintw(playerInfo, 23, 29, "T    - large treasure (50 coins)");
    mvwprintw(playerInfo, 24, 29, "A    - campsite");
    mvwprintw(playerInfo, 25, 29, "D    - dropped treasure");


    int x = strlen("Parameter:   ");
    if (global_server->players_and_bots[0] != NULL) {
        Players_Data *player = (Players_Data *) server->players_and_bots[0]->data;
        mvwprintw(playerInfo, 5, 28 + x, "Player%d", 1);
        mvwprintw(playerInfo, 6, 28 + x, "%d", player->PID);
        mvwprintw(playerInfo, 7, 28 + x, "HUMAN");
        mvwprintw(playerInfo, 8, 28 + x, "%d/%d", player->player_x, player->player_y);
        mvwprintw(playerInfo, 9, 28 + x, "%d", player->deaths);
        mvwprintw(playerInfo, 12, 28 + x, "%d", player->coins);
        mvwprintw(playerInfo, 13, 28 + x, "%d", player->fireplace_bank);
    } else {
        mvwprintw(playerInfo, 5, 28 + x, "-");
        mvwprintw(playerInfo, 6, 28 + x, "-");
        mvwprintw(playerInfo, 7, 28 + x, "-");
        mvwprintw(playerInfo, 8, 28 + x, "-/-");
        mvwprintw(playerInfo, 9, 28 + x, "-");
        mvwprintw(playerInfo, 12, 28 + x, "-");
        mvwprintw(playerInfo, 13, 28 + x, "-");
    }

    int y = strlen("Player1  ");
    if (global_server->players_and_bots[1] != NULL) {
        Players_Data *player = (Players_Data *) server->players_and_bots[1]->data;
        mvwprintw(playerInfo, 5, 28 + x + y, "Player%d", 2);
        mvwprintw(playerInfo, 6, 28 + x + y, "%d", player->PID);
        mvwprintw(playerInfo, 7, 28 + x + y, "HUMAN");
        mvwprintw(playerInfo, 8, 28 + x + y, "%d/%d", player->player_x, player->player_y);
        mvwprintw(playerInfo, 9, 28 + x + y, "%d", player->deaths);
        mvwprintw(playerInfo, 12, 28 + x + y, "%d", player->coins);
        mvwprintw(playerInfo, 13, 28 + x + y, "%d", player->fireplace_bank);
    } else {
        mvwprintw(playerInfo, 5, 28 + x + y, "-");
        mvwprintw(playerInfo, 6, 28 + x + y, "-");
        mvwprintw(playerInfo, 7, 28 + x + y, "-");
        mvwprintw(playerInfo, 8, 28 + x + y, "-/-");
        mvwprintw(playerInfo, 9, 28 + x + y, "-");
        mvwprintw(playerInfo, 12, 28 + x + y, "-");
        mvwprintw(playerInfo, 13, 28 + x + y, "-");
    }
}

void draw_info_for_player(WINDOW *playerModel, Players_Data *playerInfoModel) {
    mvwprintw(playerModel, 1, 28, "Server's PID: %d", playerInfoModel->PID);
    mvwprintw(playerModel, 2, 28, "Campsite X/Y: unknown");
    mvwprintw(playerModel, 3, 28, "Round number: %d", playerInfoModel->round_number); //nw co wpisac

    mvwprintw(playerModel, 5, 28, "Player:   ");
    mvwprintw(playerModel, 6, 29, "Number:     %d", playerInfoModel->number);
    mvwprintw(playerModel, 7, 29, "Type:       HUMAN");
    mvwprintw(playerModel, 8, 29, "Curr X/Y:   %d/%d", playerInfoModel->player_x, playerInfoModel->player_y);
    mvwprintw(playerModel, 9, 29, "Deaths:     %d", playerInfoModel->deaths);

    mvwprintw(playerModel, 11, 29, "Coins found:   %d", playerInfoModel->coins);
    mvwprintw(playerModel, 12, 29, "Coins brought: %d", playerInfoModel->fireplace_bank);

    mvwprintw(playerModel, 16, 28, "Legend: ");
    mvwprintw(playerModel, 17, 29, "1234 - players");
    mvwprintw(playerModel, 18, 29, "X    - wall");
    mvwprintw(playerModel, 19, 29, "#    - bushes (slow down)");
    mvwprintw(playerModel, 20, 29, "*    - wild beast");
    mvwprintw(playerModel, 21, 29, "c    - one coin");
    mvwprintw(playerModel, 22, 29, "t    - treasure (10 coins)");
    mvwprintw(playerModel, 23, 29, "T    - large treasure (50 coins)");
    mvwprintw(playerModel, 24, 29, "A    - campsite");
    mvwprintw(playerModel, 25, 29, "D    - dropped treasure");
}


int random_point_player(uint *x, uint *y, uint number) {

    int player1_x = 24 - 2;
    int player1_y = 12 - 1;
    int player2_x = 24;
    int player2_y = 12 - 1;
    int beast_x = 24 - 1;
    int beast_y = 12 + 1;

    if (number == 1) {
        *x = player1_x;
        *y = player1_y;
    } else if (number == 2) {
        *x = player2_x;
        *y = player2_y;
    } else if (number == 3) {
        *x = beast_x;
        *y = beast_y;
    }

    return 0;
}

void add_point(int point_char) {
    uint x, y;

    while (1) {
        x = rand() % 51;
        y = rand() % 25;

        if (global_server_map[y * 51 + x] == ' ')
            break;
    }

    global_server_map[y * 51 + x] = (char) point_char;
}

int copy_map(int x, int y, char *dest) {
    memset(dest, 0, 5 * 5);

    for (int i = 0; i < 5; ++i) {
        int src_y = y + i;
        if (src_y < 0 || src_y >= 25) {
            continue;
        }

        for (int j = 0; j < 5; ++j) {
            int src_x = x + j;

            if (src_x < 0 || src_x >= 51) {
                continue;
            }

            char object = global_server_map[src_y * 51 + src_x];
            char player = players_map[src_y * 51 + src_x];

            if (object != 0) {
                dest[i * 5 + j] = object;
            }
            if (player != 0) {
                dest[i * 5 + j] = player;
            }
        }
    }
}