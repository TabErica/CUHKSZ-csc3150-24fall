#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <time.h>

#define ROW 17
#define COL 49
#define WALL_LEN 15
#define GOLD_COUNT 6

pthread_mutex_t mutex;
char map[ROW][COL];  // Game map
int stop_game = 0;   // Game stop control variable
int gold_collected = 0;  // Number of gold pieces collected

// Define object position structure
struct Position {
    int x, y;
} adventurer = {8, 24};  // Adventurer's initial position

struct Position gold[GOLD_COUNT];  // Array of gold positions
struct Position wall[6];  // Initial wall positions

// Function declarations
void *adventurer_move(void *arg);
void *wall_move(void *arg);
void *gold_move(void *arg);
int kbhit(void);
void init_map();
void render_map();
void clear_screen();
void move_wall(int index, int direction);
void move_gold_logic(int index);

int kbhit(void) {
    struct termios oldt, newt;
    int ch;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if (ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }

    return 0;
}

// Initialize the map
void init_map() {
    // Set up borders
    for (int i = 0; i < ROW; i++) {
        for (int j = 0; j < COL; j++) {
            if ((i == 0 || i == ROW - 1) && (j == 0 || j == COL - 1)) {
                map[i][j] = '+';
            } 
            else if (i == 0 || i == ROW - 1) {
                map[i][j] = '-';
            }
            else if (j == 0 || j == COL - 1) {
                map[i][j] = '|';
            }
            else {
                map[i][j] = ' ';
            }
        }
    }
    int wall_rows[6] = {2, 4, 6, 10, 12, 14};
    int gold_rows[6] = {1, 3, 5, 11, 13, 15};

    // Initialize wall positions and draw walls
    for (int i = 0; i < 6; i++) {
        wall[i].x = wall_rows[i];
        wall[i].y = rand() % (COL - WALL_LEN - 2) + 1;

        // Draw walls
        for (int j = 0; j < WALL_LEN; j++) {
            int pos = wall[i].y + j;
            if (pos >= 1 && pos < COL - 1) {
                map[wall[i].x][pos] = '=';
            }
        }
    }

    // Initialize gold positions and draw gold
    for (int i = 0; i < GOLD_COUNT; i++) {
        gold[i].x = gold_rows[i];
        gold[i].y = rand() % (COL - 2) + 1;
        map[gold[i].x][gold[i].y] = '$';
    }

    // Place adventurer
    map[adventurer.x][adventurer.y] = '0';

    render_map();
}

// Render the map
void render_map() {
    printf("\033[H\033[2J");  // Clear the screen
    fflush(stdout);  // Flush the output buffer to force an update
    for (int i = 0; i < ROW; i++) {
        for (int j = 0; j < COL; j++) {
            printf("%c", map[i][j]);
        }
        printf("\n");
    }
    fflush(stdout);  // Ensure all output is printed
}

// Clear the screen
void clear_screen() {
    printf("\033[H\033[2J");  // ANSI escape code to clear screen
    fflush(stdout);
}

void *auto_refresh(void *arg) {
    while (!stop_game) {
        pthread_mutex_lock(&mutex);
        render_map();  // Automatically refresh the map
        pthread_mutex_unlock(&mutex);
        usleep(10000);  // Refresh every 0.01 seconds
    }
    pthread_exit(NULL);
}

// Adventurer movement thread
void *adventurer_move(void *arg) {
    while (!stop_game) {
        if (kbhit()) {
            char dir = getchar();
            pthread_mutex_lock(&mutex);
            map[adventurer.x][adventurer.y] = ' ';  // Clear original position

            // Check if 'q' is pressed to exit the game
            if (dir == 'q') {
                stop_game = 1;
                clear_screen();  // Clear the screen
                pthread_mutex_unlock(&mutex);  // Unlock mutex
                printf("You exit the game.\n");
                break;
            }

            // Update position based on 'w', 'a', 's', 'd'
            if (dir == 'w' && adventurer.x > 1) adventurer.x--;
            if (dir == 's' && adventurer.x < ROW - 2) adventurer.x++;
            if (dir == 'a' && adventurer.y > 1) adventurer.y--;
            if (dir == 'd' && adventurer.y < COL - 2) adventurer.y++;

            // Collision detection
            if (map[adventurer.x][adventurer.y] == '=') {
                map[adventurer.x][adventurer.y] = '0';  // Embed into the wall
                stop_game = 1;
                clear_screen();  // Clear the screen
                pthread_mutex_unlock(&mutex);  // Unlock mutex
                printf("You lose the game!\n");
                break;
            }

            // Collect gold
            for (int i = 0; i < GOLD_COUNT; i++) {
                if (adventurer.x == gold[i].x && adventurer.y == gold[i].y) {
                    gold_collected++;
                    map[gold[i].x][gold[i].y] = ' ';  // Remove gold symbol
                    gold[i].x = -1;  // Mark gold as collected
                    gold[i].y = -1;

                    // Check if the last gold piece is collected
                    if (gold_collected == GOLD_COUNT) {
                        stop_game = 1;
                        clear_screen();  // Clear the screen
                        pthread_mutex_unlock(&mutex);  // Unlock mutex
                        printf("You win the game!\n");
                        break;
                    }
                }
            }

            if (stop_game) {
                break;
            }

            // Update adventurer's position
            map[adventurer.x][adventurer.y] = '0';
            pthread_mutex_unlock(&mutex);
        }
        usleep(10000);  // Prevent high CPU usage
    }
    pthread_exit(NULL);
}

