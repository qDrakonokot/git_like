#include "ErrorsDefenition/errors.h"
#include "workWithFiles.h"
#include "DataStructures/vector.h"
#include "hashFunc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <dirent.h>
#include <unistd.h>


int mkdir_p (const char* dir_path) {
    if (dir_path == NULL || strlen(dir_path) == 0) { SET_ERR(EINVAL); return -1; }

    struct stat st;
    if (stat(dir_path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) return 0;
        else { SET_ERR(ENOTDIR); return -1; }
    }

    char parent[PATH_MAX];
    const char* last_slash = strrchr(dir_path, '/');
    if (last_slash != NULL && last_slash != dir_path) {
        size_t len = last_slash - dir_path;
        strncpy(parent, dir_path, len);
        parent[len] = '\0';
        if (mkdir_p(parent) != 0) return -1; 
    }

    if (mkdir(dir_path, 0755) != 0) return -1; 

    return 0;
}


int write_file (const char* path, const void* data, size_t size) {
    if (path == NULL || data == NULL) { SET_ERR(EINVAL); return -1; }

    FILE* fp = fopen(path, "wb");
    if (fp == NULL) return -1;

    size_t writeLen = fwrite(data, 1, size, fp);
    if (writeLen != size) {
        int errno_s = errno;
        fclose(fp);
        errno = errno_s;
        return -1;
    }

    fclose(fp);
    return 0;
}


int write_text_file (const char *path, const char *text) {
    return write_file(path, text, strlen(text));
}


char* read_file (const char* path, size_t* out_size) {
    if (path == NULL) { SET_ERR(EINVAL); return NULL; }

    FILE* fp = fopen(path, "rb");
    if (fp == NULL) return NULL;

    if (fseek(fp, 0, SEEK_END) != 0) {
        int errno_s = errno;
        fclose(fp);
        errno = errno_s;
        return NULL;
    }
    long file_size = ftell(fp);
    if (file_size < 0) {
        int errno_s = errno;
        fclose(fp);
        errno = errno_s;
        return NULL;
    }
    rewind(fp);

    char* buf = (char*)malloc(file_size + 1);
    if (buf == NULL) {
        int errno_s = errno;
        fclose(fp);
        errno = errno_s;
        return NULL;
    }

    size_t read_bytes = fread(buf, 1, file_size, fp);
    if (read_bytes != (size_t)file_size) {
        int errno_s = errno;
        free(buf);
        fclose(fp);
        errno = errno_s;
        return NULL;
    }

    buf[file_size] = '\0';
    if (out_size != NULL) *out_size = file_size;

    fclose(fp);
    return buf;
}


void join_path(char *out, const char *base, const char *name) {
    snprintf(out, PATH_MAX, "%s/%s", base, name);
}


int create_and_write_file (const char* path, const void* data, size_t size) {
    if (path == NULL || data == NULL) { SET_ERR(EINVAL); return -1; }
    
    const char* last_slash = strrchr(path, '/');
    char dir[PATH_MAX];
    if (last_slash != NULL && last_slash != path) {
        size_t len = last_slash - path;
        strncpy(dir, path, len);
        dir[len] = '\0';
        if (mkdir_p(dir) != 0) return -1; 
    }

    if (write_file(path, data, size) != 0) return -1;

    return 0;
}

// Работает с вектором типа FileInfo
int get_all_files (const char* path, Vector* vec) {
    if (path == NULL || path[0] == '/') { SET_ERR(EINVAL); return -1; }
    if (strcmp(path, "..") == 0) { SET_ERR(EINVAL); return -1; }

    char* tmp = (char*)malloc(sizeof(char) * (strlen(path) + 1));
    if (tmp == NULL) return -1;
    strcpy(tmp, path);
    char* real_path = tmp;
    if (strlen(tmp) > 2 && tmp[0] == '.' && tmp[1] == '/') {
        real_path = tmp + 2;
    }
    size_t len = strlen(tmp);
    if (tmp[len - 1] == '/') {
        if (tmp[len - 2] == '/') {
            free(tmp);
            SET_ERR(EINVAL);
            return -1;
        }
        tmp[len - 1] = '\0';
    }

    DIR* dir = opendir(real_path);
    if (dir == NULL) { 
        int errno_s = errno;
        free(tmp);
        errno = errno_s;
        return -1;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 || strcmp(entry->d_name, ".mygit") == 0)
            continue;

        char full_path[PATH_MAX];
        if (strcmp(real_path, ".") == 0) {
            strcpy(full_path, entry->d_name);
        }
        else {
            join_path(full_path, real_path, entry->d_name);   
        }
        
        struct stat st;
        if (lstat(full_path, &st) == 0) {
            if (S_ISLNK(st.st_mode)) {
                char target[PATH_MAX];
                ssize_t len = readlink(full_path, target, sizeof(target) - 1);
                if (len < 0) {
                    int errno_s = errno;
                    free(tmp);
                    closedir(dir);
                    errno = errno_s;
                    return -1; 
                }
                target[len] = '\0';
                char hash[HASH_LEN];
                compute_hash(target, (size_t)len, hash);

                FileInfo info;
                strncpy(info.name, full_path, FILE_NAME_MAX);
                info.name[FILE_NAME_MAX - 1] = '\0';
                strcpy(info.hash, hash);

                int ret_c = vector_push(vec, &info);
                if (ret_c != 0) {
                    int errno_s = errno;
                    free(tmp);
                    closedir(dir);
                    errno = errno_s;
                    return -1;
                }

                continue;
            }
            else if (S_ISDIR(st.st_mode)) {
                if (get_all_files(full_path, vec) != 0) {
                    int errno_s = errno;
                    closedir(dir);
                    free(tmp);
                    errno = errno_s;
                    return -1;
                }
                continue;
            }
            size_t buf_len;
            char* buf = read_file(full_path, &buf_len);
            if (buf == NULL) {
                int errno_s = errno;
                free(tmp);
                closedir(dir);
                errno = errno_s;
                return -1;
            }
            char hash[HASH_LEN];
            compute_hash(buf, buf_len, hash);
            free(buf);

            FileInfo info;
            strncpy(info.name, full_path, FILE_NAME_MAX);
            info.name[FILE_NAME_MAX - 1] = '\0';
            strcpy(info.hash, hash);

            int ret_c = vector_push(vec, &info);
            if (ret_c != 0) {
                int errno_s = errno;
                free(tmp);
                closedir(dir);
                errno = errno_s;
                return -1;
            }
        }
        else { 
            int errno_s = errno;
            free(tmp);
            closedir(dir);
            errno = errno_s;
            return -1;
        }
    }
    closedir(dir);
    free(tmp);

    return 0;
}


