#include "myers.h"
#include "workWithFiles.h"
#include "DataStructures/vector.h"
#include "ErrorsDefenition/errors.h"
#include "GeneralFunc/baseGitFunc.h"
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>

typedef enum {
    SAVE,
    DELETE,
    INSERT
} DiffType;

typedef struct {
    DiffType type;
    int index;
} DiffAction;


static int vector_get_or_insert (Vector* vec, char** line) {
    for (size_t i = 0; i < vec->len; ++i) {
        char* entry = *(char**)vector_get(vec, i);
        if (strcmp(entry, *line) == 0) {
            return i;
        }
    }

    if (vector_push(vec, line) != 0) return -1;
    return (int)vector_get_size(vec) - 1;
}


static DiffAction* myers_diff (int n, int m, const int* A, const int* B, int* actions_count) {
    int max = n + m;
    int x, y;

    int** history = (int**)calloc((max + 1), sizeof(int*));
    if (history == NULL) {
        return NULL;
    }
    DiffAction* actions = (DiffAction*)malloc(max * sizeof(DiffAction)); 
    if (actions == NULL) {
        int errno_s = errno;
        free(history);
        errno = errno_s;
        return NULL;
    }
    int act_idx = max;

    for (int D = 0; D <= max; ++D) {
        history[D] = (int*)malloc((2 * D + 1) * sizeof(int));
        if (history[D] == NULL) {
            int errno_s = errno;
            for (int i = 0; i <= max; ++i) free(history[i]);
            free(history);
            free(actions);
            errno = errno_s;
            return NULL;
        }
        int* V = history[D] + D;

        for (int k = -D; k <= D; k += 2) {
            if (D == 0) {
                x = 0;
            }
            else {
                int* V_prev = history[D - 1] + (D - 1);

                if (k == -D || (k != D && V_prev[k-1] < V_prev[k+1])) {
                    x = V_prev[k+1];
                }
                else {
                    x = V_prev[k-1] + 1;
                }
            }
            
            y = x - k;

            while (x < n && y < m && A[x] == B[y]) { 
                x++;
                y++;
            }

            V[k] = x;

            if (x >= n && y >= m) {
                // бэктрекинг

                int cur_x = n;
                int cur_y = m;

                for (int d = D; d > 0; --d) {
                    int cur_k = cur_x - cur_y;
                    int* V_prev = history[d - 1] + (d - 1);

                    int k_prev;
                    if (cur_k == -d || (cur_k != d && V_prev[cur_k - 1] < V_prev[cur_k + 1])) {
                        k_prev = cur_k + 1;
                    }
                    else {
                        k_prev = cur_k - 1;
                    }

                    int x_prev = V_prev[k_prev];
                    int y_prev = x_prev - k_prev;

                    int x_edit; // y_edit;
                    if (k_prev == cur_k + 1) {
                        x_edit = x_prev;
                        // y_edit = y_prev + 1;
                    }
                    else {
                        x_edit = x_prev + 1;
                        // y_edit = y_prev;
                    }

                    while (cur_x > x_edit) {
                        cur_x--;
                        cur_y--;
                        act_idx--;
                        actions[act_idx].type = SAVE;
                        actions[act_idx].index = cur_x;
                    }

                    act_idx--;
                    if (k_prev == cur_k + 1) {
                        actions[act_idx].type = INSERT;
                        actions[act_idx].index = y_prev;
                    }
                    else {
                        actions[act_idx].type = DELETE;
                        actions[act_idx].index = x_prev;
                    }

                    cur_x = x_prev;
                    cur_y = y_prev;
                }

                while (cur_x > 0) {
                    cur_x--;
                    cur_y--;
                    act_idx--;
                    actions[act_idx].type = SAVE;
                    actions[act_idx].index = cur_x;
                }

                // тут кончается 
                for (int i = 0; i <= D; ++i) free(history[i]);
                free(history);

                *actions_count = max - act_idx;
                return &actions[act_idx];
            }
        }
    }

    for (int i = 0; i <= max; ++i) free(history[i]);
    free(history);
    free(actions);
    return NULL;
}


