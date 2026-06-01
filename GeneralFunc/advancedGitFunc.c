#include "advancedGitFunc.h"
#include "ErrorsDefenition/errors.h"
#include "CommitAndIndex/commit.h"
#include "CommitAndIndex/index.h"
#include "CommitAndIndex/fileEntry.h"
#include "Utils/hashFunc.h"
#include "Utils/workWithFiles.h"
#include "Utils/branches.h"
#include "Utils/myers.h"
#include "DataStructures/vector.h"
#include "DataStructures/hashmap.h"
#include "GeneralFunc/baseGitFunc.h"
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <sys/stat.h>

// простая обертка для проверки файла на текстовость
static inline int is_file_binary_autopath (const char* file_hash) {
    char path[PATH_MAX];
    join_path(path, ".mygit/files", file_hash);
    return is_file_binary(path);
}


// проверка существует ли папка .mygit
static int check_repo (void) {
    struct stat st;
    if (stat(".mygit", &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, ANSI_COLOR_RED "Current directory not a repository: .mygit not exists\n" ANSI_COLOR_RESET);
        return -1;
    }
    return 0;
}


// возвращает вектор на куче с типом FileEntry с изменившимися файлами
// АХТУНГ!!! забирает владение хеш-таблицей и очищает ее по завершении работы
static Vector* get_changed_files (const char* dirpath) {
    if (dirpath == NULL) { SET_ERR(EINVAL); return NULL; }

    if (check_repo() != 0) return NULL;
        
    int ret_c = 0, err = 0;
    
    // читаем текущий коммит, чтобы понять какие файлы изменились 
    char* commit_hash = get_head_commit_hash();
    if (commit_hash == NULL) return NULL;
    char path[PATH_MAX];
    join_path(path, ".mygit/commits", commit_hash);
    free(commit_hash);
    char* buf = read_file(path, NULL);
    if (buf == NULL) return NULL;

    Commit* commit = commit_init();
    if (commit == NULL) {
        err = errno;
        free(buf);
        errno = err;
        return NULL;
    }
    ret_c = commit_parse(buf, commit);
    if (ret_c != 0) {
        err = errno;
        free(buf);
        commit_destroy(commit);
        errno = err;
        return NULL;
    }
    free(buf);

    // инициализируем и заполняем хеш таблицу с файлами коммита
    // Проходом по ней мы поймем какие файлы изменились или добавились
    ret_c = hashmap_init();
    if (ret_c != 0) {
        err = errno;
        commit_destroy(commit);
        errno = err;
        return NULL;
    }
    size_t com_vec_len = vector_get_size(&commit->files);
    for (size_t i = 0; i < com_vec_len; ++i) {
        FileEntry* entry = (FileEntry*)vector_get(&commit->files, i);
        ret_c = hashmap_insert_entry(entry);
        if (ret_c != 0) {
            err = errno;
            hashmap_destroy();
            commit_destroy(commit);
            errno = err;
            return NULL;
        }
    }

    // получаем все файлы в дирректории
    Vector vec;
    ret_c = vector_init(&vec, sizeof(FileInfo));
    if (ret_c != 0) {
        err = errno;
        hashmap_destroy();
        commit_destroy(commit);
        errno = err;
        return NULL;
    }
    ret_c = get_all_files(dirpath, &vec);
    if (ret_c != 0) {
        err = errno;
        hashmap_destroy();
        commit_destroy(commit);
        vector_destroy(&vec);
        errno = err;
        return NULL;
    }
    // инициализируем выходной вектор
    Vector* out_vec = (Vector*)malloc(sizeof(Vector));
    if (out_vec == NULL) {
        err = errno;
        hashmap_destroy();
        commit_destroy(commit);
        vector_destroy(&vec);
        errno = err;
        return NULL;
    }
    ret_c = vector_init(out_vec, sizeof(FileEntry));
    if (ret_c != 0) {
        err = errno;
        hashmap_destroy();
        commit_destroy(commit);
        vector_destroy(&vec);
        free(out_vec);
        errno = err;
        return NULL;
    }

    // Первый проход
    size_t vec_len = vector_get_size(&vec);
    for (size_t i = 0; i < vec_len;  ++i) {
        FileInfo* entry = (FileInfo*)vector_get(&vec, i);

        FileEntry* in_map_entry = hashmap_get_entry(entry->name);
        if (in_map_entry == NULL && errno == MY_ERR_HASHMAP_NOT_FOUND) {
            FileEntry new;
            strcpy(new.name, entry->name);
            strcpy(new.hash, entry->hash);
            strcpy(new.type, CHANGED_FILE_TYPE_ADD);
            ret_c = vector_push(out_vec, &new);
            if (ret_c != 0) {
                err = errno;
                hashmap_destroy();
                commit_destroy(commit);
                vector_destroy(&vec);
                vector_destroy(out_vec);
                free(out_vec);
                errno = err;
                return NULL;
            }
        }
        else {
            if (in_map_entry == NULL) {
                err = errno;
                hashmap_destroy();
                commit_destroy(commit);
                vector_destroy(&vec);
                vector_destroy(out_vec);
                free(out_vec);
                errno = err;
                return NULL;
            }

            if(strcmp(in_map_entry->hash, entry->hash) != 0) {
                FileEntry new;
                strcpy(new.name, entry->name);
                strcpy(new.hash, entry->hash);
                strcpy(new.type, CHANGED_FILE_TYPE_MOD);
                ret_c = vector_push(out_vec, &new);
                if (ret_c != 0) {
                    err = errno;
                    hashmap_destroy();
                    commit_destroy(commit);
                    vector_destroy(&vec);
                    vector_destroy(out_vec);
                    free(out_vec);
                    errno = err;
                    return NULL;
                }
            }
        }
    }
    hashmap_destroy();

    // Второй проход, на нем будем искать удаленные файлы, по сравнению с текущим коммитом
    hashmap_init();
    for (size_t i = 0; i < vec_len;  ++i) {
        FileInfo* entry = (FileInfo*)vector_get(&vec, i);

        FileEntry new_entry;
        strcpy(new_entry.name, entry->name);
        strcpy(new_entry.hash, entry->hash);
        strcpy(new_entry.type, "");

        ret_c = hashmap_insert_entry(&new_entry); 
        if (ret_c != 0) {
            err = errno;
            hashmap_destroy();
            commit_destroy(commit);
            vector_destroy(&vec);
            vector_destroy(out_vec);
            free(out_vec);
            errno = err;
            return NULL;
        }
    }

    for (size_t i = 0; i < com_vec_len; ++i) {
        FileEntry* entry = (FileEntry*)vector_get(&commit->files, i);
        
        FileEntry* in_map_entry = hashmap_get_entry(entry->name);
        if (in_map_entry == NULL && errno == MY_ERR_HASHMAP_NOT_FOUND) {
            if (strcmp(entry->type, ENTRY_TYPE_DEL) == 0) {
                continue;
            }
            char* first_slash = strchr(entry->name, '/');
            if (first_slash != NULL) {
                *first_slash = '\0';
                if (strcmp(entry->name, dirpath) == 0 || (strcmp(dirpath, ".") == 0 || strcmp(dirpath, "./") == 0)) {
                    *first_slash = '/';
                    goto add_file_as_deleted;
                }
                else {
                    continue;
                }
            }
            else if (strcmp(dirpath, ".") == 0 || strcmp(dirpath, "./") == 0) {
                goto add_file_as_deleted;
            }
            else {
                continue;
            }

add_file_as_deleted:
            FileEntry new;
            strcpy(new.name, entry->name);
            strcpy(new.hash, NULL_HASH);
            strcpy(new.type, CHANGED_FILE_TYPE_DEL);
            ret_c = vector_push(out_vec, &new);
            if (ret_c != 0) {
                err = errno;
                hashmap_destroy();
                commit_destroy(commit);
                vector_destroy(&vec);
                vector_destroy(out_vec);
                free(out_vec);
                errno = err;
                return NULL;
            }
        }
        else if (in_map_entry == NULL) {
            err = errno;
            hashmap_destroy();
            commit_destroy(commit);
            vector_destroy(&vec);
            vector_destroy(out_vec);
            free(out_vec);
            errno = err;
            return NULL;
        }
    }
    hashmap_destroy();

    commit_destroy(commit);
    vector_destroy(&vec);

    return out_vec;
}


