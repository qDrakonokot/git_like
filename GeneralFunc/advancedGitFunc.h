#ifndef ADVANCEDGITFUNC_H
#define ADVANCEDGITFUNC_H

int adv_cmd_init ();
int adv_cmd_branch (const char* branchName);
int adv_cmd_commit (const char* message);
int adv_cmd_add (const char* main_path);
int adv_cmd_remove (const char* main_path);
int adv_cmd_switch (const char* branchName);
int adv_cmd_status ();
int adv_cmd_log (const char* start_commit, const int* commit_cnt, const char* end_commit);
int adv_cmd_checkout (const char* branchName);
int adv_cmd_diff (const char* branchName1, const char* branchName2);

#endif