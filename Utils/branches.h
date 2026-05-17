#ifndef BRANCHES_H
#define BRANCHES_H

int check_detached ();
int create_branch (const char* branchName);
char* resolve_branch (const char* branchName);
char* get_head_commit_hash ();
int get_head_branch_name (char* out_buf);
int check_name_per_branch (const char* name);

#endif