// инициализация репозитория
int adv_cmd_init (void) {
    int ret_c = 0, err = 0;
    struct stat st;
    // проверяем существование папки гита
    if (stat(".mygit", &st) == 0) {
        if (S_ISDIR(st.st_mode)) { 
            fprintf(stderr, ANSI_COLOR_RED "Repository already exists!\n" ANSI_COLOR_RESET);
            return -1;
        }
        else { 
            fprintf(stderr, ANSI_COLOR_RED "Repository already exists, but not a directory!\n" ANSI_COLOR_RESET);
            return -1; 
        }
    }
    
    // создаём папку и вложенные папки
    ret_c = mkdir_p(".mygit/commits");
    if (ret_c != 0) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Error while creating commits dir: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        return -1;
    }
    ret_c = mkdir_p(".mygit/files");
    if (ret_c != 0) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Error while creating files dir: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        return -1;
    }
    ret_c = mkdir_p(".mygit/branches");
    if (ret_c != 0) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Error while creating branches dir: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        return -1;
    }

    // создаем пустой HEAD
    ret_c = write_text_file(".mygit/HEAD", NULL_HASH);
    if (ret_c != 0) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Error while writting in HEAD: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        return -1;
    }

    // создаем мастер ветку
    ret_c = create_branch("master");
    if (ret_c != 0) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Error while writting in master branch: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        return -1;
    }

    // создаем пустой индекс
    ret_c = write_text_file(".mygit/index", "");
    if (ret_c != 0) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Error while writting in index: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        return -1;
    }

    // создаем начальный коммит
    Commit* commit = commit_init();
    if (commit == NULL) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Error while creating commit file: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        commit_destroy(commit);
        return -1;
    }
    memset(commit->parent, '0', HASH_LEN - 1);
    commit->parent[HASH_LEN - 1] = '\0';
    memset(commit->own_hash, '0', HASH_LEN - 1);
    commit->own_hash[HASH_LEN - 1] = '\0';
    commit->timestamp = time(NULL);
    ret_c = commit_set_message(commit, "Initial commit");
    if (ret_c != 0) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Error while writting message in initial commit: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        commit_destroy(commit);
        return -1;
    }

    // пишем коммит в файл 
    ret_c = commit_write_file(commit, NULL, "master");
    if (ret_c != 0) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Error while creating commit file: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        commit_destroy(commit);
        return -1;
    }

    commit_destroy(commit);
    printf("Initialazed empty repository in .mygit\n");
    return 0;
}


// создает новую ветку
int adv_cmd_branch (const char* branchName) {
    int ret_c = 0, err = 0;

    if (check_repo() != 0) return -1;

    // вывод всех веток, если не задана новая ветка
    if (branchName == NULL) {
        Vector branches;
        ret_c = vector_init(&branches, sizeof(FileInfo));
        if (ret_c != 0) {
            err = errno;
            fprintf(stderr, ANSI_COLOR_RED "Error while initializing vector with branches names: %s\n" ANSI_COLOR_RESET, strmyerr(err));
            return -1;
        }
        ret_c = get_all_files(".mygit/branches", &branches);
        if (ret_c != 0) {
            err = errno;
            fprintf(stderr, ANSI_COLOR_RED "Error while getting branches: %s\n" ANSI_COLOR_RESET, strmyerr(err));
            vector_destroy(&branches);
            return -1;
        }
        size_t branches_vec_len = vector_get_size(&branches);
        char current[FILE_NAME_MAX];
        ret_c = get_head_branch_name(current);
        if (ret_c != 1 && ret_c != 0) {
            err = errno;
            fprintf(stderr, ANSI_COLOR_RED "Error while getting current branch name: %s\n" ANSI_COLOR_RESET, strmyerr(err));
            vector_destroy(&branches);
            return -1;
        }

        printf(ANSI_COLOR_YELLOW "List of branches: \n" ANSI_COLOR_RESET);
        for (size_t i = 0; i < branches_vec_len; ++i) {
            FileInfo* entry = (FileInfo*)vector_get(&branches, i);

            char* name = strrchr(entry->name, '/') + 1;
            if (strcmp(name, current) == 0) {
                printf("->\t%s\n", name);
            } 
            else {
                printf("  \t%s\n", name);
            }
            
        }
        vector_destroy(&branches);
        return 0;
    }

    ret_c = create_branch(branchName);
    if (ret_c != 0) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Error while creating new branch: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        return -1;
    }

    return 0;
}


// переключается на другую ветку 
int adv_cmd_switch (const char* branchName) {
    int ret_c = 0, err = 0;

    if (branchName == NULL) {
        fprintf(stderr, ANSI_COLOR_RED "Argument invalid\n" ANSI_COLOR_RESET);
        return -1;
    }

    if (check_repo() != 0) return -1;
    ret_c = check_name_per_branch(branchName);
    if (ret_c != true) {
        if (ret_c == false) {
            fprintf(stderr, ANSI_COLOR_RED "Branch doesn't exists\n" ANSI_COLOR_RESET);
            return -1;
        }
        else {
            fprintf(stderr, ANSI_COLOR_RED "Undefined behaviour\n" ANSI_COLOR_RESET);
            return -1;
        }
    }

    char reference[FILE_NAME_MAX];
    snprintf(reference, FILE_NAME_MAX, "ref: %s", branchName);
    ret_c = write_text_file(".mygit/HEAD", reference);
    if (ret_c != 0) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Cannot write HEAD file: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        return -1;
    }

    printf("Switched to '%s'\n", branchName);

    return 0;
}


