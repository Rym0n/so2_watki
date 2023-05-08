#ifndef so2_watki
#define so2_watki

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <err.h>
#include <pthread.h>
#include <stdbool.h>
#include <wchar.h>
#include <stdio.h>
#include <ncurses.h>
#include <locale.h>
#include <curses.h>


enum klawisze{
    ZERO = 0, LEWO, PRAWO, GORA, DOL, WYJSCIE, AKCEPTACJA
};

typedef struct {
    uint x;
    uint y;

    unsigned int ilosc_pieniedzy;
} Grobek; //miejsce smierci

typedef struct {
    int fd;
    int type_player;
    bool player_running;
    bool is_this_new_player;
    char player_map_fov[5 * 5];
    enum klawisze klawisz;

    pid_t PID;
    pid_t game_server_PID;

    sem_t game_sem;
    sem_t player_sem;
    sem_t new_data_sem;

    uint number;
    uint frezze;

    uint spanwer_posiition_x;
    uint spanwer_posiition_y;
    uint round_number;
    uint player_x;
    uint player_y;

    uint deaths;
    uint coins;
    uint fireplace_bank;

} Players_Data; //struktura gracza

typedef struct {
    int fd;

    int thread_bool;

    Players_Data *data;
    pthread_t thread_pointer;

} Threads_For_Player_Timer_Lobby; //kontener do przechowywania watkow i szerd memory dla gracza

typedef struct {
    int thread_bool;
    pthread_t thread_pointer;
} Timer; // struktura do timera zeby wszystko bylo synchronizowane rundy itp

typedef struct {
    int type; //gracz -1 bestia -2
    int number; //numer gracza
    pid_t PID;
    pid_t pid_join;
    sem_t sem;
    sem_t sem2;
    sem_t sem3;
} Lobby; //struktura do dolaczanie graczy


typedef struct {
    int fd;
    int thread_bool;
    Lobby *data;
    pthread_t thread_pointer;

} Lobby_helper; //przechowuje lobby jako shared memory i thread to watek do tego shared memory


typedef struct {

    Threads_For_Player_Timer_Lobby *players_and_bots[3];
    uint players;
    Timer timer;

    Grobek dropped_loots[10];
    uint32_t rounds;

    Lobby_helper join_request;

    sem_t sem_to_update;

} Game; //struktura gry

Game *game_create(); //przygotowuje wszystko i tworzy shared memory i watki dla servera
//osobne funkcje do danego watku. Komunikacja odbywa się w shared memory
void *join_function(void *var);//join fuction tworzy watki player i enemy
void *clock_function(void *var);
void *player_function(void *var); //czeka na klawisze od klienta i przetwarza je dalej
void *enemy_function(void *var);

void add_player(Lobby *join_info);
int delete_player(uint number);

int shared_memory_player(Lobby *lobby_data, uint number);

int player_press_key(Players_Data *player, enum klawisze key);
int spawn_player(uint number);
int send_map(uint number);

int cleaner();

int player_rules(Players_Data *player);
int manage_player(Players_Data *player);
int enemy_rules(Players_Data *player);

int set_position(uint number);
int remove_from_map(uint32_t x, uint32_t y);
int remove_player_from_map(uint number);

int add_points(uint p1, uint p2);
int get_death_points(int loot_number);

void new_round();

void start_data_transfer(int type);
void stop_data_transfer(Players_Data *player);

Threads_For_Player_Timer_Lobby *add_enenmy();
void delete_enenemy(Threads_For_Player_Timer_Lobby *thread_helper);
int enemy_search(Players_Data *player, int *x, int *y);
int eneemy_move(Players_Data *player, int x, int y);
int enenmy_send(Players_Data *player, enum klawisze key);

int random_point(uint *x, uint *y);
int random_point_player(uint *x, uint *y, uint number);
void add_point(int point_char);

int copy_map(int x, int y, char *dest);

void run_game();
void run_player();

void *read_keyboard(void *var);
void *player_keyboard(void *var);

enum klawisze key_value(int key);

void map_draw(int y, int x, WINDOW *, const char *);
void map_draw_for_game(int y, int x, WINDOW *window);
void players_map_draw(int y, int x, WINDOW *, const char *);

void draw_info_for_game(WINDOW *playerInfo, Game *server);
void draw_info_for_player(WINDOW *playerModel, Players_Data *playerInfoModel);

#endif