int check_dir (const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) { SET_ERR(MY_ERR_FILE_NOT_FOUND); return -1; }
    if (S_ISDIR(st.st_mode)) return true;
    if (S_ISREG(st.st_mode)) return false;
    { SET_ERR(MY_ERR_BAD_FILE_FORMAT); return -1; }
}


int remove_all_repofiles (const char* path) {
    if (path == NULL) { SET_ERR(EINVAL); return -1; }

    DIR* dir = opendir(path);
    if (dir == NULL) { 
        return -1;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 || strcmp(entry->d_name, ".mygit") == 0)
            continue;
            
        char full_path[PATH_MAX];
        if (strcmp(path, ".") == 0) {
            strcpy(full_path, entry->d_name);
        }
        else {
            join_path(full_path, path, entry->d_name);   
        }
        
        struct stat st;
        if (lstat(full_path, &st) == 0) {
            if (S_ISLNK(st.st_mode)) {
                int ret_c = remove(full_path);
                if (ret_c != 0) {
                    int errno_s = errno;
                    closedir(dir);
                    errno = errno_s;
                    return -1;
                }
                continue;
            }
            else if (S_ISDIR(st.st_mode)) {
                if (remove_all_repofiles(full_path) != 0) {
                    int errno_s = errno;
                    closedir(dir);
                    errno = errno_s;
                    return -1;
                }
            }
            
            int ret_c = remove(full_path);
            if (ret_c != 0) {
                int errno_s = errno;
                closedir(dir);
                errno = errno_s;
                return -1;
            }

        }
        else { 
            int errno_s = errno;
            closedir(dir);
            errno = errno_s;
            return -1;
        }
        
    }

    closedir(dir);

    return 0;
}

// изменеят буффер; грязная функция; НЕ очищать буфер до очищения массива строк
char** split_buf_by_lines (char* buf, int* out_lines_cnt) {
    if (buf == NULL) { SET_ERR(EINVAL); return NULL; }
    int ret_c = 0;
    const char* delimeters = "\r\n";
    Vector tmp;
    ret_c = vector_init(&tmp, sizeof(char*));
    if (ret_c != 0) return NULL;

    char* line = strtok(buf, delimeters);
    while (line != NULL) {
        vector_push(&tmp, &line);
        line = strtok(NULL, delimeters);
    }

    size_t lines_cnt = vector_get_size(&tmp);
    char** lines = (char**)malloc(lines_cnt * sizeof(char*));
    if (lines == NULL) {
        int errno_s = errno;
        vector_destroy(&tmp);
        errno = errno_s;
        return NULL;
    }
    for (size_t i = 0; i < lines_cnt; ++i) lines[i] = *(char**)vector_get(&tmp, i);
    if (out_lines_cnt != NULL) *out_lines_cnt = lines_cnt;
    vector_destroy(&tmp);

    return lines;
}


int is_file_binary (const char* filepath) {
    FILE* file = fopen(filepath, "rb");
    if (file == NULL) return -1;

    unsigned char buffer[1024]; // читаем килобайт
    size_t bytes_read = fread(buffer, 1, 1024, file);
    fclose(file);

    if (bytes_read == 0) return false;

    for (size_t i = 0; i < bytes_read; ++i) {
        unsigned char ch = buffer[i];
        if (ch == '\0') return true;
        if (ch < 32 && ch != '\t' && ch != '\n' && ch != '\r') return true;
    }

    return false;
}