// создать и сохранить коммит
int adv_cmd_commit (const char* message) {
    if (message == NULL) {
        fprintf(stderr, ANSI_COLOR_RED "Empty message\n" ANSI_COLOR_RESET);
        return -1;
    }

    int ret_c = 0, err = 0;

    if (check_repo() != 0) return -1;

    if (check_detached() == true) {
        fprintf(stderr, ANSI_COLOR_RED "This commad not available in detached status.\n" ANSI_COLOR_RESET);
        return -1;
    }

    // считывыем хеш родителя
    char* parent_hash = get_head_commit_hash();
    if (parent_hash == NULL) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Error with HEAD file: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        free(parent_hash);
        return -1;
    }
    parent_hash[HASH_LEN - 1] = '\0';

    // читаем родительский коммит
    char parent_path[PATH_MAX];
    join_path(parent_path, ".mygit/commits", parent_hash);
    size_t buf_size;
    char* buf = read_file(parent_path, &buf_size);
    if (buf == NULL) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Cannot read parent commit: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        free(parent_hash);
        return -1;
    }
    
    // создаем и заполняем структуру коммита
    Commit* parent_commit = commit_init();
    if (parent_commit == NULL) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Cannot create parent commit image in memory: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        free(parent_hash);
        free(buf);
        return -1;
    }
    ret_c = commit_parse(buf, parent_commit);
    if (ret_c != 0) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Cannot parse parrent commit: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        free(parent_hash);
        free(buf);
        commit_destroy(parent_commit);
        return -1;
    }
    free(buf);

    // читаем индекс
    Vector* index = index_read();
    if (index == NULL) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Cannot read index file: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        free(parent_hash);
        commit_destroy(parent_commit);
        return -1;
    }
    size_t index_size = vector_get_size(index);
    if (index_size == 0) {
        fprintf(stderr, ANSI_COLOR_RED "Nothing to commit\n" ANSI_COLOR_RESET);
        free(parent_hash);
        commit_destroy(parent_commit);
        vector_destroy(index);
        free(index);
        return -1;
    }

    // создаем и заполняем новый коммит
    Commit* new_commit = commit_init();
    if (new_commit == NULL) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Cannot create new commit image in memory: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        free(parent_hash);
        commit_destroy(parent_commit);
        vector_destroy(index);
        free(index);
        return -1;
    }
    new_commit->timestamp = time(NULL);
    strcpy(new_commit->parent, parent_hash);
    memset(new_commit->own_hash, 0, HASH_LEN);
    ret_c = commit_set_message(new_commit, message);
    if (ret_c != 0) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Cannot set message in commit: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        free(parent_hash);
        commit_destroy(parent_commit);
        vector_destroy(index);
        free(index);
        commit_destroy(new_commit);
        return -1;
    }

    // все файлы из индекса
    for (size_t i = 0; i < index_size; ++i) {
        IndexEntry* entry = (IndexEntry*)vector_get(index, i);

        if (strcmp(entry->type, INDEX_TYPE_ADD) == 0) {
            ret_c = commit_add_file(new_commit, entry->name, ENTRY_TYPE_BLOB, entry->hash);
            if (ret_c != 0) {
                err = errno;
                fprintf(stderr, ANSI_COLOR_RED "Сommit recording error: %s\n" ANSI_COLOR_RESET, strmyerr(err));
                free(parent_hash);
                commit_destroy(parent_commit);
                vector_destroy(index);
                free(index);
                commit_destroy(new_commit);
                return -1;
            }
        }
        if (strcmp(entry->type, INDEX_TYPE_DEL) == 0) {
            ret_c = commit_add_file(new_commit, entry->name, ENTRY_TYPE_DEL, NULL);
            if (ret_c != 0) {
                err = errno;
                fprintf(stderr, ANSI_COLOR_RED "Сommit recording error: %s\n" ANSI_COLOR_RESET, strmyerr(err));
                free(parent_hash);
                commit_destroy(parent_commit);
                vector_destroy(index);
                free(index);
                commit_destroy(new_commit);
                return -1;
            }
        }
    }

    // сравниваем файлы из родительского коммита и протаскиваем в новый
    size_t parent_files_len = vector_get_size(&parent_commit->files);
    for (size_t i = 0; i < parent_files_len; ++i) {
        FileEntry* entry = (FileEntry*)vector_get(&parent_commit->files, i);

        int file_in_commit_flag = 0;
        size_t new_commit_files_len = vector_get_size(&new_commit->files);
        for (size_t j = 0; j < new_commit_files_len; ++j) {
            FileEntry* new_commit_entry = (FileEntry*)vector_get(&new_commit->files, j);

            if (strcmp(new_commit_entry->name, entry->name) == 0) {
                file_in_commit_flag = 1;
                break;
            }
        }

        if (file_in_commit_flag == 0) {
            if (strcmp(entry->type, ENTRY_TYPE_DEL) == 0) {
                ret_c = commit_add_file(new_commit, entry->name, ENTRY_TYPE_DEL, NULL);
                if (ret_c != 0) {
                    err = errno;
                    fprintf(stderr, ANSI_COLOR_RED "Сommit recording error: %s\n" ANSI_COLOR_RESET, strmyerr(err));
                    free(parent_hash);
                    commit_destroy(parent_commit);
                    vector_destroy(index);
                    free(index);
                    commit_destroy(new_commit);
                    return -1;
                }                
            }
            else {
                ret_c = commit_add_file(new_commit, entry->name, ENTRY_TYPE_REF, entry->hash);
                if (ret_c != 0) {
                    err = errno;
                    fprintf(stderr, ANSI_COLOR_RED "Сommit recording error: %s\n" ANSI_COLOR_RESET, strmyerr(err));
                    free(parent_hash);
                    commit_destroy(parent_commit);
                    vector_destroy(index);
                    free(index);
                    commit_destroy(new_commit);
                    return -1;
                }
            }
        }
    }

    // записываем новый коммит в файл
    char new_hash[HASH_LEN];
    char branchName[FILE_NAME_MAX];
    char* branch = NULL;
    ret_c = get_head_branch_name(branchName);
    if (ret_c == 0) {
        branch = branchName;
    }
    else if (ret_c == -1) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Cannot write new commit file: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        free(parent_hash);
        commit_destroy(parent_commit);
        vector_destroy(index);
        free(index);
        commit_destroy(new_commit);
        return -1;
    }

    ret_c = commit_write_file(new_commit, new_hash, branch);
    if (ret_c != 0) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Cannot write new commit file: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        free(parent_hash);
        commit_destroy(parent_commit);
        vector_destroy(index);
        free(index);
        commit_destroy(new_commit);
        return -1;
    }

    // явно обнуляем индекс
    Vector new_index;
    ret_c = vector_init(&new_index, sizeof(IndexEntry));
    if (ret_c != 0) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Cannot create index vector: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        free(parent_hash);
        commit_destroy(parent_commit);
        vector_destroy(index);
        free(index);
        commit_destroy(new_commit);
        return -1;
    }
    ret_c = index_write(&new_index);
    if (ret_c != 0) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Cannot write index file: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        free(parent_hash);
        commit_destroy(parent_commit);
        vector_destroy(index);
        free(index);
        commit_destroy(new_commit);
        vector_destroy(&new_index);
        return -1;
    }
    
    printf("New commit succesfully created with hash: '%s'\n", new_hash);

    free(parent_hash);
    commit_destroy(parent_commit);
    commit_destroy(new_commit);
    vector_destroy(index);
    free(index);
    vector_destroy(&new_index);

    return 0;
}


