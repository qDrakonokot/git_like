#include "ErrorsDefenition/errors.h"
#include "index.h"
#include "fileEntry.h"
#include "DataStructures/vector.h"
#include "Utils/hashFunc.h"
#include "Utils/workWithFiles.h"
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/stat.h>

// читает индекс из файла индекса. Возвращет вектор на куче.
// То есть требует как уничтожение внутренностей вектора так и очистку самого вектора с кучи
Vector* index_read() {
    char* index_data = read_file(".mygit/index", NULL);
    if (index_data == NULL) {
        Vector* null_vec = (Vector*)malloc(sizeof(Vector));
        if (null_vec == NULL) {
            int errno_s = errno;
            errno = errno_s;
            return NULL;
        }
        int ret_c = vector_init(null_vec, sizeof(IndexEntry));
        if (ret_c != 0) {
            int errno_s = errno;
            vector_destroy(null_vec);
            free(null_vec);
            errno = errno_s;
            return NULL;
        }
        return null_vec;
    }

    Vector* entries = (Vector*)malloc(sizeof(Vector));
    if (entries == NULL) {
        int errno_s = errno;
        free(index_data);
        errno = errno_s;
        return NULL;
    }
    int ret_c = vector_init(entries, sizeof(IndexEntry));
    if (ret_c != 0) {
        int errno_s = errno;
        vector_destroy(entries);
        free(entries);
        free(index_data);
        errno = errno_s;
        return NULL;
    }
    size_t index_data_len = strlen(index_data) + 1;
    char* index_data_for_tokenize = malloc(sizeof(char) * index_data_len);
    if (index_data_for_tokenize == NULL) {
        int errno_s = errno;
        free(index_data);
        vector_destroy(entries);
        free(entries);
        errno = errno_s;
        return NULL;
    }
    strcpy(index_data_for_tokenize, index_data);
    index_data_for_tokenize[index_data_len - 1] = '\0';

    char* line = strtok(index_data_for_tokenize, "\n\r");
    while (line != NULL) {

        // проверка на наличие пробела в строке, если их нет -> строка недействительна
        int space_cnt = 0;
        int line_len = strlen(line);
        for (int i = 0; i < line_len; ++i) if (line[i] == ' ') space_cnt++;
        if (space_cnt == 0) {
            free(index_data);
            vector_destroy(entries);
            free(entries);
            free(index_data_for_tokenize);
            SET_ERR(MY_ERR_BAD_FILE_FORMAT);
            return NULL;
        }

        // пропуск пробелов и пустых строк
        int skip_spaces = 0;
        while (isspace(line[skip_spaces])) skip_spaces++;
        if (line[skip_spaces] == '\0') {
            line = strtok(NULL, "\n\r");
            continue;
        }

        // заменяем пробелы-разделители на терминаторы
        // идем с начала для отсечения типа
        for (int i = skip_spaces; i < line_len; ++i) {
            if (line[i] == ' ') {
                line[i] = '\0';
                break;
            }
        }
        // идем с конца для отсецения хеша
        for (int i = line_len; i >= 0; --i) {
            if (line[i] == ' ') {
                line[i] = '\0';
                break;
            }
        }

        // создаем буфферы
        char indexName[FILE_NAME_MAX];
        char indexType[FILE_TYPE_MAX];
        char indexHash[HASH_LEN];

        // выделяем тип
        int type_ptr = skip_spaces;
        while (line[type_ptr] != '\0') type_ptr++;
        strncpy(indexType, line + skip_spaces, FILE_TYPE_MAX);
        indexType[FILE_TYPE_MAX - 1] = '\0';
        // если не тот тип -> ошибка формата
        if (strcmp(indexType, INDEX_TYPE_DEL) != 0 && strcmp(indexType, INDEX_TYPE_ADD) != 0) {
            free(index_data);
            vector_destroy(entries);
            free(entries);
            free(index_data_for_tokenize);
            SET_ERR(MY_ERR_BAD_FILE_FORMAT);
            return NULL;
        }

        // выделяем имя
        int name_ptr = type_ptr + 1;
        while (line[name_ptr] != '\0') name_ptr++;
        strncpy(indexName, line + type_ptr + 1, FILE_NAME_MAX);
        indexName[FILE_NAME_MAX - 1] = '\0';

        // выделяем хеш
        if (strcmp(indexType, INDEX_TYPE_DEL) != 0) {
            int hash_ptr = name_ptr + 1;
            for (int i = 0; i < HASH_LEN - 1; ++i) 
                indexHash[i] = line[hash_ptr + i];
            indexHash[HASH_LEN - 1] = '\0';
        }
        else {
            memset(indexHash, '0', HASH_LEN - 1);
            indexHash[HASH_LEN - 1] = '\0';
        }

        // создаем запись индекса
        IndexEntry entry;
        strcpy(entry.name, indexName);
        strcpy(entry.type, indexType);
        strcpy(entry.hash, indexHash);
        int ret_c = vector_push(entries, &entry);
        if (ret_c != 0) {
            int errno_s = errno;
            free(index_data);
            vector_destroy(entries);
            free(entries);
            free(index_data_for_tokenize);
            errno = errno_s;
            return NULL;
        }

        // к след строке
        line = strtok(NULL, "\n\r");
    }

    free(index_data);
    free(index_data_for_tokenize);

    return entries;
}

