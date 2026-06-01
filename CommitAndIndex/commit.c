#include "ErrorsDefenition/errors.h"
#include "commit.h"
#include "DataStructures/vector.h"
#include "fileEntry.h"
#include "Utils/hashFunc.h"
#include "Utils/workWithFiles.h"
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

// инициализирует пустой коммит
Commit* commit_init(void) {
    Commit* commit = (Commit*)malloc(sizeof(Commit));
    if (commit == NULL) return NULL;
    memset(commit->parent, '0', HASH_LEN);
    commit->parent[HASH_LEN - 1] = '\0';
    memset(commit->own_hash, '0', HASH_LEN);
    commit->own_hash[HASH_LEN - 1] = '\0';
    commit->timestamp = 0;
    commit->message = NULL;
    int ret_c = vector_init(&commit->files, sizeof(FileEntry));
    if (ret_c != 0) {
        int errno_s = errno;
        commit_destroy(commit);
        errno = errno_s;
        return NULL;
    }
    return commit;
}

// выставляет msg в поле сообщения коммита
int commit_set_message(Commit* commit, const char* msg) {
    if (commit == NULL || msg == NULL) { SET_ERR(EINVAL); return -1; }
    free(commit->message);
    commit->message = strdup(msg);
    if (commit->message == NULL) return -1;
    return 0;
}

// добавляет файл в вектор файлов коммита
int commit_add_file(Commit* commit, const char* name, const char* type, const char* hash) {
    if (commit == NULL || name == NULL || type == NULL) { SET_ERR(EINVAL); return -1; }
    FileEntry tmp;
    fileEntry_init(&tmp);
    fileEntry_set(&tmp, name, type, hash);
    int ret_c = vector_push(&commit->files, &tmp);
    if (ret_c != 0) return -1;
    return 0;
}

// форматирование структуры в текст
static int commit_format(const Commit* commit, char* buf, size_t buf_size) {
    if (commit == NULL || buf == NULL) { SET_ERR(EINVAL); return -1; }

    // форматируем время
    struct tm *local = localtime(&commit->timestamp);
    char time_buf[100];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", local);

    // базовая информация
    int written = snprintf(buf, buf_size,
        "parent: %s\n"
        "time: %s\n"
        "message: %s\n"
        "files:\n",
        commit->parent,
        time_buf,
        commit->message ? commit->message : ""
    );

    if (written < 0 || (size_t)written >= buf_size) return -1;

    // пишем файлы
    size_t offset = written;
    size_t vec_len = vector_get_size(&commit->files);
    for (size_t i = 0; i < vec_len; ++i) {
        FileEntry* fe = (FileEntry*)vector_get(&commit->files, i);

        int res = snprintf(buf + offset, buf_size - offset, "\t%s %s %s\n", fe->name, fe->type, fe->hash);
        
        if (res < 0 || offset + res >= buf_size) return -1;
        offset += res;
    }

    return 0;
}