// добавить файл в индекс или по-умному добавить папку
int adv_cmd_add (const char* main_path) {
    if (main_path == NULL) {
        fprintf(stderr, ANSI_COLOR_RED "Argument invalid\n" ANSI_COLOR_RESET);
        return -1;
    }
    int ret_c = 0, err = 0;

    if (check_repo() != 0) return -1;

    if (check_detached() == true) {
        fprintf(stderr, ANSI_COLOR_RED "This commad not available in detached status.\n" ANSI_COLOR_RESET);
        return -1;
    }

    // если подана дирректория, обрабатываем все ее файлы рекурсивно
    if (check_dir(main_path) == true) {
        Vector* changed_files = get_changed_files(main_path);
        if (changed_files == NULL) {
            err = errno;
            fprintf(stderr, ANSI_COLOR_RED "Cannot find changed files in directory: '%s': %s\n" ANSI_COLOR_RESET, main_path, strmyerr(err));
            return -1;
        }
        size_t vec_len = vector_get_size(changed_files);
        for (size_t i = 0; i < vec_len; ++i) {
            FileEntry* entry = (FileEntry*)vector_get(changed_files, i);

            if (strcmp(entry->type, CHANGED_FILE_TYPE_DEL) == 0) {
                ret_c = index_del(entry->name);
                if (ret_c != 0) {
                    err = errno;
                    fprintf(stderr, ANSI_COLOR_RED "Cannot remove file: '%s': %s\n" ANSI_COLOR_RESET, entry->name, strmyerr(err));
                    vector_destroy(changed_files);
                    free(changed_files);
                    return -1;
                }
                printf("File '%s' marked as " ANSI_COLOR_YELLOW "removed" ANSI_COLOR_RESET "\n", entry->name);
            }
            else {
                index_add(entry->name);
                if (ret_c != 0) {
                    err = errno;
                    fprintf(stderr, ANSI_COLOR_RED "Cannot add file: '%s': %s\n" ANSI_COLOR_RESET, entry->name, strmyerr(err));
                    vector_destroy(changed_files);
                    free(changed_files);
                    return -1;
                }
                printf(ANSI_COLOR_YELLOW "Added" ANSI_COLOR_RESET " file '%s' to index\n", entry->name);
            }
        }
        vector_destroy(changed_files);
        free(changed_files);

    }
    // Обрабатываем как одиночный файл
    else if (check_dir(main_path) == false) {
        ret_c = index_add(main_path);
        if (ret_c != 0) {
            err = errno;
            fprintf(stderr, "Cannot add file: '%s': %s\n", main_path, strmyerr(err));
            return -1;
        }
        printf(ANSI_COLOR_YELLOW "Added" ANSI_COLOR_RESET " file '%s' to index\n", main_path);
    }
    else {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Cannot add file: '%s': %s\n" ANSI_COLOR_RESET, main_path, strmyerr(err));
        return -1;
    }

    return 0;
}


// пометить файл или все подфайлы папки удаленным в индексе
int adv_cmd_remove (const char* main_path) {
    if (main_path == NULL) {
        fprintf(stderr, ANSI_COLOR_RED "Argument invalid\n" ANSI_COLOR_RESET);
        return -1;
    }

    int ret_c = 0, err = 0;

    if (check_repo() != 0) return -1;

    if (check_detached() == true) {
        fprintf(stderr, ANSI_COLOR_RED "This commad not available in detached status.\n" ANSI_COLOR_RESET);
        return -1;
    }

    // если подана дирректория, обрабатываем все ее файлы рекурсивно
    if (check_dir(main_path) == true) {
        Vector all_files;
        ret_c = vector_init(&all_files, sizeof(FileInfo));
        if (ret_c != 0) {
            err = errno;
            fprintf(stderr, ANSI_COLOR_RED "Cannot init all files list: %s\n" ANSI_COLOR_RESET, strmyerr(err));
            return -1;
        }
        ret_c = get_all_files(main_path, &all_files);
        if (ret_c != 0) {
            err = errno;
            fprintf(stderr, ANSI_COLOR_RED "Cannot get all files list: %s\n" ANSI_COLOR_RESET, strmyerr(err));
            return -1;
        }
        size_t vec_len = vector_get_size(&all_files);
        for (size_t i = 0; i < vec_len; ++i) {
            FileInfo* entry = (FileInfo*)vector_get(&all_files, i);

            ret_c = index_del(entry->name);
            if (ret_c != 0) {
                err = errno;
                fprintf(stderr, ANSI_COLOR_RED "Cannot remove file: '%s': %s\n" ANSI_COLOR_RESET, entry->name, strmyerr(err));
                return -1;
            }
            printf("File '%s' marked as " ANSI_COLOR_YELLOW "removed" ANSI_COLOR_RESET "\n", entry->name);
        }
        vector_destroy(&all_files);
    }
    // Обрабатываем как одиночный файл
    else if (check_dir(main_path) == false) {
        ret_c = index_del(main_path);
        if (ret_c != 0) {
            err = errno;
            fprintf(stderr, ANSI_COLOR_RED "Cannot remove file: '%s': %s\n" ANSI_COLOR_RESET, main_path, strmyerr(err));
            return -1;
        }
        printf("File '%s' marked as " ANSI_COLOR_YELLOW "removed" ANSI_COLOR_RESET "\n", main_path);
    }
    else {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Cannot remove file: '%s': %s\n" ANSI_COLOR_RESET, main_path, strmyerr(err));
        return -1;
    }

    return 0;
}


