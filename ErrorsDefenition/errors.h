#ifndef ERROR_H
#define ERROR_H

#include <errno.h>

#define MY_ERR_REPO_EXISTS 1000
#define MY_ERR_NOT_A_REPO 1001
#define MY_ERR_FILE_NOT_FOUND 1002
#define MY_ERR_BAD_FILE_FORMAT 1003
#define MY_ERR_COMMIT_NOT_FOUND 1004
#define MY_ERR_OUT_OF_VEC_SIZE 1005
#define MY_ERR_REPO_BAD_INDEX 1006
#define MY_ERR_HASHMAP_FULL 1007
#define MY_ERR_HASHMAP_DOES_NOT_EXISTS 1008
#define MY_ERR_REHASH_FAILED 1009
#define MY_ERR_HASHMAP_NOT_FOUND 1010
#define MY_ERR_BRANCH_DOES_NOT_EXIST 1011

#define SET_ERR(code) do { errno = (code); } while(0)
char* strmyerr(int err);


#endif