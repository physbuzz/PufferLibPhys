#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include "raylib.h"
#include <string.h>
#define NOOP 0
#define MOVE_MIN 1
#define MOVE_MAX 1000
#define HALF_MAX_SCORE 432
#define MAX_SCORE 864
#define HALF_PADDLE_WIDTH 31
#define Y_OFFSET 50
#define TICK_RATE 1.0f/60.0f

//  LD_LIBRARY_PATH=raylib-5.0_linux_amd64/lib ./gogame

typedef struct CGo CGo;
struct CGo {
    float* observations;
    unsigned short* actions;
    float* rewards;
    unsigned char* dones;
    int score;
    float episode_return;
    int width;
    int height;
    int* board_x;
    int* board_y;
    int board_width;
    int board_height;
    int grid_square_size;
    int grid_size;
    int* board_states;
    int* previous_board_state;
    int last_capture_position;
    int* temp_board_states;
    int moves_made;
    bool* visited;
};

void generate_board_positions(CGo* env) {
    for (int row = 0; row < env->grid_size ; row++) {
        for (int col = 0; col < env->grid_size; col++) {
            int idx = row * env->grid_size + col;
            env->board_x[idx] = col* env->grid_square_size;
            env->board_y[idx] = row*env->grid_square_size;
        }
    }
}



void init(CGo* env) {
    env->board_x = (int*)calloc((env->grid_size)*(env->grid_size), sizeof(int));
    env->board_y = (int*)calloc((env->grid_size)*(env->grid_size), sizeof(int));
    env->board_states = (int*)calloc((env->grid_size+1)*(env->grid_size+1), sizeof(int));
    env->visited = (bool*)calloc((env->grid_size+1)*(env->grid_size+1), sizeof(bool));
    env->previous_board_state = (int*)calloc((env->grid_size+1)*(env->grid_size+1), sizeof(int));
    env->temp_board_states = (int*)calloc((env->grid_size+1)*(env->grid_size+1), sizeof(int));
    env->last_capture_position = -1;
    generate_board_positions(env);
}

void allocate(CGo* env) {
    init(env);
    env->observations = (float*)calloc((env->grid_size+1)*(env->grid_size+1), sizeof(float));
    env->actions = (unsigned short*)calloc(1, sizeof(unsigned short));
    env->rewards = (float*)calloc(1, sizeof(float));
    env->dones = (unsigned char*)calloc(1, sizeof(unsigned char));
}

void free_initialized(CGo* env) {
    free(env->board_x);
    free(env->board_y);
    free(env->board_states);
    free(env->visited);
    free(env->previous_board_state);
    free(env->temp_board_states);
}

void free_allocated(CGo* env) {
    free(env->actions);
    free(env->observations);
    free(env->dones);
    free(env->rewards);
    free_initialized(env);
}

void compute_observations(CGo* env) {

}