// вывод списка файлов из индекса и их состояние + невнесенные изменения в индекс
int adv_cmd_status (void) {
    int ret_c = 0, err = 0;

    if (check_repo () != 0) return -1;

    if (check_detached() == true) {
        fprintf(stderr, ANSI_COLOR_RED "This commad not available in detached status.\n" ANSI_COLOR_RESET);
        return -1;
    }

    // вывод текущей ветки
    char curr_branch[FILE_NAME_MAX]; 
    ret_c = get_head_branch_name(curr_branch);
    if (ret_c == 0) {
        printf(ANSI_COLOR_YELLOW "Current branch:" ANSI_COLOR_RESET " %s\n", curr_branch);
    }
    else if (ret_c == -1) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Cannot read current branch name: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        return -1;
    }

    // читаем и создаем последний коммит
    Commit* last_commit = commit_init();
    if (last_commit == NULL) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Cannot init last commit: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        return -1;
    }
    char* last_commit_hash = get_head_commit_hash();
    if (last_commit_hash == NULL) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Error with HEAD file: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        commit_destroy(last_commit);
        return -1;
    }
    last_commit_hash[HASH_LEN - 1] = '\0';
    char last_commit_path[PATH_MAX];
    join_path(last_commit_path, ".mygit/commits", last_commit_hash);
    size_t buf_size;
    char* buf = read_file(last_commit_path, &buf_size);
    if (buf == NULL) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Cannot read last commit: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        free(last_commit_hash);
        commit_destroy(last_commit);
        return -1;
    }
    ret_c = commit_parse(buf, last_commit);
    if (ret_c != 0) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Cannot parse last commit: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        free(last_commit_hash);
        commit_destroy(last_commit);
        return -1;
    }
    free(buf);
    free(last_commit_hash);

    // создаем хеш-карту с файлами коммита
    ret_c = hashmap_init ();
    if (ret_c != 0) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Cannot initialize hashmap: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        commit_destroy(last_commit);
        return -1;
    } 
    size_t last_commit_file_vec_len = vector_get_size(&last_commit->files);
    for (size_t i = 0; i < last_commit_file_vec_len; ++i) {
        FileEntry* file = (FileEntry*)vector_get(&last_commit->files, i);
        ret_c = hashmap_insert_entry(file);
        if (ret_c != 0) {
            err = errno;
            fprintf(stderr, ANSI_COLOR_RED "Cannot create hashmap with commit files: %s\n" ANSI_COLOR_RESET, strmyerr(err));
            commit_destroy(last_commit);
            hashmap_destroy();
            return -1;
        } 
    }

    // читаем индекс, его и будем сравнивать
    Vector* idx = index_read();
    if (idx == NULL) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Cannot read index: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        commit_destroy(last_commit);
        hashmap_destroy();
        return -1;
    }

    size_t vec_size = vector_get_size(idx);
    if (vec_size == 0) {
        printf("Nothing staged.\n");
    }
    else {
        printf("Changes to be committed:\n");
        for (size_t i = 0; i < vec_size; ++i) {
            IndexEntry* entry = (IndexEntry*)vector_get(idx, i);

            if (strcmp(entry->type, INDEX_TYPE_DEL) == 0) {
                printf("\t" ANSI_COLOR_YELLOW "deleted:" ANSI_COLOR_RESET " %s\n", entry->name);
            }
            else if (strcmp(entry->type, INDEX_TYPE_ADD) == 0) {
                // если файл есть в карте -> проверяем хеш:
                // если хеш сопадает, то файл заново добавлен явно
                // если нет, то файл изменен
                // если файла нет в карте, то добавлен
                FileEntry* last_commit_entry_in_hashmap = hashmap_get_entry(entry->name);
                if (last_commit_entry_in_hashmap == NULL && errno == MY_ERR_HASHMAP_NOT_FOUND) {
                    printf("\t" ANSI_COLOR_YELLOW "added:" ANSI_COLOR_RESET " %s\n", entry->name);
                }
                else {
                    if (last_commit_entry_in_hashmap == NULL) {
                        err = errno;
                        fprintf(stderr, ANSI_COLOR_RED "Error with commit hashmap: %s\n" ANSI_COLOR_RESET, strmyerr(err));
                        commit_destroy(last_commit);
                        vector_destroy(idx);
                        free(idx);
                        hashmap_destroy();
                        return -1;
                    }
                    if (strcmp(last_commit_entry_in_hashmap->type, ENTRY_TYPE_DEL) == 0) {
                        printf("\t" ANSI_COLOR_YELLOW "added:" ANSI_COLOR_RESET " %s\n", entry->name);
                    }   
                    else if (strcmp(entry->hash, last_commit_entry_in_hashmap->hash) == 0) {
                        printf("\t" ANSI_COLOR_YELLOW "added without changes:" ANSI_COLOR_RESET " %s\n", entry->name);
                    }
                    else {
                        printf("\t" ANSI_COLOR_YELLOW "file changed:" ANSI_COLOR_RESET " %s\n", entry->name);
                    }
                }
            }
        }
    }

    hashmap_destroy();
    commit_destroy(last_commit);

    // Ниже выводим все файлы которые не попали в индекс
    Vector* changed_files = get_changed_files(".");
    if (changed_files == NULL) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Cannot get changed files: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        vector_destroy(idx);
        free(idx);
        vector_destroy(changed_files);
        free(changed_files);
        return -1;
    }

    ret_c = hashmap_init();
    if (ret_c != 0) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Error with index hashmap: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        vector_destroy(idx);
        free(idx);
        vector_destroy(changed_files);
        free(changed_files);
        return -1;
    }
    // заполняем файлами из индекса
    for (size_t i = 0; i < vec_size; ++i) {
        IndexEntry* entry = (IndexEntry*)vector_get(idx, i);

        ret_c = hashmap_insert_entry(entry);
        if (ret_c != 0) {
            err = errno;
            fprintf(stderr, ANSI_COLOR_RED "Error with index hashmap: %s\n" ANSI_COLOR_RESET, strmyerr(err));
            vector_destroy(idx);
            free(idx);
            hashmap_destroy();
            return -1;
        }
    }

    size_t changed_files_vec_len = vector_get_size(changed_files);
    if (changed_files_vec_len == 0) {
        printf("All changes are saved!\n");
    }
    else {
        printf("Unsaved changes: \n");
        // Если файл не в индексе -> выводи информацию о нем
        // Если в индексе -> сравниваем хеши, если различаются -> выводим информацию
        for (size_t i = 0; i < changed_files_vec_len;  ++i) {
            FileEntry* entry = (FileEntry*)vector_get(changed_files, i);
            FileEntry* inmap_entry = hashmap_get_entry(entry->name);
            if (inmap_entry == NULL && errno == MY_ERR_HASHMAP_NOT_FOUND) {
                if (strcmp(entry->type, CHANGED_FILE_TYPE_ADD) == 0) {
                    printf("\t" ANSI_COLOR_YELLOW "created but not in index:" ANSI_COLOR_RESET " %s\n", entry->name);
                }
                else if (strcmp(entry->type, CHANGED_FILE_TYPE_MOD) == 0) {
                    printf("\t" ANSI_COLOR_YELLOW "modified but not in index:" ANSI_COLOR_RESET " %s\n", entry->name);
                }
                else if (strcmp(entry->type, CHANGED_FILE_TYPE_DEL) == 0) {
                    printf("\t" ANSI_COLOR_YELLOW "deleted but not marked as deleted:" ANSI_COLOR_RESET " %s\n", entry->name);
                }
            }
            else {
                if (inmap_entry == NULL) {
                    err = errno;
                    fprintf(stderr, ANSI_COLOR_RED "Error with index hashmap: %s\n" ANSI_COLOR_RESET, strmyerr(err));
                    vector_destroy(idx);
                    free(idx);
                    vector_destroy(changed_files);
                    free(changed_files);
                    hashmap_destroy();
                    return -1;
                }
                if (strcmp(entry->hash, inmap_entry->hash) != 0) {
                    if (strcmp(entry->type, CHANGED_FILE_TYPE_ADD) == 0) {
                        printf("\t" ANSI_COLOR_YELLOW "version in index not final:" ANSI_COLOR_RESET " %s\n", entry->name);
                    }
                    else if (strcmp(entry->type, CHANGED_FILE_TYPE_MOD) == 0) {
                        printf("\t" ANSI_COLOR_YELLOW "version in index not final:" ANSI_COLOR_RESET " %s\n", entry->name);
                    }
                    else if (strcmp(entry->type, CHANGED_FILE_TYPE_DEL) == 0) {
                        printf("\t" ANSI_COLOR_YELLOW "deleted but not marked as deleted:" ANSI_COLOR_RESET " %s\n", entry->name);
                    }
                }
            }
        }
    }

    hashmap_destroy();
    vector_destroy(changed_files);
    free(changed_files);
    vector_destroy(idx);
    free(idx);

    return 0;
}