static DiffAction* myers_diff_lines (char** lines_A, int n, char** lines_B, int m, int* actions_cnt) {
    Vector vec;
    int ret_c = vector_init(&vec, sizeof(char*));
    if (ret_c != 0) {
        return NULL;
    }

    int* tokens_A = (int*)malloc(n * sizeof(int));
    int* tokens_B = (int*)malloc(m * sizeof(int));

    for (int i = 0; i < n; ++i) {
        int ret_c = vector_get_or_insert(&vec, &lines_A[i]);
        if (ret_c == -1) {
            int errno_s = errno;
            free(tokens_A);
            free(tokens_B);
            vector_destroy(&vec);
            errno = errno_s;
            return NULL;
        }
        tokens_A[i] = ret_c;
    }

    for (int i = 0; i < m; ++i) {
        int ret_c = vector_get_or_insert(&vec, &lines_B[i]);
        if (ret_c == -1) {
            int errno_s = errno;
            free(tokens_A);
            free(tokens_B);
            vector_destroy(&vec);
            errno = errno_s;
            return NULL;
        }
        tokens_B[i] = ret_c;
    }

    DiffAction* result = myers_diff(n, m, tokens_A, tokens_B, actions_cnt);
    if (result == NULL) {
        int errno_s = errno;
        free(tokens_A);
        free(tokens_B);
        vector_destroy(&vec);
        errno = errno_s;
        return NULL;
    }

    free(tokens_A);
    free(tokens_B);
    vector_destroy(&vec);

    return result;
}


int print_lines_diff (const char* file1_hash, const char* file2_hash) {
    char path[PATH_MAX];
    int n = 0, m = 0;

    join_path(path, ".mygit/files", file1_hash);
    char* buf1 = read_file(path, NULL);
    if (buf1 == NULL) {
        return -1;
    }
    char** lines_A = split_buf_by_lines(buf1, &n);
    if (lines_A == NULL) {
        int errno_s = errno;
        free(buf1);
        errno = errno_s;
        return -1;
    }

    memset(path, 0, PATH_MAX);
    join_path(path, ".mygit/files", file2_hash);
    char* buf2 = read_file(path, NULL);
    if (buf2 == NULL) {
        int errno_s = errno;
        free(buf1);
        free(lines_A);
        errno = errno_s;
        return -1;
    }
    char** lines_B = split_buf_by_lines(buf2, &m);
    if (lines_B == NULL) {
        int errno_s = errno;
        free(buf1);
        free(lines_A);
        free(buf2);
        errno = errno_s;
        return -1;
    }

    int actions_cnt = 0;
    int max = n + m;
    DiffAction* actions = myers_diff_lines(lines_A, n, lines_B, m, &actions_cnt);
    if (actions == NULL) {
        int errno_s = errno;
        free(buf1);
        free(lines_A);
        free(buf2);
        free(lines_B);
        errno = errno_s;
        return -1;
    }

    int changes_from = actions_cnt;
    for (int i = 0; i < actions_cnt; ++i) {
        switch (actions[i].type)
        {
        case DELETE:
            changes_from = i;
            goto skip_others_changes;
            break;
        case INSERT:
            changes_from = i;
            goto skip_others_changes;
            break;
        default:
            break;
        }
    }

skip_others_changes:
    int start = changes_from >= 3 ? changes_from - 3 : 0;
    for (int i = start; i < changes_from; ++i) {
        switch (actions[i].type)
        {
        case SAVE:
            printf(" \t%s\n", lines_A[actions[i].index]);
            break;
        default:
            break;
        }
    }

    int change_detected = 3;
    for (int i = changes_from; i < actions_cnt; ++i) {
        switch (actions[i].type)
        {
        case SAVE:
            if (change_detected > 0) {
                change_detected--;
                if (change_detected == 0) {
                    printf(ANSI_COLOR_BLUE "\n<------------------------------------------------------------------->\n" ANSI_COLOR_RESET);
                }
                printf(" \t%s\n", lines_A[actions[i].index]);
            }
            break;
        case DELETE:
            change_detected = 3;
            printf(ANSI_COLOR_RED "-\t%s\n" ANSI_COLOR_RESET, lines_A[actions[i].index]);
            break;
        case INSERT:
            change_detected = 3;
            printf(ANSI_COLOR_GREEN "+\t%s\n" ANSI_COLOR_RESET, lines_B[actions[i].index]);
            break;
        default:
            break;
        }
    }
    
    free(lines_A);
    free(lines_B);
    free(buf1);
    free(buf2);
    free(actions - (max - actions_cnt)); // майерс возвращяет указатель не на начало выделенного буффера, поэтому сдвигаем в начало

    return 0;
}