// пишет индекс в файл
int index_write(const Vector* entries) {
    if (entries == NULL) { SET_ERR(EINVAL); return -1; }

    size_t vec_size = vector_get_size(entries);
    size_t buf_size = (vec_size + 5) * (FILE_NAME_MAX + FILE_TYPE_MAX + HASH_LEN + 10);
    char* buf = (char*)malloc(buf_size);
    if (buf == NULL) return -1;
    memset(buf, 0, buf_size);

    size_t remaining = buf_size;
    size_t offset = 0;
    for (size_t i = 0; i < vec_size; ++i) {
        IndexEntry* entry = (IndexEntry*)vector_get(entries, i);

        int written = snprintf(buf + offset, remaining,
            "%s %s %s\n", entry->type, entry->name, entry->hash
        );
        if (written < 0 || (size_t)written >= remaining) {
            int errno_s = errno;
            free(buf);
            errno = errno_s;
            return -1;
        }
        offset += written;
        remaining -= written;
    }

    int ret_c = write_text_file(".mygit/index", buf);
    int errno_s = errno;
    free(buf);
    if (ret_c != 0) { errno = errno_s; return -1; }
    return 0;

}

// добавляет запись в индекс и создает блоб 
int index_add(const char* filename) {
    if (filename == NULL) { SET_ERR(EINVAL); return -1; }

    // проверка, что файл есть и он не является папкой
    struct stat st;
    if (stat(filename, &st) != 0) { SET_ERR(MY_ERR_FILE_NOT_FOUND); return -1; }
    else if (S_ISDIR(st.st_mode)) { SET_ERR(EISDIR); return -1; }

    size_t file_size;
    char* file_data = read_file(filename, &file_size);
    if (file_data == NULL) return -1;

    char fileHash[HASH_LEN];
    compute_hash(file_data, file_size, fileHash);

    char file_hash_name[HASH_LEN + 15];
    snprintf(file_hash_name, HASH_LEN + 15, ".mygit/files/%s", fileHash);
    int ret_c = write_file(file_hash_name, file_data, file_size);
    int errno_s = errno;
    free(file_data);
    if (ret_c != 0) { errno = errno_s; return -1; }

    Vector* entries = index_read();
    if (entries == NULL) return -1;

    // поиск сущ записи
    int found = 0;
    size_t vec_len = vector_get_size(entries);
    for (size_t i = 0; i < vec_len; ++i) {
        IndexEntry* entry = (IndexEntry*)vector_get(entries, i);

        if (strcmp(entry->name, filename) == 0) {
            strncpy(entry->hash, fileHash, HASH_LEN);
            entry->hash[HASH_LEN - 1] = '\0';
            strncpy(entry->type, INDEX_TYPE_ADD, FILE_TYPE_MAX);
            entry->type[FILE_TYPE_MAX - 1] = '\0';
            found = 1;
            break;
        }
    }

    if (found == 0) {
        IndexEntry entry;
        strncpy(entry.hash, fileHash, HASH_LEN);
        entry.hash[HASH_LEN - 1] = '\0';
        strncpy(entry.type, INDEX_TYPE_ADD, FILE_TYPE_MAX);
        entry.type[FILE_TYPE_MAX - 1] = '\0';
        strncpy(entry.name, filename, FILE_NAME_MAX);
        entry.name[FILE_NAME_MAX - 1] = '\0';
        ret_c = vector_push(entries, &entry);
        if (ret_c != 0) {
            int errno_s = errno;
            vector_destroy(entries);
            free(entries);
            errno = errno_s;
            return -1;
        }
    }  
    
    ret_c = index_write(entries);
    errno_s = errno;
    vector_destroy(entries);
    free(entries);
    if (ret_c != 0) { errno = errno_s; return -1; }

    return 0;
}

// помечает файл удаленным, сам файл никак не трогает
int index_del(const char* filename) {
    if (filename == NULL) { SET_ERR(EINVAL); return -1; }
   
    Vector* entries = index_read();
    if (entries == NULL) return -1;

    // поиск сущ записи
    int found = 0;
    size_t vec_len = vector_get_size(entries);
    for (size_t i = 0; i < vec_len; ++i) {
        IndexEntry* entry = (IndexEntry*)vector_get(entries, i);

        if (strcmp(entry->name, filename) == 0) {
            strncpy(entry->hash, NULL_HASH, HASH_LEN);
            entry->hash[HASH_LEN - 1] = '\0';
            strncpy(entry->type, INDEX_TYPE_DEL, FILE_TYPE_MAX);
            entry->type[FILE_TYPE_MAX - 1] = '\0';
            found = 1;
            break;
        }
    }

    if (found == 0) {
        IndexEntry entry;
        strncpy(entry.hash, NULL_HASH, HASH_LEN);
        entry.hash[HASH_LEN - 1] = '\0';
        strncpy(entry.type, INDEX_TYPE_DEL, FILE_TYPE_MAX);
        entry.type[FILE_TYPE_MAX - 1] = '\0';
        strncpy(entry.name, filename, FILE_NAME_MAX);
        entry.name[FILE_NAME_MAX - 1] = '\0';
        int ret_c = vector_push(entries, &entry);
        if (ret_c != 0) {
            int errno_s = errno;
            vector_destroy(entries);
            free(entries);
            errno = errno_s;
            return -1;
        }
    }  
    
    int ret_c = index_write(entries);
    int errno_s = errno;
    vector_destroy(entries);
    free(entries);
    if (ret_c != 0) { errno = errno_s; return -1; }

    return 0;
}