// Wall movement thread
void *wall_move(void *arg) {
    long index = (long)arg;
    int direction = (index % 2 == 0) ? 1 : -1;
    while (!stop_game) {
        pthread_mutex_lock(&mutex);
        move_wall(index, direction);
        pthread_mutex_unlock(&mutex);
        usleep(100000);
    }
    pthread_exit(NULL);
}

// Wall movement logic
void move_wall(int index, int direction) {
    // Clear current wall position
    for (int i = 0; i < WALL_LEN; i++) {
        int pos = wall[index].y + i;
        if (pos < 1) pos += (COL - 2);
        else if (pos >= COL - 1) pos -= (COL - 2);
        if (map[wall[index].x][pos] == '=') {
            map[wall[index].x][pos] = ' ';
        }
    }

    // Update wall position
    wall[index].y += direction;

    // Wrap around handling
    if (direction == -1 && wall[index].y + WALL_LEN - 1 < 1) {
        // When moving left and the wall completely exits the left boundary, reappear from the right
        wall[index].y = COL - WALL_LEN - 1;
    } else if (direction == 1 && wall[index].y >= COL - 1) {
        // When moving right and the wall completely exits the right boundary, reappear from the left
        wall[index].y = 1;
    }

    // Draw new wall position
    for (int i = 0; i < WALL_LEN; i++) {
        int pos = wall[index].y + i;
        if (pos < 1) pos += (COL - 2);
        else if (pos >= COL - 1) pos -= (COL - 2);

        // Check if it overlaps with the adventurer's position
        if (wall[index].x == adventurer.x && pos == adventurer.y) {
            // If collision occurs, clear the wall position (optional)
            map[wall[index].x][pos] = ' ';
            // Set game stop flag
            stop_game = 1;
            clear_screen();  // Clear the screen
            // Print game over message
            printf("You lose the game!\n");
            return;  // Return immediately to avoid further operations
        }

        // Do not overwrite the adventurer
        if (map[wall[index].x][pos] != '0') {
            map[wall[index].x][pos] = '=';
        }
    }
}

// Gold movement logic
void move_gold_logic(int index) {
    static int direction[GOLD_COUNT] = {0};

    if (direction[index] == 0) {
        direction[index] = (rand() % 2 == 0) ? 1 : -1;
    }

    if (gold[index].x == -1) return;

    // Clear current gold position
    if (map[gold[index].x][gold[index].y] == '$') {
        map[gold[index].x][gold[index].y] = ' ';
    }

    // Update gold position
    gold[index].y += direction[index];

    // Wrap around handling
    if (gold[index].y < 1) {
        gold[index].y = COL - 2;
    } else if (gold[index].y >= COL - 1) {
        gold[index].y = 1;
    }

    // Collision detection: gold moves to adventurer's position
    if (gold[index].x == adventurer.x && gold[index].y == adventurer.y) {
        gold_collected++;
        gold[index].x = -1;  // Mark gold as collected
        gold[index].y = -1;

        // Check if the last gold piece is collected
        if (gold_collected == GOLD_COUNT) {
            stop_game = 1;
            clear_screen();  // Clear the screen
            printf("You win the game!\n");
            return;
        }
    } else {
        // If gold and adventurer positions are different, draw the gold
        map[gold[index].x][gold[index].y] = '$';
    }
}

void *gold_move(void *arg) {
    long index = (long)arg;

    while (!stop_game) {
        pthread_mutex_lock(&mutex);
        move_gold_logic(index);
        pthread_mutex_unlock(&mutex);
        usleep(100000);
    }
    pthread_exit(NULL);
}

// Main function
int main() {
    srand(time(0));

    pthread_t adventurer_thread;
    pthread_t wall_threads[6];
    pthread_t gold_threads[GOLD_COUNT];
    pthread_t refresh_thread;

    init_map();

    pthread_mutex_init(&mutex, NULL);

    // Create threads
    pthread_create(&adventurer_thread, NULL, adventurer_move, NULL);

    for (long i = 0; i < 6; i++) {
        pthread_create(&wall_threads[i], NULL, wall_move, (void *)i);
    }

    for (long i = 0; i < GOLD_COUNT; i++) {
        pthread_create(&gold_threads[i], NULL, gold_move, (void *)i);
    }

    pthread_create(&refresh_thread, NULL, auto_refresh, NULL);

    // Wait for threads to finish
    pthread_join(adventurer_thread, NULL);
    for (int i = 0; i < 6; i++) {
        pthread_join(wall_threads[i], NULL);
    }
    for (int i = 0; i < GOLD_COUNT; i++) {
        pthread_join(gold_threads[i], NULL);
    }

    // Terminate the refresh thread
    pthread_cancel(refresh_thread);
    pthread_mutex_destroy(&mutex);
    return 0;
}
