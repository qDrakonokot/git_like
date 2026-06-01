#include "branches.h"
#include "workWithFiles.h"
#include "ErrorsDefenition/errors.h"
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#define lstat stat
#endif


int check_detached (void) {
    char* buf = read_file(".mygit/HEAD", NULL);
    if (buf == NULL) return -1;

    buf[5] = '\0';
    if (strcmp(buf, "ref: ") == 0) {
        free(buf);
        return false;
    }
    else {
        free(buf);
        return true;
    }

    free(buf);
    return -1;
}

// записывает в буффер имя текущей ветки
// возвращает 1, если в состояние отсоединенной головы
int get_head_branch_name (char* out_buf) {
    int ret_c = check_detached();
    if (ret_c == true) {
        return 1;
    }
    else if (ret_c == false) {
        char* buf = read_file(".mygit/HEAD", NULL);
        if (buf == NULL) return -1;
        strcpy(out_buf, buf + 5);
        free(buf);
        return 0;
    }
    else {
        SET_ERR(MY_ERR_BAD_FILE_FORMAT);
        return -1;
    }

    return -1;
}


int create_branch (const char* branchName) {
    if (branchName == NULL) { SET_ERR(EINVAL); return -1; }

    char path[PATH_MAX];
    join_path(path, ".mygit/branches", branchName);
    struct stat st;
    if (stat(path, &st) == 0) { SET_ERR(EEXIST); return -1; }
    char* actual_commit_hash = get_head_commit_hash();
    if (actual_commit_hash == NULL) return -1;
    int ret_c = write_text_file(path, actual_commit_hash);
    int errno_s = errno;
    free(actual_commit_hash);
    if (ret_c != 0) {
        errno = errno_s;
        return -1;
    }

    return 0;
}

// вызывающий получает память во владение
// либо прочитанный хеш ветки, либо копию хеша на куче
char* resolve_branch (const char* branchName) {
    if (branchName == NULL) { SET_ERR(EINVAL); return NULL; }

    size_t len = strlen(branchName);
    char* tmp = (char*)malloc(sizeof(char) * (len + 1));
    if (tmp == NULL) return NULL;
    memcpy(tmp, branchName, len);
    if (tmp[len - 1] == '\n') {
        tmp[len - 1] = '\0';
    }
    tmp[len] = '\0';

    char path[PATH_MAX];
    join_path(path, ".mygit/branches", tmp);

    struct stat st;
    if (lstat(path, &st) == 0) {
        free(tmp);
        if (S_ISDIR(st.st_mode)) {
            SET_ERR(MY_ERR_BAD_FILE_FORMAT);
            return NULL; 
        }
#ifndef _WIN32
        else if (S_ISLNK(st.st_mode)) {
            SET_ERR(MY_ERR_BAD_FILE_FORMAT);
            return NULL;
        }
#endif
        char* buf = read_file(path, NULL);
        if (buf == NULL) return NULL;
        return buf;
    }
    else {
        if (len < 40){
            free(tmp);
            SET_ERR(MY_ERR_BRANCH_DOES_NOT_EXIST);
            return NULL;
        }
        char* detached_hash = (char*)malloc(sizeof(char) * HASH_LEN);
        if (detached_hash == NULL) {
            int errno_s = errno;
            free(tmp);
            errno = errno_s;
            return NULL;
        }
        memcpy(detached_hash, tmp, HASH_LEN);
        free(tmp);
        return detached_hash;
    }

    return NULL;
}

// вызывающий получает память во владение
// хеш актуального коммита
char* get_head_commit_hash (void) {

    char* buf = read_file(".mygit/HEAD", NULL);
    if (buf == NULL) return NULL;

    char* head_data;
    if (check_detached() == false) {
        head_data = buf + 5;
    }
    else if (check_detached() == true) {
        head_data = buf;
    }
    else {
        free(buf);
        SET_ERR(MY_ERR_BAD_FILE_FORMAT);
        return NULL;
    }

    char* commit_hash = resolve_branch(head_data);

    int errno_s = errno;
    free(buf);
    errno = errno_s;
    return commit_hash;
}

int check_name_per_branch (const char* name) {
    if (name == NULL) { SET_ERR(EINVAL); return -1; }

    size_t len = strlen(name);
    char* tmp = (char*)malloc(sizeof(char) * (len + 1));
    memcpy(tmp, name, len);
    if (tmp[len - 1] == '\n') {
        tmp[len - 1] = '\0';
    }
    tmp[len] = '\0';

    char path[PATH_MAX];
    join_path(path, ".mygit/branches", tmp);
    free(tmp);
    struct stat st;
    if (stat(path, &st) == 0) {
        return true;
    }
    else {
        return false;
    }

    return -1;
}