// Add this helper function before check_capture_pieces
bool has_liberties(CGo* env, int* board,  int x, int y, int player) {
    if (x < 0 || x >= env->grid_size+1 || y < 0 || y >= env->grid_size+1) {
        return false;
    }

    int pos = y * (env->grid_size + 1) + x;
    
    if (env->visited[pos]) {
        return false;
    }
    
    env->visited[pos] = true;

    if (board[pos] == 0) {
        return true;  // Found a liberty
    }

    if (board[pos] != player) {
        return false;
    }

    // Check adjacent positions
    int directions[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    for (int i = 0; i < 4; i++) {
        if (has_liberties(env, board, x + directions[i][0], y + directions[i][1], player)) {
            return true;
        }
    }

    return false;
}

void reset_visited(CGo* env) {
    for (int i = 0; i < (env->grid_size + 1) * (env->grid_size + 1); i++) {
        env->visited[i] = 0;
    }
}

void check_capture_pieces(CGo* env, int* board, int tile_placement) {
    int player = board[tile_placement];
    int opponent = (player == 1) ? 2 : 1;
    int directions[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}}; // up, down, left, right
    int placed_x = tile_placement % (env->grid_size + 1);
    int placed_y = tile_placement / (env->grid_size + 1);
    bool captured = false;
    for (int i = 0; i < 4; i++) {
        int dx = directions[i][0];
        int dy = directions[i][1];
        int adjacent_x = tile_placement % (env->grid_size + 1) + dx;
        int adjacent_y = tile_placement / (env->grid_size + 1) + dy;
        int adjacent_pos = adjacent_y * (env->grid_size + 1) + adjacent_x;

        if (adjacent_x >= 0 && adjacent_x < env->grid_size+1 && adjacent_y >= 0 && adjacent_y < env->grid_size+1) {
            if (board[adjacent_pos] == opponent) {
                // Reset visited array for each group check
                reset_visited(env);
                printf("Checking capture around x=%d, y=%d\n", placed_x, placed_y);
                printf("Adjacent stone at x=%d, y=%d, state=%d\n", adjacent_x, adjacent_y, board[adjacent_pos]);
                bool has_liberty = has_liberties(env, board, adjacent_x, adjacent_y, opponent);
                printf("Has liberties: %s\n", has_liberty ? "true" : "false");
                if (!has_liberty) {
                    printf("Capturing group at x=%d, y=%d\n", adjacent_x, adjacent_y);

                    // Capture the group
                    for (int y = 0; y < env->grid_size+1; y++) {
                        for (int x = 0; x < env->grid_size+1; x++) {
                            int pos = y * (env->grid_size + 1) + x;
                            if (env->visited[pos] && board[pos] == opponent) {

                                board[pos] = 0;  // Remove captured stones
                                captured = true;
                                env->last_capture_position = pos;
                                printf("Last Captured stone at x=%d, y=%d\n", x, y);
                            }
                        }
                    }
                }
            }
        }
    }
    if (!captured) {
        env->last_capture_position = -1;
    }
}
bool check_legal_placement(CGo* env, int tile_placement, int player) {
    if (env->board_states[tile_placement] != 0) {
        return false;
    } 

    // check for ko rule violation
    // Create a temporary board to simulate the move
    printf("player: %d\n", player);
    memcpy(env->temp_board_states, env->board_states, sizeof(int) * (env->grid_size+1) * (env->grid_size+1));
    env->temp_board_states[tile_placement] = player;

    // Check if this move would capture any opponent stones
    check_capture_pieces(env, env->temp_board_states, tile_placement);

    // Check if the placed stone has liberties after potential captures
    int x = tile_placement % (env->grid_size + 1);
    int y = tile_placement / (env->grid_size + 1);
    reset_visited(env);
    if (!has_liberties(env, env->temp_board_states, x, y, player)) {
        // If the move results in self-capture, it's illegal
        printf("Self capture\n");
        return false;
    }

    // Check for ko rule violation
    printf("Last captured position: %d\n", env->last_capture_position);
    printf("Tile placement: %d\n", tile_placement);
    bool is_ko = true;
    for (int i = 0; i < (env->grid_size+1) * (env->grid_size+1); i++) {
        if (env->temp_board_states[i] != env->previous_board_state[i]) {
            is_ko = false;
            break;
        }
    }

    if (is_ko) {
        printf("Ko rule violation\n");
        return false;  // Ko rule violation
    }
    return true;
}

  
void reset(CGo* env) {
    env->dones[0] = 0;
    for (int i = 0; i < (env->grid_size+1)*(env->grid_size+1); i++) {
        env->board_states[i] = 0;
    }
    env->moves_made = 0;
}

