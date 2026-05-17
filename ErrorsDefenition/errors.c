#include "errors.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>


char* strmyerr(int err) {
    switch (err) 
    {
    case MY_ERR_REPO_EXISTS:
        return "Repository already exists!";
        break;
    case MY_ERR_NOT_A_REPO:
        return "Repository not a dir!";
        break;
    case MY_ERR_FILE_NOT_FOUND:
        return "File not found!";
        break;
    case MY_ERR_BAD_FILE_FORMAT:
        return "Bad file format or file is a special type!";
        break; 
    case MY_ERR_COMMIT_NOT_FOUND:
        return "Commit not found!";
        break;
    case MY_ERR_OUT_OF_VEC_SIZE:
        return "Out of vector size!";
        break;
    case MY_ERR_REPO_BAD_INDEX:
        return "Index corrupted!";
        break;
    case MY_ERR_HASHMAP_FULL:
        return "Hashmap with filesEntry FULL!";
        break;
    case MY_ERR_HASHMAP_DOES_NOT_EXISTS:
        return "Init hashmap before using!";
        break;
    case MY_ERR_REHASH_FAILED:
        return "Rehash failed!";
        break;
    case MY_ERR_HASHMAP_NOT_FOUND:
        return "Element in hashmap doesn't exists!";
        break;
    
    default:
        return strerror(err);
    }
}

