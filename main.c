// SNAKE GAME!!!
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/time.h>

typedef enum {
    UP, DOWN, RIGHT, LEFT, STILL
} Direction;

typedef struct {
    double x, y;
} Vec2;

typedef enum {
    SNAKE_HEAD,
    SNAKE_PART_UP,
    SNAKE_PART_DOWN,
    SNAKE_PART_LEFT,
    SNAKE_PART_RIGHT,
    FRUIT,
    EMPTY,
} Tile;

typedef struct {
    Vec2 *array;
    int size;
    Direction *direction;
} Snake;

struct termios original;

void setup_input() {
    struct termios raw;
    tcgetattr(STDIN_FILENO, &original);
    raw = original;
    raw.c_lflag &= ~(ICANON | ECHO | ISIG);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    //make read nonblocking
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL, 0) | O_NONBLOCK);
}

void setup() {
    setup_input();
    srandom(time(NULL));
}

void reset_input() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original);
}

Vec2 *new_vec2(const int x, const int y) {
    Vec2 *vec2 = malloc(sizeof(Vec2));
    vec2->x = x;
    vec2->y = y;
    return vec2;
}

Vec2 *vec2_from_dir(const Direction dir) {
    switch (dir) {
        case UP:
            return new_vec2(0, -1);
        case DOWN:
            return new_vec2(0, 1);
        case LEFT:
            return new_vec2(-1, 0);
        case RIGHT:
            return new_vec2(1, 0);
        default:
            return new_vec2(0, 0);
    }
}

Direction handle_input(char *buf) {
    if (!buf) {
        return STILL;
    }
    if (buf[0] == '\x1b' && buf[1] == '[') {
        switch (buf[2]) {
            case 'A':
                return UP;
            case 'B':
                return DOWN;
            case 'C':
                return RIGHT;
            case 'D':
                return LEFT;
            default:
                return STILL;
        }
    }
    switch (tolower(buf[0])) {
        case 'w': return UP;
        case 'a': return LEFT;
        case 's': return DOWN;
        case 'd': return RIGHT;
        default: return STILL;
    }
}

char *get_input() {
    char *buf = malloc(4);
    fflush(stdin);
    if (!fgets(buf, 2, stdin)) {
        free(buf);
        return NULL;
    }
    if (buf[0] == '\x1b') {
        if (!fgets(buf + 1, 3, stdin)) {
            free(buf);
            return NULL;
        }
    }
    return buf;
}

void print_instructions() {
    puts("Use W A S D or arrow keys to control snake!");
    sleep(1);
    puts("Press [q] to quit!");
}

int width = 20, height = 20, num_fruits = 3;
int init_snake_size = 3;
int speed = 5;
int fps = 30;
Snake *snake;
Vec2 *fruits;

bool eq_vec2(const Vec2 *const first, const Vec2 *const second) {
    return (long long) first->x == (long long) second->x && (long long) first->y == (long long) second->y;
}

int rand_range(const int min, const int max) {
    return (int) ((double) random() / ((double) RAND_MAX + 1) * (max - min)) + min;
}

Tile **get_grid();

Tile *get_tile(Tile **grid, const Vec2 *vec2) {
    if (grid == NULL) return NULL;
    if (vec2->y < 0 || vec2->y >= height || vec2->x < 0 || vec2->x >= width) {
        return NULL;
    }
    return &grid[(int) vec2->y][(int) vec2->x];
}

void free_grid(Tile **grid) {
    if (grid == NULL) return;
    for (int i = 0; i < height; i++) {
        free(grid[i]);
    }
    free(grid);
}

Vec2 *get_empty_positions() {
    Tile **grid = get_grid();
    Vec2 *empty = malloc(sizeof(Vec2) * width * height + 1);
    int idx = 0;
    for (int i = 0; i < height; ++i) {
        for (int j = 0; j < width; ++j) {
            if (grid[i][j] == EMPTY) {
                Vec2 *vec2 = new_vec2(j, i);
                empty[idx] = *vec2;
                ++idx;
                free(vec2);
            }
        }
    }
    free_grid(grid);
    Vec2 *vec2 = new_vec2(-1, -1);
    empty[idx] = *vec2;
    free(vec2);
    return empty;
}

