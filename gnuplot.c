#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <time.h>

enum { EMPTY=0, X=1, O=2 };     // Define constants for each board state

typedef enum { MODE_1P=1, MODE_2P=2 } game_mode_t;
typedef enum { DIFF_EASY=1, DIFF_MED=2, DIFF_HARD=3 } difficulty_t;

static int board[9];
static int turn = X;

static int winner = 0;                  // 0 = none, 1 = X, 2 = O, 3 = draw
static int winLine[3] = {-1,-1,-1};     // indices of the winning triplet

static game_mode_t mode_choice = MODE_2P;
static difficulty_t diff_choice = DIFF_EASY; // only used in 1P
static int human = X;
static int ai    = O;


static int CheckWinner(const int *b, int *wline) {
    static const int W[8][3] = {
        {0,1,2},{3,4,5},{6,7,8},  // rows
        {0,3,6},{1,4,7},{2,5,8},  // cols
        {0,4,8},{2,4,6}           // diagonals
    };
    for (int i = 0; i < 8; ++i) {
        int a=W[i][0], c=W[i][1], d=W[i][2];
        if (b[a] != EMPTY && b[a] == b[c] && b[c] == b[d]) {
            if (wline) { wline[0]=a; wline[1]=c; wline[2]=d; }
            return b[a]; // X or O
        }
    }
    // any empty? then still playing
    for (int i=0;i<9;i++) if (b[i]==EMPTY) return 0;
    return 3; // draw
}



void draw(FILE *gp) {
    // clear previous objects/arrows
    fprintf(gp, "unset object\n");
    fprintf(gp, "unset arrow\n");

    // canvas and grid
    fprintf(gp,
        "unset key;"                                            // no legend
        "set size square;"                                      // square aspect ratio
        "set xrange [0:3];"                                     // x from 0 to 3
        "set yrange [0:3];"                                     // y from 0 to 3
        "unset xtics;"                                          // no x ticks
        "unset ytics;"                                          // no y ticks
        "unset border;"                                         // no border
        "set style line 1 lc rgb 0x000000 lw 3;\n"
        "set arrow from 1,0 to 1,3 nohead ls 1;\n"
        "set arrow from 2,0 to 2,3 nohead ls 1;\n"
        "set arrow from 0,1 to 3,1 nohead ls 1;\n"
        "set arrow from 0,2 to 3,2 nohead ls 1;\n"
    );                  

    // add objects for X and O
    for (int i = 0; i < 9; ++i) {
        int r = i/3; 
        int c = i%3;
        double x = c + 0.5; 
        double y = r + 0.5;

        // draw X
        if (board[i] == X) {
            // two thick diagonal arrows for X
            fprintf(gp, "set arrow from %f-0.30,%f-0.30 to %f+0.30,%f+0.30 nohead lw 6 lc rgb 0xFF0000\n", x,y,x,y);
            fprintf(gp, "set arrow from %f-0.30,%f+0.30 to %f+0.30,%f-0.30 nohead lw 6 lc rgb 0xFF0000\n", x,y,x,y);
        // draw O
        } else if (board[i] == O) {
            fprintf(gp, "set object circle at %f,%f size 0.30 front fs empty border lc rgb 0x0000FF lw 6\n", x, y);
        }
    }

    // if there is a winner, draw a gold line across the winning triplet
    if (winner == X || winner == O) {
        int a = winLine[0], b = winLine[2];
        int ar = a/3, ac = a%3;
        int br = b/3, bc = b%3;

        double x1 = ac + 0.5, y1 = ar + 0.5;
        double x2 = bc + 0.5, y2 = br + 0.5;

        // small extension so the line touches the glyph edges nicely
        double dx = x2 - x1, dy = y2 - y1;
        double len = (dx*dx + dy*dy) > 0 ? sqrt(dx*dx + dy*dy) : 1.0;
        dx /= len; dy /= len;
        double extend = 0.35; // tweak if you want longer/shorter

        x1 -= dx * extend; y1 -= dy * extend;
        x2 += dx * extend; y2 += dy * extend;

        fprintf(gp, "set arrow from %f,%f to %f,%f lw 8 lc rgb 0x00FF00 nohead front\n", x1,y1,x2,y2);
    }

    // one plot to render everything
    fprintf(gp, "plot NaN notitle\n");
    fflush(gp);
}

static void reset_board(void) {
    for (int i=0;i<9;i++) board[i]=EMPTY;
    winner = 0;
    winLine[0]=winLine[1]=winLine[2]=-1;
    turn = X;
}

static int next_turn(int t) { return (t==X) ? O : X; }

static int empty_count(const int *b) {
    int n=0; for (int i=0;i<9;i++) if (b[i]==EMPTY) n++; return n;
}

static void list_empty(const int *b, int *idxs, int *n) {
    int k=0; for (int i=0;i<9;i++) if (b[i]==EMPTY) idxs[k++]=i; *n=k;
}

/* ---------- AI helpers ---------- */
static int find_winning_move(int who) {
    // try each empty; if placing 'who' wins immediately, return it
    for (int i=0;i<9;i++) if (board[i]==EMPTY) {
        board[i]=who;
        int w = CheckWinner(board, NULL);
        board[i]=EMPTY;
        if (w==who) return i;
    }
    return -1;
}