// выводит цепочку коммитов c прикольным форматированием 
int adv_cmd_log (const char* start_commit, const int* commit_cnt, const char* end_commit) {
    if (commit_cnt != NULL && *commit_cnt < 0) {
        fprintf(stderr, ANSI_COLOR_RED "Invalid count of commit.\n" ANSI_COLOR_RESET);
        return -1;
    }
    
    int ret_c = 0, err = 0;

    if (check_repo() != 0) return -1;

    if (check_detached() == true) {
        fprintf(stderr, ANSI_COLOR_RED "This commad not available in detached status.\n" ANSI_COLOR_RESET);
        return -1;
    }

    // получаем хеши начального и конечного коммита
    char* hash;
    if (start_commit == NULL) {
        hash = get_head_commit_hash();
        if (hash == NULL) {
            err = errno;
            fprintf(stderr, ANSI_COLOR_RED "Cannot resolve HEAD file: %s\n" ANSI_COLOR_RESET, strmyerr(err));
            return -1;
        }
        hash[HASH_LEN - 1] = '\0';
    }
    else {
        hash = resolve_branch(start_commit);
        if (hash == NULL) {
            err = errno;
            fprintf(stderr, ANSI_COLOR_RED "Cannot resolve first input branch: %s\n" ANSI_COLOR_RESET, strmyerr(err));
            return -1;
        }
        hash[HASH_LEN - 1] = '\0';
    }

    char* end_hash;
    if (end_commit == NULL) {
        end_hash = strdup(NULL_HASH);
        if (end_hash == NULL) {
            err = errno;
            fprintf(stderr, ANSI_COLOR_RED "Cannot resolve second input branch: %s\n" ANSI_COLOR_RESET, strmyerr(err));
            free(hash);
            return -1;
        }
        end_hash[HASH_LEN - 1] = '\0';
    }
    else {
        end_hash = resolve_branch(end_commit);
        if (end_hash == NULL) {
            err = errno;
            fprintf(stderr, ANSI_COLOR_RED "Cannot resolve second input branch: %s\n" ANSI_COLOR_RESET, strmyerr(err));
            free(hash);
            return -1;
        }
        end_hash[HASH_LEN - 1] = '\0';
    }

    // выводим информацию о каждом коммите
    int cnt = 0;
    int total_cnt = (commit_cnt == NULL) ? INT32_MAX : *commit_cnt;
    while (strcmp(hash, NULL_HASH) != 0 && strcmp(hash, end_hash) != 0 && cnt < total_cnt) {
        char path[PATH_MAX];
        join_path(path, ".mygit/commits", hash);
        Commit* commit = commit_init();
        if (commit == NULL) {
            err = errno;
            fprintf(stderr, ANSI_COLOR_RED "Cannot init commit: %s\n" ANSI_COLOR_RESET, strmyerr(err));
            free(hash);
            free(end_hash);
            return -1;
        }
        char* buf = read_file(path, NULL);
        if (buf == NULL) {
            err = errno;
            fprintf(stderr, ANSI_COLOR_RED "Cannot read commit file: %s\n" ANSI_COLOR_RESET, strmyerr(err));
            free(hash);
            free(end_hash);
            commit_destroy(commit);
            return -1;
        }
        ret_c = commit_parse(buf, commit);
        if (ret_c != 0) {
            err = errno;
            fprintf(stderr, ANSI_COLOR_RED "Cannot parse commit file: %s\n" ANSI_COLOR_RESET, strmyerr(err));
            free(hash);
            free(end_hash);
            commit_destroy(commit);
            free(buf);
            return -1;
        }
        struct tm *local = localtime(&commit->timestamp);
        char time_buf[100];
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", local);

        printf(ANSI_COLOR_YELLOW "commit: %s\n" ANSI_COLOR_RESET, hash);
        printf("Date: %s\n", time_buf);
        printf(ANSI_BOLD "\n\t%s\n\n" ANSI_COLOR_RESET, commit->message);

        strcpy(hash, commit->parent);
        free(buf);
        commit_destroy(commit);
        cnt++;
    }
    // печатаем последний ненулевой коммит
    if (strcmp(hash, end_hash) == 0 && strcmp(hash, NULL_HASH) != 0) {
        char path[PATH_MAX];
        join_path(path, ".mygit/commits", hash);
        Commit* commit = commit_init();
        if (commit == NULL) {
            err = errno;
            fprintf(stderr, ANSI_COLOR_RED "Cannot init commit: %s\n" ANSI_COLOR_RESET, strmyerr(err));
            free(hash);
            free(end_hash);
            return -1;
        }
        char* buf = read_file(path, NULL);
        if (buf == NULL) {
            err = errno;
            fprintf(stderr, ANSI_COLOR_RED "Cannot read commit file: %s\n" ANSI_COLOR_RESET, strmyerr(err));
            free(hash);
            free(end_hash);
            commit_destroy(commit);
            return -1;
        }
        ret_c = commit_parse(buf, commit);
        if (ret_c != 0) {
            err = errno;
            fprintf(stderr, ANSI_COLOR_RED "Cannot parse commit file: %s\n" ANSI_COLOR_RESET, strmyerr(err));
            free(hash);
            free(end_hash);
            commit_destroy(commit);
            free(buf);
            return -1;
        }
        struct tm *local = localtime(&commit->timestamp);
        char time_buf[100];
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", local);

        printf(ANSI_COLOR_YELLOW "commit: %s\n" ANSI_COLOR_RESET, hash);
        printf("Date: %s\n", time_buf);
        printf(ANSI_BOLD "\n\t%s\n\n" ANSI_COLOR_RESET, commit->message);

        strcpy(hash, commit->parent);
        free(buf);
        commit_destroy(commit);
    }

    free(hash);
    free(end_hash);

    return 0;
}