Vec2 *new_fruit() {
    Vec2 *empty = get_empty_positions();
    if (empty[0].x == -1) {
        free(empty);
        return NULL;
    }
    int size = 1;
    for (int i = 0; empty[i].x != -1; ++i) {
        ++size;
    }
    const int idx = rand_range(0, size - 1);
    Vec2 *fruit = malloc(sizeof(Vec2));
    fruit->x = empty[idx].x;
    fruit->y = empty[idx].y;
    free(empty);
    return fruit;
}

void init_fruits() {
    fruits = malloc(sizeof(Vec2) * num_fruits + 1);
    for (int i = 0; i < num_fruits; ++i) {
        Vec2 *fruit = new_fruit();
        if (!fruit) continue;
        fruits[i] = *fruit;
        free(fruit);
    }
}

Snake *new_snake(const int size) {
    Snake *snake = malloc(sizeof(Snake));
    snake->array = malloc(sizeof(Vec2) * width * height);
    snake->size = size;
    snake->direction = malloc(sizeof(Direction) * width * height);
    for (int i = 0; i < snake->size; ++i) {
        snake->direction[i] = RIGHT;
    }
    const int head_pos_x = width / 2 + size / 2;
    for (int i = 0; i < snake->size; ++i) {
        Vec2 *vec2 = new_vec2(head_pos_x - i, height / 2);
        snake->array[i] = *vec2;
        free(vec2);
    }
    return snake;
}

void draw() {
    system("clear");
    Tile **grid = get_grid();
    printf("+");
    for (int i = 0; i < 2 * width; ++i) {
        printf("_");
    }
    puts("+");
    for (int i = 0; i < height; ++i) {
        printf("|");
        for (int j = 0; j < width; ++j) {
            char *curr = "  ";
            switch (grid[i][j]) {
                case SNAKE_HEAD:
                    curr = "[]";
                    break;
                case SNAKE_PART_RIGHT:
                    curr = "<(";
                    break;
                case SNAKE_PART_LEFT:
                    curr = ")>";
                    break;
                case SNAKE_PART_DOWN:
                    curr = "/\\";
                    break;
                case SNAKE_PART_UP:
                    curr = "\\/";
                    break;
                case FRUIT:
                    curr = "()";
                    break;
                case EMPTY:
                    curr = "  ";
                    break;
            }
            printf(curr);
        }
        puts("|");
    }
    printf("+");
    for (int i = 0; i < 2 * width; ++i) {
        printf("-");
    }
    puts("+");
    free_grid(grid);
}

void add(Vec2 *vec2, const Vec2 *plus) {
    vec2->x += plus->x;
    vec2->y += plus->y;
}

Tile **get_grid() {
    Tile **grid = malloc(sizeof(Tile *) * height + 1);
    for (int j = 0; j < height; ++j) {
        grid[j] = malloc(sizeof(Tile) * width + 1);
    }
    for (int i = 0; i < height; ++i) {
        for (int j = 0; j < width; ++j) {
            grid[i][j] = EMPTY;
        }
    }
    if (fruits) {
        for (int i = 0; i < num_fruits; ++i) {
            const Vec2 pos = fruits[i];
            *get_tile(grid, &pos) = FRUIT;
        }
    }
    for (int i = 1; i < snake->size; ++i) {
        Tile snake_part;
        switch (snake->direction[i]) {
            case UP:
                snake_part = SNAKE_PART_UP;
                break;
            case DOWN:
                snake_part = SNAKE_PART_DOWN;
                break;
            case RIGHT:
                snake_part = SNAKE_PART_RIGHT;
                break;
            case LEFT:
                snake_part = SNAKE_PART_LEFT;
                break;
            default:
                snake_part = SNAKE_PART_RIGHT;
                break;
        }
        *get_tile(grid, &snake->array[i]) = snake_part;
    }
    *get_tile(grid, &snake->array[0]) = SNAKE_HEAD;

    return grid;
}

void free_all() {
    free(snake->array);
    free(snake->direction);
    free(snake);
    free(fruits);
}

void end_game() {
    free_all();
}

void game_over() {
    puts("GAME OVER!\n");
    printf("SNAKE SIZE: %d\n", snake->size);
    fflush(stdout);
    sleep(1);
}

