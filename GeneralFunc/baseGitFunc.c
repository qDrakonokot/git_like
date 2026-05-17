#include "baseGitFunc.h"
#include "ErrorsDefenition/errors.h"
#include "CommitAndIndex/commit.h"
#include "CommitAndIndex/index.h"
#include "CommitAndIndex/fileEntry.h"
#include "Utils/hashFunc.h"
#include "Utils/workWithFiles.h"
#include "DataStructures/vector.h"
#include "DataStructures/hashmap.h"
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

// проверка существует ли папка .mygit
static int check_repo () {
    struct stat st;
    if (stat(".mygit", &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Current directory not a repository: .mygit not exists\n");
        return -1;
    }
    return 0;
}

// инициализация репозитория
int cmd_init () {
    int ret_c = 0, err = 0;
    struct stat st;
    // проверяем существование папки гита
    if (stat(".mygit", &st) == 0) {
        if (S_ISDIR(st.st_mode)) { 
            fprintf(stderr, "Repository already exists!\n");
            return -1;
        }
        else { 
            fprintf(stderr, "Repository already exists, but not a directory!\n");
            return -1; 
        }
    }
    
    // создаём папку и вложенные папки
    ret_c = mkdir_p(".mygit/commits");
    if (ret_c != 0) {
        err = errno;
        fprintf(stderr, "Error while creating commits dir: %s\n", strmyerr(err));
        return -1;
    }
    ret_c = mkdir_p(".mygit/files");
    if (ret_c != 0) {
        err = errno;
        fprintf(stderr, "Error while creating files dir: %s\n", strmyerr(err));
        return -1;
    }

    // создаем пустой индекс
    ret_c = write_text_file(".mygit/index", "");
    if (ret_c != 0) {
        err = errno;
        fprintf(stderr, "Error while writting in index: %s\n", strmyerr(err));
        return -1;
    }

    // создаем начальный коммит
    Commit* commit = commit_init();
    if (commit == NULL) {
        err = errno;
        fprintf(stderr, "Error while creating commit file: %s\n", strmyerr(err));
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
        fprintf(stderr, "Error while writting message in initial commit: %s\n", strmyerr(err));
        commit_destroy(commit);
        return -1;
    }

    // пишем коммит в файл 
    ret_c = commit_write_file(commit, NULL, NULL);
    if (ret_c != 0) {
        err = errno;
        fprintf(stderr, "Error while creating commit file: %s\n", strmyerr(err));
        commit_destroy(commit);
        return -1;
    }

    commit_destroy(commit);
    printf("Initialazed empty repository in .mygit\n");
    return 0;
}

// добавить файл в индекс
int cmd_add (const char* filename) {
    if (filename == NULL) {
        fprintf(stderr, "Argument invalid\n");
        return -1;
    }
    int ret_c = 0, err = 0;

    if (check_repo() != 0) return -1;

    ret_c = index_add(filename);
    if (ret_c != 0) {
        err = errno;
        fprintf(stderr, "Cannot add file: '%s': %s\n", filename, strmyerr(err));
        return -1;
    }

    printf("Added file '%s' to index\n", filename);
    return 0;
}

// пометить файл удаленным в индексе
int cmd_remove (const char* filename) {
    if (filename == NULL) {
        fprintf(stderr, "Argument invalid\n");
        return -1;
    }

    int ret_c = 0, err = 0;

    if (check_repo() != 0) return -1;

    ret_c = index_del(filename);
    if (ret_c != 0) {
        err = errno;
        fprintf(stderr, "Cannot remove file: '%s': %s\n", filename, strmyerr(err));
        return -1;
    }

    printf("File '%s' marked as removed\n", filename);
    return 0;
}

// создать коммит и сохранить коммит
int cmd_commit (const char* message) {

    if (message == NULL) {
        fprintf(stderr, "Empty message\n");
        return -1;
    }

    int ret_c = 0, err = 0;

    if (check_repo() != 0) return -1;

    // считывыем хеш родителя
    char* parent_hash = read_file(".mygit/HEAD", NULL);
    if (parent_hash == NULL) {
        err = errno;
        fprintf(stderr, "Error with HEAD file: %s\n", strmyerr(err));
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
        fprintf(stderr, "Cannot read parent commit: %s\n", strmyerr(err));
        free(parent_hash);
        return -1;
    }
    
    // создаем и заполняем структуру коммита
    Commit* parent_commit = commit_init();
    if (parent_commit == NULL) {
        err = errno;
        fprintf(stderr, "Cannot create parent commit image in memory: %s\n", strmyerr(err));
        free(parent_hash);
        free(buf);
        return -1;
    }
    ret_c = commit_parse(buf, parent_commit);
    if (ret_c != 0) {
        err = errno;
        fprintf(stderr, "Cannot parse parrent commit: %s\n", strmyerr(err));
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
        fprintf(stderr, "Cannot read index file: %s\n", strmyerr(err));
        free(parent_hash);
        commit_destroy(parent_commit);
        return -1;
    }
    size_t index_size = vector_get_size(index);
    if (index_size == 0) {
        fprintf(stderr, "Nothing to commit\n");
        free(parent_hash);
        commit_destroy(parent_commit);
        vector_destroy(index);
        free(index);
        return -1;
    }

    // содаем и заполняем новый коммит
    Commit* new_commit = commit_init();
    if (new_commit == NULL) {
        err = errno;
        fprintf(stderr, "Cannot create new commit image in memory: %s\n", strmyerr(err));
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
        fprintf(stderr, "Cannot set message in commit: %s\n", strmyerr(err));
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
                fprintf(stderr, "Сommit recording error: %s\n", strmyerr(err));
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
                fprintf(stderr, "Сommit recording error: %s\n", strmyerr(err));
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
                    fprintf(stderr, "Сommit recording error: %s\n", strmyerr(err));
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
                    fprintf(stderr, "Сommit recording error: %s\n", strmyerr(err));
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
    ret_c = commit_write_file(new_commit, new_hash, NULL);
    if (ret_c != 0) {
        err = errno;
        fprintf(stderr, "Cannot write new commit file: %s\n", strmyerr(err));
        free(parent_hash);
        commit_destroy(parent_commit);
        vector_destroy(index);
        free(index);
        commit_destroy(new_commit);
        return -1;
    }

    Vector new_index;
    ret_c = vector_init(&new_index, sizeof(IndexEntry));
    if (ret_c != 0) {
        err = errno;
        fprintf(stderr, "Cannot create index vector: %s\n", strmyerr(err));
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
        fprintf(stderr, "Cannot write index file: %s\n", strmyerr(err));
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

// вывод списка файлов из индекса и их состояние
int cmd_status () {
    int ret_c = 0, err = 0;

    if (check_repo () != 0) return -1;

    // читаем и создаем последний коммит
    Commit* last_commit = commit_init();
    if (last_commit == NULL) {
        err = errno;
        fprintf(stderr, "Cannot read last commit: %s\n", strmyerr(err));
        return -1;
    }
    char* last_commit_hash = read_file(".mygit/HEAD", NULL);
    if (last_commit_hash == NULL) {
        err = errno;
        fprintf(stderr, "Error with HEAD file: %s\n", strmyerr(err));
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
        fprintf(stderr, "Cannot read last commit: %s\n", strmyerr(err));
        free(last_commit_hash);
        commit_destroy(last_commit);
        return -1;
    }
    ret_c = commit_parse(buf, last_commit);
    if (ret_c != 0) {
        err = errno;
        fprintf(stderr, "Cannot read last commit: %s\n", strmyerr(err));
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
        fprintf(stderr, "Cannot initialize hashmap: %s\n", strmyerr(err));
        commit_destroy(last_commit);
        return -1;
    } 
    size_t last_commit_file_vec_len = vector_get_size(&last_commit->files);
    for (size_t i = 0; i < last_commit_file_vec_len; ++i) {
        FileEntry* file = (FileEntry*)vector_get(&last_commit->files, i);
        ret_c = hashmap_insert_entry(file);
        if (ret_c != 0) {
            err = errno;
            fprintf(stderr, "Cannot create hashmap with commit files: %s\n", strmyerr(err));
            hashmap_destroy();
            commit_destroy(last_commit);
            return -1;
        } 
    }

    // читаем индекс, его и будем сравнивать
    Vector* idx = index_read();
    if (idx == NULL) {
        err = errno;
        fprintf(stderr, "Cannot read index: %s\n", strmyerr(err));
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
                        fprintf(stderr, "Error with commit hashmap: %s\n", strmyerr(err));
                        commit_destroy(last_commit);
                        vector_destroy(idx);
                        hashmap_destroy();
                        free(idx);
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
    vector_destroy(idx);
    free(idx);

    return 0;
}

// выводит цепочку коммитов c прикольным форматированием 
int cmd_log (const char* start_commit_hash, const int* commit_cnt) {
    if (commit_cnt != NULL && *commit_cnt < 0) {
        fprintf(stderr, "Invalid count of commit.\n");
        return -1;
    }
    
    int ret_c = 0, err = 0;

    if (check_repo() != 0) return -1;

    char* hash;
    if (start_commit_hash == NULL) {
        hash = read_file(".mygit/HEAD", NULL);
        if (hash == NULL) {
            err = errno;
            fprintf(stderr, "Cannot read HEAD file: %s\n", strmyerr(err));
            return -1;
        }
        hash[HASH_LEN - 1] = '\0';
    }
    else {
        hash = (char*)malloc(sizeof(char) * HASH_LEN);
        if (hash == NULL) {
            err = errno;
            fprintf(stderr, "Allocator error: %s\n", strmyerr(err));
            return -1;
        }
        strncpy(hash, start_commit_hash, HASH_LEN);
        hash[HASH_LEN - 1] = '\0';
    }

    int cnt = 0;
    int total_cnt = (commit_cnt == NULL) ? INT32_MAX : *commit_cnt;
    while (strcmp(hash, NULL_HASH) != 0 && cnt < total_cnt) {
        char path[PATH_MAX];
        join_path(path, ".mygit/commits", hash);
        Commit* commit = commit_init();
        if (commit == NULL) {
            err = errno;
            fprintf(stderr, "Cannot read HEAD file: %s\n", strmyerr(err));
            free(hash);
            return -1;
        }
        char* buf = read_file(path, NULL);
        if (buf == NULL) {
            err = errno;
            fprintf(stderr, "Cannot read HEAD file: %s\n", strmyerr(err));
            free(hash);
            commit_destroy(commit);
            return -1;
        }
        ret_c = commit_parse(buf, commit);
        if (ret_c != 0) {
            err = errno;
            fprintf(stderr, "Cannot read HEAD file: %s\n", strmyerr(err));
            free(hash);
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

    free(hash);

    return 0;
}

// выводит список измененных файлов
int cmd_diff (const char* target_commit_hash) {
    if (target_commit_hash == NULL) { fprintf(stderr, "Argument invalid\n"); return -1; }
    int ret_c = 0, err = 0;

    if (check_repo () != 0) return -1;

    // читаем текущий коммит из хеда
    char* current_commit_hash = read_file(".mygit/HEAD", NULL);
    if (current_commit_hash == NULL) {
        err = errno;
        fprintf(stderr, "Cannot read HEAD file: %s\n", strmyerr(err));
        return -1;
    }
    current_commit_hash[HASH_LEN - 1] = '\0';
    char current_commit_path[PATH_MAX];
    join_path(current_commit_path, ".mygit/commits", current_commit_hash);
    char* buf = read_file(current_commit_path, NULL);
    if (buf == NULL) {
        err = errno;
        fprintf(stderr, "Cannot read current commit: %s\n", strmyerr(err));
        free(current_commit_hash);
        return -1;
    }

    Commit* current_commit = commit_init();
    if (current_commit == NULL) {
        err = errno;
        fprintf(stderr, "Cannot init current commit: %s\n", strmyerr(err));
        free(current_commit_hash);
        free(buf);
        return -1;
    }
    ret_c = commit_parse(buf, current_commit);
    if (ret_c != 0) {
        err = errno;
        fprintf(stderr, "Cannot parse current commit: %s\n", strmyerr(err));
        free(current_commit_hash);
        free(buf);
        commit_destroy(current_commit);
        return -1;
    }
    free(buf);
    free(current_commit_hash);

    // читаем целевой коммит
    char target_commit_path[PATH_MAX];
    join_path(target_commit_path, ".mygit/commits", target_commit_hash);
    buf = read_file(target_commit_path, NULL);
    if (buf == NULL) {
        err = errno;
        fprintf(stderr, "Cannot read current commit: %s\n", strmyerr(err));
        commit_destroy(current_commit);
        return -1;
    }

    Commit* target_commit = commit_init();
    if (target_commit == NULL) {
        err = errno;
        fprintf(stderr, "Cannot init target commit: %s\n", strmyerr(err));
        commit_destroy(current_commit);
        free(buf);
        return -1;
    }
    ret_c = commit_parse(buf, target_commit);
    if (ret_c != 0) {
        err = errno;
        fprintf(stderr, "Cannot parse target commit: %s\n", strmyerr(err));
        commit_destroy(current_commit);
        free(buf);
        commit_destroy(target_commit);
        return -1;
    }
    free(buf);

    // создаем и заполняем хеш-карту с файлами целевого коммита
    ret_c = hashmap_init();
    if (ret_c != 0) {
        err = errno;
        fprintf(stderr, "Cannot init hashmap: %s\n", strmyerr(err));
        commit_destroy(current_commit);
        commit_destroy(target_commit);
        return -1;
    }
    size_t target_commit_vec_size = vector_get_size(&target_commit->files);
    for (size_t i = 0; i < target_commit_vec_size; ++i) {
        FileEntry* entry = (FileEntry*)vector_get(&target_commit->files, i);
        ret_c = hashmap_insert_entry(entry);
        if (ret_c != 0) {
            err = errno;
            fprintf(stderr, "Cannot insert to hashmap: %s\n", strmyerr(err));
            commit_destroy(current_commit);
            commit_destroy(target_commit);
            hashmap_destroy();
            return -1;
        }
    }

    // для каждого файла текущего коммита проверяем его в хеш-карте
    size_t current_commit_vec_sise = vector_get_size(&current_commit->files);
    for (size_t i = 0; i < current_commit_vec_sise; ++i) {
        FileEntry* entry = (FileEntry*)vector_get(&current_commit->files, i);

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
                fprintf(stderr, "Bad file type in file '%s' with type: %s\n", entry->name, entry->type);
            }
        }
        else {
            if (in_map_entry == NULL) {
                err = errno;
                fprintf(stderr, "Error with commit hashmap: %s\n", strmyerr(err));
                commit_destroy(current_commit);
                commit_destroy(target_commit);
                hashmap_destroy();
                return -1;
            }
            if (strcmp(in_map_entry->hash, entry->hash) != 0) {
                if (strcmp(entry->type, ENTRY_TYPE_BLOB) == 0) {
                    if (strcmp(in_map_entry->type, ENTRY_TYPE_BLOB) == 0){
                        printf("File '%s' " ANSI_COLOR_YELLOW "changed" ANSI_COLOR_RESET ". New hash: %s\n", entry->name, entry->hash);
                    }
                    else {
                        printf(ANSI_COLOR_YELLOW "Added" ANSI_COLOR_RESET " file '%s' with hash: %s\n", entry->name, entry->hash);
                    }  
                }
                else if (strcmp(entry->type, ENTRY_TYPE_DEL) == 0) {
                    printf(ANSI_COLOR_YELLOW "Deleted" ANSI_COLOR_RESET " file '%s'\n", entry->name);
                }
                else if (strcmp(entry->type, ENTRY_TYPE_REF) == 0) {
                    printf("File '%s' " ANSI_COLOR_YELLOW "changed" ANSI_COLOR_RESET " in other commit. File hash: %s\n", entry->name, entry->hash);
                }
                else {
                    fprintf(stderr, "Bad file type in file '%s' with type: %s\n", entry->name, entry->type);
                }
            }
        }
    }

    hashmap_destroy();
    commit_destroy(current_commit);
    commit_destroy(target_commit);

    return 0;
}

// восстановление файла по комииту
int cmd_checkout (const char* commit_hash, const char* filename) {
    if (commit_hash == NULL || filename == NULL) { fprintf(stderr, "Argument invalid\n"); return -1; }
    int ret_c = 0, err = 0;

    if (check_repo() != 0) return -1;

    // получаем файл по хешу
    char path[PATH_MAX];
    join_path(path, ".mygit/commits", commit_hash);
    char* buf = read_file(path, NULL);
    if (buf == NULL) {
        err = errno;
        fprintf(stderr, "Cannot commit file: %s\n", strmyerr(err));
        return -1;
    }
    Commit* commit = commit_init();
    if (commit == NULL) {
        err = errno;
        fprintf(stderr, "Cannot init commit: %s\n", strmyerr(err));
        free(buf);
        return -1;
    }
    ret_c = commit_parse(buf, commit);
    if (ret_c != 0) {
        err = errno;
        fprintf(stderr, "Cannot parse commit: %s\n", strmyerr(err));
        free(buf);
        commit_destroy(commit);
        return -1;
    }
    free(buf);

    char* hash = NULL;
    size_t commit_vec_size = vector_get_size(&commit->files);
    for (size_t i = 0; i < commit_vec_size; ++i) {
        FileEntry* entry = (FileEntry*)vector_get(&commit->files, i);

        if (strcmp(entry->name, filename) == 0) {
            hash = entry->hash;
            break;
        }
    }
    
    if (hash == NULL) {
        fprintf (stderr, "File not found in commit\n");
        commit_destroy(commit);
        return -1;
    }
    
    // читаем файл и записываем восстанавливаем в репозитории
    join_path(path, ".mygit/files", hash);
    size_t write_size;
    buf = read_file(path, &write_size);
    if (buf == NULL) {
        err = errno;
        fprintf(stderr, "Cannot read input file: %s\n", strmyerr(err));
        commit_destroy(commit);
        return -1;
    }
    ret_c = create_and_write_file(filename, buf, write_size);
    if (ret_c != 0) {
        err = errno;
        fprintf(stderr, "Cannot replace input file: %s\n", strmyerr(err));
        free(buf);
        commit_destroy(commit);
        return -1;
    }
    free(buf);
    commit_destroy(commit);
    printf("File '%s' succesfully recovered!\n", filename);

    return 0;
}

