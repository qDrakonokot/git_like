#ifndef BASEGITFUNC_H
#define BASEGITFUNC_H

#define ANSI_BOLD          "\033[1m"
#define ANSI_COLOR_RED     "\033[31m"
#define ANSI_COLOR_GREEN   "\033[32m"
#define ANSI_COLOR_YELLOW  "\033[33m"
#define ANSI_COLOR_BLUE    "\033[34m"
#define ANSI_COLOR_MAGENTA "\033[35m"
#define ANSI_COLOR_CYAN    "\033[36m"
#define ANSI_COLOR_RESET   "\033[0m"

int cmd_init ();
int cmd_add (const char* filename);
int cmd_remove (const char* filename);
int cmd_commit (const char* message);
int cmd_status ();
int cmd_log (const char* start_commit_hash, const int* commit_cnt);
int cmd_diff (const char* target_commit_hash);
int cmd_checkout (const char* commit_hash, const char* filename);

#endif