void step(CGo* env) {
    env->rewards[0] = 0.0;
    int action = (int)env->actions[0];
    if (action >= MOVE_MIN) {
        memcpy(env->previous_board_state, env->board_states, sizeof(int) * (env->grid_size+1) * (env->grid_size+1));
        if (check_legal_placement(env, action-1, 1)) {
            env->board_states[action-1] = 1;
            check_capture_pieces(env, env->board_states, action-1);
        }
        // opponent move
        // Opponent move (player 2)
        int legal_moves[361];  // Maximum possible moves on a 19x19 board
        int num_legal_moves = 0;
        // Find all legal moves
        for (int i = 0; i < (env->grid_size+1)*(env->grid_size+1); i++) {
            if (check_legal_placement(env, i, 2)) {
                legal_moves[num_legal_moves++] = i;
            }
        }
        // Randomly select a legal move
        if (num_legal_moves > 0) {
            int random_index = rand() % num_legal_moves;
            int opponent_move = legal_moves[random_index];
            env->board_states[opponent_move] = 2;
            check_capture_pieces(env, env->board_states, opponent_move);
        }
        /* Testing ko rule violation*/
        // create three fixed moves that can create a ko rule violation
        // the 4 moves are as positions 42,62,80,61
        // the opponet will randomly select on of thse moves only
        // int fixed_moves[4] = {42,62,80,60};
        // if (env->moves_made < 10) {
        //     int random_index = rand() % 4;
        //     int opponent_move = fixed_moves[random_index];
        //     env->board_states[opponent_move] = 2;
        //     check_capture_pieces(env, env->board_states, opponent_move);
        // } else {
        //     if(check_legal_placement(env, 60, 2)) {
        //         env->board_states[60]=2;
        //         check_capture_pieces(env, env->board_states, 60);
        //     }
        // }
        // env->moves_made++;  
        // printf("Moves made: %d\n", env->moves_made);

        
    }

    if (env->dones[0] == 1) {
        env->episode_return = env->score;
        reset(env);
    }
    compute_observations(env);
}


typedef struct Client Client;
struct Client {
    float width;
    float height;
};

Client* make_client(CGo* env) {
    Client* client = (Client*)calloc(1, sizeof(Client));
    client->width = env->width;
    client->height = env->height;

    InitWindow(env->width, env->height, "PufferLib Ray Breakout");
    SetTargetFPS(15);

    //sound_path = os.path.join(*self.__module__.split(".")[:-1], "hit.wav")
    //self.sound = rl.LoadSound(sound_path.encode())

    return client;
}

void render(Client* client, CGo* env) {
    if (IsKeyDown(KEY_ESCAPE)) {
        exit(0);
    }

    BeginDrawing();
    ClearBackground((Color){6, 24, 24, 255});

    for (int row = 0; row < env->grid_size; row++) {
        for (int col = 0; col < env->grid_size; col++) {
            int idx = row * env->grid_size + col;
            int x = env->board_x[idx];
            int y = env->board_y[idx];
            Color tile_color = (Color){ 253, 208, 124, 255 };
            DrawRectangle(x+100, y+100, env->grid_square_size, env->grid_square_size, tile_color);
            DrawRectangleLines(x+100, y+100, env->grid_square_size, env->grid_square_size, BLACK);
        }
    }

    for (int i = 0; i < (env->grid_size + 1) * (env->grid_size + 1); i++) {

        int position_state = env->board_states[i];

        int row = i / (env->grid_size + 1);
        int col = i % (env->grid_size + 1);
        int x = col * env->grid_square_size;
        int y = row * env->grid_square_size;
        // Calculate the circle position based on the grid
        int circle_x = x + 100;
        int circle_y = y + 100;
        // if player draw circle tile for black 
        if (position_state == 1) {
            DrawCircle(circle_x, circle_y, env->grid_square_size / 2, BLACK);
            DrawCircleLines(circle_x, circle_y, env->grid_square_size / 2, BLACK);
        }
        // if enemy draw circle tile for white
        if (position_state == 2) {
            DrawCircle(circle_x, circle_y, env->grid_square_size / 2, WHITE);
            DrawCircleLines(circle_x, circle_y, env->grid_square_size / 2, WHITE);
        }
    }

    EndDrawing();
    //PlaySound(client->sound);
}

void close_client(Client* client) {
    CloseWindow();
    free(client);
}