// парсинг коммита из текста
int commit_parse(const char* text, Commit* commit) {
    if (text == NULL || commit == NULL) { SET_ERR(EINVAL); return -1; }
    
    // создаем необходимые переменные
    Vector files;
    int ret_c = vector_init(&files, sizeof(FileEntry));
    if (ret_c != 0) return -1;
    time_t timestamp = 0;
    char* message = NULL;
    char parent[HASH_LEN];
    memset(parent, '0', HASH_LEN);
    parent[HASH_LEN - 1] = '\0';
    char own_hash[HASH_LEN];
    memset(own_hash, '0', HASH_LEN);
    own_hash[HASH_LEN - 1] = '\0';
    int in_files_section = 0;
    char* tmp_text = strdup(text);
    if (tmp_text == NULL) {
        int errno_s = errno;
        vector_destroy(&files);
        errno = errno_s;
        return -1;
    }

    // парсим строки через перенос строки
    char* line = strtok(tmp_text, "\n\r");
    while (line != NULL) {
        if (in_files_section == 0) {
            // попали в секцию с файлами
            if (strncmp(line, "files:", 6) == 0) {
                in_files_section = 1;
            }
            else if (strncmp(line, "parent: ", 8) == 0) {
                strncpy(parent, line + 8, HASH_LEN - 1);
                parent[HASH_LEN - 1] = '\0';
            }
            else if (strncmp(line, "time: ", 6) == 0) {
                // обратное преобразование времени
                struct tm tm = {0};
                int cnt_args = sscanf(line + 6, "%d-%d-%d %d:%d:%d", 
                                        &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                                        &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
                if (cnt_args != 6) {
                    vector_destroy(&files);
                    free(message);
                    free(tmp_text);
                    SET_ERR(MY_ERR_BAD_FILE_FORMAT);
                    return -1;
                }
                tm.tm_year -= 1900;  
                tm.tm_mon -= 1;       
                tm.tm_isdst = -1;     
                timestamp = mktime(&tm);
            }
            else if (strncmp(line, "message: ", 9) == 0) {
                message = strdup(line + 9);
                if (message == NULL) {
                    int errno_s = errno;
                    vector_destroy(&files);
                    free(message);
                    free(tmp_text);
                    errno = errno_s;
                    return -1;
                }
            }
            else {
                SET_ERR(MY_ERR_BAD_FILE_FORMAT);
                return -1;
            }
        }
        else {
            char entryName[FILE_NAME_MAX];
            char entryType[FILE_TYPE_MAX];
            char entryHash[HASH_LEN];
            int del_type = 0;

            // проверка на наличие пробела в строке, если их нет -> строка недействительна
            int space_cnt = 0;
            int line_len = strlen(line);
            for (int i = 0; i < line_len; ++i) if (line[i] == ' ') space_cnt++;
            if (space_cnt == 0) {
                vector_destroy(&files);
                free(message);
                free(tmp_text);
                SET_ERR(MY_ERR_BAD_FILE_FORMAT);
                return -1;
            }

            // пропуск пробелов и пустых строк
            int skip_spaces = 0;
            while (isspace(line[skip_spaces])) skip_spaces++;
            if (line[skip_spaces] == '\0') {
                line = strtok(NULL, "\n\r");
                continue;
            }

            // проверка не вышли ли мы из секции с файлами
            // после нее сразу идет хеш
            if (strncmp(line + skip_spaces, "hash: ", 6) == 0) {
                strncpy(own_hash, line + skip_spaces + 6, HASH_LEN - 1);
                own_hash[HASH_LEN - 1] = '\0';
                break;
            }

            // заменяем пробелы-разделители на терминаторы
            int space_delimeter_cnt = 0;
            for (int i = line_len; i >= 0; --i) {
                if (space_delimeter_cnt == 2) break;
                if (line[i] == ' ') {
                    line[i] = '\0';
                    space_delimeter_cnt++;
                }
            }

            // записываем имя файла
            int name_ptr = skip_spaces;
            while (line[name_ptr] != '\0') name_ptr++;
            strncpy(entryName, line + skip_spaces, FILE_NAME_MAX);
            entryName[FILE_NAME_MAX - 1] = '\0';

            // записываем тип файла
            int type_ptr = name_ptr + 1;
            while (line[type_ptr] != '\0') type_ptr++;
            if (strcmp(line + name_ptr + 1, ENTRY_TYPE_DEL) == 0) {
                strncpy(entryType, ENTRY_TYPE_DEL, FILE_TYPE_MAX);
                entryType[FILE_TYPE_MAX - 1] = '\0';
                del_type = 1;
            }
            else if (strcmp(line + name_ptr + 1, ENTRY_TYPE_BLOB) == 0) {
                strncpy(entryType, ENTRY_TYPE_BLOB, FILE_TYPE_MAX);
                entryType[FILE_TYPE_MAX - 1] = '\0';
            }
            else if (strcmp(line + name_ptr + 1, ENTRY_TYPE_REF) == 0) {
                strncpy(entryType, ENTRY_TYPE_REF, FILE_TYPE_MAX);
                entryType[FILE_TYPE_MAX - 1] = '\0';
            }
            else {
                vector_destroy(&files);
                free(message);
                free(tmp_text);
                SET_ERR(MY_ERR_BAD_FILE_FORMAT);
                return -1;
            }

            // записываем хеш файла
            if (del_type == 0) {
                int hash_ptr = type_ptr + 1;
                for (int i = 0; i < HASH_LEN - 1; ++i)
                    entryHash[i] = line[hash_ptr + i];
                entryHash[HASH_LEN - 1] = '\0';
            }
            else {
                memset(entryHash, '0', HASH_LEN - 1);
                entryHash[HASH_LEN - 1] = '\0';
            }

            FileEntry entry;
            fileEntry_init(&entry);
            fileEntry_set(&entry, entryName, entryType, entryHash);
            int ret_c = vector_push(&files, &entry);
            if (ret_c != 0) {
                int errno_s = errno;
                vector_destroy(&files);
                free(message);
                free(tmp_text);
                errno = errno_s;
                return -1;
            }
        }

        line = strtok(NULL, "\n\r");
    }

    if (in_files_section == 0) {
        vector_destroy(&files);
        free(message);
        free(tmp_text);
        SET_ERR(MY_ERR_BAD_FILE_FORMAT);
        return -1;
    }

    vector_destroy(&commit->files);
    free(commit->message);

    strncpy(commit->parent, parent, HASH_LEN);
    strncpy(commit->own_hash, own_hash, HASH_LEN);
    commit->message = message;
    commit->timestamp = timestamp;
    commit->files = files;
    
    free(tmp_text);

    return 0;
}

// уничтожение коммита на куче 
void commit_destroy(Commit* commit) {
    if (commit == NULL) { SET_ERR(EINVAL); return; }
    memset(commit->parent, 0, HASH_LEN);
    memset(commit->own_hash, 0, HASH_LEN);
    commit->timestamp = 0;
    free(commit->message);
    commit->message = NULL;
    vector_destroy(&commit->files);
    free(commit);
}

// запись структуры коммита в текстовый файл 
int commit_write_file(Commit* commit, char* out_hash, const char* branchName) {
    if (commit == NULL) { SET_ERR(EINVAL); return -1; }
    int ret_c = 0; 

    // форматируем коммит и вычисляем его хеш
    char buf[BUFFER_MAX_LEN];
    memset(buf, 0, BUFFER_MAX_LEN);
    ret_c = commit_format(commit, buf, BUFFER_MAX_LEN);
    if (ret_c != 0) return -1;
    char hash[HASH_LEN];
    compute_hash(buf, strlen(buf), hash);
    if (out_hash != NULL) strcpy(out_hash, hash);

    // Дописываем cтроку собственного хеша
    size_t offset = strlen(buf);
    int written = snprintf(buf + offset, BUFFER_MAX_LEN, "hash: %s\n", hash);
    if (written < 0 || (size_t)written >= BUFFER_MAX_LEN) return -1;

    // сохраняем файл коммита
    char path[PATH_MAX];
    join_path(path, ".mygit/commits", hash);
    ret_c = write_text_file(path, buf);
    if (ret_c != 0) return -1;

    if (branchName == NULL) {
        // выставляем указатель в голове на начальный коммит
        ret_c = write_text_file(".mygit/HEAD", hash);
        if (ret_c != 0) return -1;
    }
    else {
        // тоже выставляем указатель в голове на начальный коммит, но уже с веткой
        memset(path, 0, PATH_MAX);
        join_path(path, ".mygit/branches", branchName);
        ret_c = write_text_file(path, hash);
        if (ret_c != 0) return -1;

        char reference[FILE_NAME_MAX];
        snprintf(reference, PATH_MAX, "ref: %s", branchName);
        ret_c = write_text_file(".mygit/HEAD", reference);
        if (ret_c != 0) return -1;
    }

    return 0;
}