static int random_move(void) {
    int idxs[9], n;
    list_empty(board, idxs, &n);
    if (n==0) return -1;
    return idxs[rand()%n];
}

/* Minimax (perfect play) — AI is 'ai'; human is 'human' */
static int minimax_score(int player) {
    int w = CheckWinner(board, NULL);
    if (w==ai)    return +10;
    if (w==human) return -10;
    if (w==3)     return 0;

    int best = (player==ai) ? -1000 : 1000;

    for (int i=0;i<9;i++) if (board[i]==EMPTY) {
        board[i]=player;
        int sc = minimax_score(next_turn(player));
        board[i]=EMPTY;
        if (player==ai) {
            if (sc>best) best=sc;
        } else {
            if (sc<best) best=sc;
        }
    }
    return best;
}

static int best_minimax_move(void) {
    int bestMove = -1;
    int bestScore = -1000;
    for (int i=0;i<9;i++) if (board[i]==EMPTY) {
        board[i]=ai;
        int sc = minimax_score(human);
        board[i]=EMPTY;
        if (sc>bestScore) { bestScore=sc; bestMove=i; }
    }
    return bestMove;
}

static int ai_pick_move(void) {
    if (diff_choice == DIFF_EASY) {
        return random_move();
    } else if (diff_choice == DIFF_MED) {
        int m = find_winning_move(ai);
        if (m!=-1) return m;
        m = find_winning_move(human);
        if (m!=-1) return m;
        return random_move();
    } else { // DIFF_HARD
        // small optimization: win/block first, else minimax
        int m = find_winning_move(ai);
        if (m!=-1) return m;
        m = find_winning_move(human);
        if (m!=-1) return m;
        return best_minimax_move();
    }
}

/* ---------- Menu ---------- */
static long read_long_choice(const char *prompt, long lo, long hi) {
    char buf[64];
    while (1) {
        if (prompt && *prompt) fputs(prompt, stdout);
        if (!fgets(buf, sizeof(buf), stdin)) return lo;
        char *p=buf; while (*p && isspace((unsigned char)*p)) p++;
        char *end=NULL; long v = strtol(p,&end,10);
        if (end!=p && v>=lo && v<=hi) return v;
        printf("Please enter a number between %ld and %ld.\n", lo, hi);
    }
}

static void show_menu(void) {
    printf("=== Tic-Tac-Toe ===\n");
    printf("1) 1 Player (you are X)\n");
    printf("2) 2 Players\n");
    long m = read_long_choice("Select mode: ", 1, 2);
    mode_choice = (m==1) ? MODE_1P : MODE_2P;

    if (mode_choice == MODE_1P) {
        printf("\nSelect difficulty:\n");
        printf("1) Easy (random)\n");
        printf("2) Medium (win/block/random)\n");
        printf("3) Hard (minimax)\n");
        long d = read_long_choice("Difficulty: ", 1, 3);
        diff_choice = (difficulty_t)d;
        human = X; ai = O; // human starts by default
        printf("\nYou are X. Enter 1-9 to play.\n\n");
    } else {
        printf("\n2-Player mode: X goes first. Enter 1-9 to play.\n\n");
    }
}

int main(void) {
    srand((unsigned)time(NULL));
    // open a pipe and keep the window open even after the program ends
    FILE *gp = _popen("gnuplot -persist", "w");
    if (!gp) {
        printf("Could not open gnuplot.\n");
        return 1;
    }
    show_menu();
    reset_board();
    // draw initial empty board
    draw(gp);
    printf("Enter position (1-9):\n");

    char buf[64];
    while (fgets(buf, sizeof(buf), stdin)) {
        // handle quit
        char *p=buf; while (*p && isspace((unsigned char)*p)) p++;
        if (*p=='q' || *p=='Q') break;

        // If it's AI's turn (1P) — play automatically
        if (mode_choice==MODE_1P && turn==ai) {
            int m = ai_pick_move();
            if (m>=0) {
                board[m]=ai;
                winner = CheckWinner(board, winLine);
                draw(gp);
                if (winner!=0) {
                    if (winner==3) printf("Draw!\n");
                    else printf("%s wins!\n", winner==X?"X":"O");
                    break;
                }
                turn = next_turn(turn);
                // fall through to prompt human
            } else {
                // no move (shouldn’t happen unless game over)
            }
            printf("Enter position (1-9), or 'q' to quit.\n");
            continue;
        }

        // Human / current player input
        char *end=NULL; long v=strtol(p,&end,10);
        if (end==p || v<1 || v>9) {
            printf("Please enter 1-9 (or q to quit):\n");
            continue;
        }
        int cell=(int)v-1;

        if (board[cell]!=EMPTY) {
            printf("Cell already filled. Try another.\n");
            continue;
        }

        board[cell]=turn;
        winner = CheckWinner(board, winLine);
        draw(gp);
        if (winner!=0) {
            if (winner==3) printf("Draw!\n");
            else printf("%s wins!\n", winner==X?"X":"O");
            break;
        }

        turn = next_turn(turn);

        // If AI should move next, loop will auto-handle at top
        if (mode_choice==MODE_2P || (mode_choice==MODE_1P && turn==human)) {
            printf("Enter position (1-9), or 'q' to quit.\n");
        }
    }

    // close pipe
    _pclose(gp);
    return 0;
}