// восстановление состояния репозитория по комииту
int adv_cmd_checkout (const char* branchName, const char* fileName) {
    if (branchName == NULL) { fprintf(stderr, ANSI_COLOR_RED "Argument invalid\n" ANSI_COLOR_RESET); return -1; }
    int ret_c = 0, err = 0;

    if (check_repo() != 0) return -1;

    // получаем все измененные файлы, если они есть, то запрещаем воостановление по другим коммитам
    Vector* changed = get_changed_files(".");
    if (changed == NULL) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Cannot get changed files: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        return -1;
    }
    size_t changed_cnt = vector_get_size(changed);
    vector_destroy(changed);
    free(changed);
    int detached = check_detached();
    if (detached == -1) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Cannot check detached: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        return -1;
    }
    // Если находимся в состоянии отсоединенной головы, то можно восстанавливаться всегда
    if (changed_cnt > 0 && detached == false) {
        fprintf(stderr, ANSI_COLOR_RED "Currency state doesn't commited.\n" ANSI_COLOR_RESET);
        return -1;
    }
    
    // получаем коммит для восстановления
    char* commit_hash = resolve_branch(branchName);
    if (commit_hash == NULL) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Cannot resolve branch: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        return -1;
    }

    // проверяем, пришел ли запрос на восстановления одного файла
    if (fileName != NULL) {
        ret_c = cmd_checkout(commit_hash, fileName);
        int errno_s = errno;
        free(commit_hash);
        errno = errno_s;
        return ret_c;
    }

    char path[PATH_MAX];
    join_path(path, ".mygit/commits", commit_hash);
    char* buf = read_file(path, NULL);
    if (buf == NULL) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Cannot commit file: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        free(commit_hash);
        return -1;
    }
    Commit* commit = commit_init();
    if (commit == NULL) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Cannot init commit: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        free(commit_hash);
        return -1;
    }
    ret_c = commit_parse(buf, commit);
    if (ret_c != 0) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Cannot parse commit file: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        free(commit_hash);
        commit_destroy(commit);
        return -1;
    }
    free(buf);

    // удаляем все файлы из репозитория
    ret_c = remove_all_repofiles(".");
    if (ret_c != 0) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Cannot remove files in current directory: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        return -1;
    }

    // восстанавливаем каждый файл
    size_t com_vec_len = vector_get_size(&commit->files);
    for (size_t i = 0; i < com_vec_len; ++i) {
        FileEntry* entry = (FileEntry*)vector_get(&commit->files, i);
        if (strcmp(entry->type, ENTRY_TYPE_DEL) == 0) continue;

        memset(path, 0, PATH_MAX);
        join_path(path, ".mygit/files", entry->hash);
        size_t write_size = 0;
        buf = read_file(path, &write_size);
        if (buf == NULL) {
            err = errno;
            fprintf(stderr, ANSI_COLOR_RED "Cannot read file: '%s': %s\n" ANSI_COLOR_RESET, entry->name, strmyerr(err));
            free(commit_hash);
            commit_destroy(commit);
            return -1;
        }
        ret_c = create_and_write_file(entry->name, buf, write_size);
        if (ret_c != 0) {
            err = errno;
            fprintf(stderr, ANSI_COLOR_RED "Cannot create file: '%s': %s\n" ANSI_COLOR_RESET, entry->name, strmyerr(err));
            free(commit_hash);
            commit_destroy(commit);
            free(buf);
            return -1;
        }
        free(buf);
    }
    commit_destroy(commit);

    // выставляем в хед ссылку на ветку или хеш коммита
    int is_it_branch = check_name_per_branch(branchName);
    if (is_it_branch == true) {
        char current_branch[PATH_MAX];
        snprintf(current_branch, PATH_MAX, "ref: %s", branchName);
        ret_c = write_text_file(".mygit/HEAD", current_branch);
        if (ret_c != 0) {
            err = errno;
            fprintf(stderr, ANSI_COLOR_RED "Cannot write HEAD file: %s\n" ANSI_COLOR_RESET, strmyerr(err));
            free(commit_hash);
            return -1;
        }
    }
    else if (is_it_branch == false) {
        ret_c = write_text_file(".mygit/HEAD", commit_hash);
        if (ret_c != 0) {
            err = errno;
            fprintf(stderr, ANSI_COLOR_RED "Cannot write HEAD file: %s\n" ANSI_COLOR_RESET, strmyerr(err));
            free(commit_hash);
            return -1;
        }
    }
    else {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Critical error: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        free(commit_hash);
        return -1;
    }

    free(commit_hash);

    printf("State restored succesfully!\n");

    return 0;

}