Direction validate_dir(Direction dir) {
    if (snake->size < 2) {
        return STILL;
    }
    if (snake->array == NULL) return STILL;
    Tile **grid = get_grid();
    Vec2 *dir_vec2 = vec2_from_dir(dir);
    Vec2 head_plus_dir = snake->array[0];
    head_plus_dir.x += dir_vec2->x;
    head_plus_dir.y += dir_vec2->y;
    if (eq_vec2(&head_plus_dir, &snake->array[1])) {
        dir = STILL;
    }
    free_grid(grid);
    free(dir_vec2);
    return dir;
}

void update_snake(Direction direction) {
    if (!snake->direction || !snake->array) return;
    direction = validate_dir(direction);
    if (direction != STILL) {
        snake->direction[0] = direction;
    }
    Vec2 *plus = vec2_from_dir(snake->direction[0]);
    plus->x *= speed / 15.0;
    plus->y *= speed / 15.0;
    Vec2 head = snake->array[0];
    const Vec2 prev = head;
    add(&head, plus);
    if (!eq_vec2(&head, &prev)) {
        for (int i = snake->size - 1; i >= 1; --i) {
            snake->direction[i] = snake->direction[i - 1];
            snake->array[i] = snake->array[i - 1];
        }
    }
    add(&snake->array[0], plus);
    free(plus);
}

void check_fruits() {
    if (!fruits) {
        return;
    }
    for (int i = 0; i < num_fruits; ++i) {
        if (eq_vec2(&snake->array[0], &fruits[i])) {
            Vec2 *fruit = new_fruit();
            if (fruit == NULL) continue;
            fruits[i] = *fruit;
            ++snake->size;
            free(fruit);
        }
    }
}

int check_collisions() {
    if (snake->array[0].x < 0 || snake->array[0].y < 0 || snake->array[0].x >= width || snake->array[0].y >= height) {
        return 1;
    }
    for (int i = 1; i < snake->size; ++i) {
        if (eq_vec2(&snake->array[0], &snake->array[i])) {
            return 1;
        }
    }
    return 0;
}

int check_state() {
    check_fruits();
    return check_collisions();
}

long long curr_time_micros() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long) tv.tv_sec * 1000000 + tv.tv_usec;
}

void set_direction(char *in_buf, Direction *dir_buff, const int idx) {
    if (!(dir_buff && in_buf)) {
        return;
    }
    const Direction dir = handle_input(in_buf);
    if (dir == STILL)
        return;
    dir_buff[idx] = dir;
    free(in_buf);
}

void start_game() {
    snake = new_snake(init_snake_size);
    init_fruits();
    const int INTERVAL = (int) (1e6 / fps);
    usleep(INTERVAL);
    bool lost = false;
    Direction dir = STILL;
    while (true) {
        const int64_t curr = curr_time_micros();
        char *buf = get_input();
        if (buf != NULL && buf[0] == 'q') {
            free(buf);
            break;
        }
        if (buf) {
            dir = handle_input(buf);
        }
        if (curr % INTERVAL == 0) {
            check_fruits();
            update_snake(dir);
            lost = check_state();
            if (lost) {
                break;
            }
            draw();
            free(buf);
        }
    }
    if (lost)
        game_over();
    end_game();
}

void set_arg(const char *arg, const char *name, int *var) {
    const char *pos = strstr(arg, name);
    if (pos == NULL) {
        return;
    }
    if (*(pos + strlen(name)) != '=') {
        return;
    }
    const size_t val_size = strlen(arg) - strlen(name) - 1;
    if (val_size < 1) {
        return;
    }
    char *val = malloc(val_size);
    int i = 0;
    for (const char *curr = pos + strlen(name) + 1; curr < arg + strlen(arg); ++curr) {
        val[i] = *curr;
        ++i;
    }
    char *end = val + val_size;
    long num_val = strtol(val, &end, 10);
    free(val);


    if (end == val) {
        return;
    }
    *var = (int) num_val;
}

typedef struct {
    char *name;
    int *var;
} Arg;

#define NUM_ARGS 6

const Arg args[NUM_ARGS] = {
    {"width", &width},
    {"height", &height},
    {"size", &init_snake_size},
    {"fruits", &num_fruits},
    {"speed", &speed},
    {"fps", &fps},
};

void set_args(const int argc, char **argv) {
    for (int i = 0; i < argc; ++i) {
        for (int j = 0; j < NUM_ARGS; ++j) {
            set_arg(argv[i], args[j].name, args[j].var);
        }
    }
}

int main(const int argc, char **argv) {
    set_args(argc, argv);
    setup();
    start_game();
    reset_input();
}
