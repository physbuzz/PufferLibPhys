#include "connect4.h"
#include "puffernet.h"

#include <unistd.h>
#include "time.h"

const unsigned char NOOP = 8;

void interactive() {
    Weights* weights = load_weights("resources/pong_weights.bin", 6536);
    Default* net = make_default(weights, 1, 42, 128, 7);

    CConnect4 env = {
        .width = 672,
        .height = 576,
        .piece_width = 96,
        .piece_height = 96,
    };
    allocate_cconnect4(&env);
    reset(&env);
 
    Client* client = make_client(env.width, env.height);
    float observations[42] = {0};
    int actions[1] = {0};

    while (!WindowShouldClose()) {
        env.actions[0] = NOOP;
        // user inputs 1 - 7 key pressed
        if (IsKeyDown(KEY_LEFT_SHIFT)) {
            if(IsKeyPressed(KEY_ONE)) env.actions[0] = 0;
            if(IsKeyPressed(KEY_TWO)) env.actions[0] = 1;
            if(IsKeyPressed(KEY_THREE)) env.actions[0] = 2;
            if(IsKeyPressed(KEY_FOUR)) env.actions[0] = 3;
            if(IsKeyPressed(KEY_FIVE)) env.actions[0] = 4;
            if(IsKeyPressed(KEY_SIX)) env.actions[0] = 5;
            if(IsKeyPressed(KEY_SEVEN)) env.actions[0] = 6;
        } else {
            for (int i = 0; i < 42; i++) {
                observations[i] = env.observations[i];
            }
            forward_default(net, (float*)&observations, (int*)&actions);
            env.actions[0] = actions[0];
        }

        if (env.actions[0] >= 0 && env.actions[0] <= 6) {
            step(&env);
            usleep(500000);
        }

        render(client, &env);
    }
    close_client(client);
    free_allocated_cconnect4(&env);
}

void performance_test() {
    long test_time = 10;
    CConnect4 env = {
        .width = 672,
        .height = 576,
        .piece_width = 96,
        .piece_height = 96,
    };
    allocate_cconnect4(&env);
    reset(&env);
 
    long start = time(NULL);
    int i = 0;
    while (time(NULL) - start < test_time) {
        env.actions[0] = rand() % 7;
        step(&env);
        i++;
    }
    long end = time(NULL);
    printf("SPS: %ld\n", i / (end - start));
    free_allocated_cconnect4(&env);
}

int main() {
    //performance_test();
    interactive();
    return 0;
}