// выводит список измененных файлов и их изиененные строки
int adv_cmd_diff (const char* branchName1, const char* branchName2) {
    if (branchName1 == NULL) { fprintf(stderr, ANSI_COLOR_RED "Argument invalid\n" ANSI_COLOR_RESET); return -1; }
    int ret_c = 0, err = 0;

    if (check_repo () != 0) return -1;

    if (check_detached() == true) {
        fprintf(stderr, ANSI_COLOR_RED "This commad not available in detached status.\n" ANSI_COLOR_RESET);
        return -1;
    }

    char path[PATH_MAX];
    char* buf = NULL;

    // читаем оба коммита
    char* commit2_hash = NULL;
    if (branchName2 == NULL) {
        commit2_hash = get_head_commit_hash();
        if (commit2_hash == NULL) {
            err = errno;
            fprintf(stderr, ANSI_COLOR_RED "Cannot get current commit hash: %s\n" ANSI_COLOR_RESET, strmyerr(err));
            return -1;
        }
    }
    else {
        commit2_hash = resolve_branch(branchName2);
        if (commit2_hash == NULL) {
            err = errno;
            fprintf(stderr, ANSI_COLOR_RED "Cannot get commit2 hash: %s\n" ANSI_COLOR_RESET, strmyerr(err));
            return -1;
        }
    }
    join_path(path, ".mygit/commits", commit2_hash);
    free(commit2_hash);
    buf = read_file(path, NULL);
    if (buf == NULL) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Cannot read commit2 file: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        return -1;
    }
    Commit* commit2 = commit_init();
    if (commit2 == NULL) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Cannot init commit2: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        free(buf);
        return -1;
    }
    ret_c = commit_parse(buf, commit2);
    if (ret_c != 0) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Cannot parse commit2 file: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        commit_destroy(commit2);
        free(buf);
        return -1;
    }
    free(buf);

    memset(path, 0, PATH_MAX);
    char* commit1_hash = resolve_branch(branchName1);
    if (commit1_hash == NULL) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Cannot get commit1 hash: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        commit_destroy(commit2);
        return -1;
    }
    join_path(path, ".mygit/commits", commit1_hash);
    free(commit1_hash);
    buf = read_file(path, NULL);
    if (buf == NULL) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Cannot read commit1 file: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        commit_destroy(commit2);
        return -1;
    }
    Commit* commit1 = commit_init();
    if (commit1 == NULL) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Cannot init commit1: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        commit_destroy(commit2);
        free(buf);
        return -1;
    }
    ret_c = commit_parse(buf, commit1);
    if (ret_c != 0) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Cannot parse commit1 file: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        commit_destroy(commit2);
        free(buf);
        commit_destroy(commit1);
        return -1;
    }
    free(buf);

    // сравнивать будем файлы через хеш таблицу
    // если файл изменился -> выводим измененные строки
    ret_c = hashmap_init();
    if (ret_c != 0) {
        err = errno;
        fprintf(stderr, ANSI_COLOR_RED "Cannot init hashmap: %s\n" ANSI_COLOR_RESET, strmyerr(err));
        commit_destroy(commit2);
        commit_destroy(commit1);
        return -1;
    }
    size_t commit1_vec_len = vector_get_size(&commit1->files);
    for (size_t i = 0; i < commit1_vec_len; ++i) {
        FileEntry* entry = (FileEntry*)vector_get(&commit1->files, i);
        ret_c = hashmap_insert_entry(entry);
        if (ret_c != 0) {
            err = errno;
            fprintf(stderr, ANSI_COLOR_RED "Cannot insert entry to hashmap: %s\n" ANSI_COLOR_RESET, strmyerr(err));
            commit_destroy(commit2);
            commit_destroy(commit1);
            hashmap_destroy();
            return -1;
        }
    }

    size_t commit2_vec_len = vector_get_size(&commit2->files);
    for (size_t i = 0; i < commit2_vec_len; ++i) {
        FileEntry* entry = (FileEntry*)vector_get(&commit2->files, i);

        FileEntry* in_map_entry = hashmap_get_entry(entry->name);
        if (in_map_entry == NULL && errno == MY_ERR_HASHMAP_NOT_FOUND) {
            if (strcmp(entry->type, ENTRY_TYPE_BLOB) == 0) {
                printf(ANSI_COLOR_YELLOW "Added" ANSI_COLOR_RESET " file '%s' with hash: %s\n", entry->name, entry->hash);
            }
            else if (strcmp(entry->type, ENTRY_TYPE_DEL) == 0) {
                printf(ANSI_COLOR_YELLOW "Deleted" ANSI_COLOR_RESET " file '%s'\n", entry->name);
            }
            else if (strcmp(entry->type, ENTRY_TYPE_REF) == 0) {
                printf("File '%s' " ANSI_COLOR_YELLOW "added" ANSI_COLOR_RESET " in other commit. File hash: %s\n", entry->name, entry->hash);
            }
            else {
                fprintf(stderr, ANSI_COLOR_RED "Bad file type in file '%s' with type: %s\n" ANSI_COLOR_RESET, entry->name, entry->type);
            }
        }
        else {
            if (in_map_entry == NULL) {
                err = errno;
                fprintf(stderr, ANSI_COLOR_RED "Cannot get entry from hashmap: %s\n" ANSI_COLOR_RESET, strmyerr(err));
                commit_destroy(commit2);
                commit_destroy(commit1);
                hashmap_destroy();
                return -1;
            }
            if (strcmp(in_map_entry->hash, entry->hash) != 0) {
                if (strcmp(entry->type, ENTRY_TYPE_BLOB) == 0) {
                    if (strcmp(in_map_entry->type, ENTRY_TYPE_BLOB) == 0){
                        printf("File '%s' " ANSI_COLOR_YELLOW "changed" ANSI_COLOR_RESET ". New hash: %s\n", entry->name, entry->hash);
                        int is_bin = is_file_binary_autopath(entry->hash);
                        if (is_bin == false) {
                            ret_c = print_lines_diff(in_map_entry->hash, entry->hash);
                            if (ret_c != 0) {
                                err = errno;
                                fprintf(stderr, ANSI_COLOR_RED "Cannot get difference from files: %s\n" ANSI_COLOR_RESET, strmyerr(err));
                                commit_destroy(commit2);
                                commit_destroy(commit1);
                                hashmap_destroy();
                                return -1;
                            }
                        }
                        else if (is_bin == -1) {
                            err = errno;
                            fprintf(stderr, ANSI_COLOR_RED "Cannot check file for binarity: %s\n" ANSI_COLOR_RESET, strmyerr(err));
                            commit_destroy(commit2); 
                            commit_destroy(commit1);
                            hashmap_destroy();
                            return -1;
                        }
                    }
                    else {
                        printf(ANSI_COLOR_YELLOW "Added" ANSI_COLOR_RESET " file '%s' with hash: %s\n", entry->name, entry->hash);
                        int is_bin = is_file_binary_autopath(entry->hash);
                        if (is_bin == false && strcmp(in_map_entry->type, ENTRY_TYPE_DEL) != 0) {
                            ret_c = print_lines_diff(in_map_entry->hash, entry->hash);
                            if (ret_c != 0) {
                                err = errno;
                                fprintf(stderr, ANSI_COLOR_RED "Cannot get difference from files: %s\n" ANSI_COLOR_RESET, strmyerr(err));
                                commit_destroy(commit2);
                                commit_destroy(commit1);
                                hashmap_destroy();
                                return -1;
                            }
                        }
                        else if (is_bin == -1) {
                            err = errno;
                            fprintf(stderr, ANSI_COLOR_RED "Cannot check file for binarity: %s\n" ANSI_COLOR_RESET, strmyerr(err));
                            commit_destroy(commit2);
                            commit_destroy(commit1);
                            hashmap_destroy();
                            return -1;
                        }
                    }  
                }
                else if (strcmp(entry->type, ENTRY_TYPE_DEL) == 0) {
                    printf(ANSI_COLOR_YELLOW "Deleted" ANSI_COLOR_RESET " file '%s'\n", entry->name);
                }
                else if (strcmp(entry->type, ENTRY_TYPE_REF) == 0) {
                    printf("File '%s' " ANSI_COLOR_YELLOW "changed" ANSI_COLOR_RESET " in other commit. File hash: %s\n", entry->name, entry->hash);
                    int is_bin = is_file_binary_autopath(entry->hash);
                    if (is_bin == false && strcmp(in_map_entry->type, ENTRY_TYPE_DEL) != 0) {
                        ret_c = print_lines_diff(in_map_entry->hash, entry->hash);
                        if (ret_c != 0) {
                            err = errno;
                            fprintf(stderr, ANSI_COLOR_RED "Cannot get difference from files: %s\n" ANSI_COLOR_RESET, strmyerr(err));
                            commit_destroy(commit2);
                            commit_destroy(commit1);
                            hashmap_destroy();
                            return -1;
                        }
                    }
                    else if (is_bin == -1) {
                        err = errno;
                            fprintf(stderr, ANSI_COLOR_RED "Cannot check file for binarity: %s\n" ANSI_COLOR_RESET, strmyerr(err));
                            commit_destroy(commit2);
                            commit_destroy(commit1);
                            hashmap_destroy();
                        return -1;
                    }
                }
                else {
                    fprintf(stderr, ANSI_COLOR_RED "Bad file type in file '%s' with type: %s\n" ANSI_COLOR_RESET, entry->name, entry->type);
                }
            }
        }
    }

    hashmap_destroy();
    commit_destroy(commit1);
    commit_destroy(commit2);

    return 0;
}


