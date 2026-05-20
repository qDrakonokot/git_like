// чтобы сделать пулл реквест

#include "GeneralFunc/baseGitFunc.h"
#include "GeneralFunc/advancedGitFunc.h"
#include "CommitAndIndex/fileEntry.h"
#include "Utils/workWithFiles.h"
#include "Utils/branches.h"
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
// Включаем поддержку ANSI-кодов в консоли Windows
void setup_windows_console() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return;
    dwMode |= 0x0004; 
    SetConsoleMode(hOut, dwMode);
}
#endif


int main (int argc, char* argv[]) {
    #ifdef _WIN32
    setup_windows_console();
    #endif
    
    int ret_c = 0;
    if (argc >= 2) {
        if (strcmp(argv[1], "--help") == 0) {
            printf("\n");
            printf(ANSI_COLOR_MAGENTA "Hello! It's small local git-like system\n" ANSI_COLOR_RESET);
            printf("Below is a set of commands:\n");
            printf("\t" ANSI_COLOR_YELLOW "init:" ANSI_COLOR_RESET "     initialize empty repository with empty start commit.\n");
            printf("\t" ANSI_COLOR_YELLOW "add:" ANSI_COLOR_RESET "      adds files to the list to be added to the next commit.\n");
            printf("\t\t  Can accept both files and directories. Minimum 1 file per addition.\n");
            printf("\t\t  Also can marked deleted file as deleted automaticly.\n");
            printf("\t" ANSI_COLOR_YELLOW "remove:" ANSI_COLOR_RESET "   adds files to the list to be marked as removed to the next commit.\n");
            printf("\t\t  Can accept both files and directories. Minimum 1 file per remove.\n");
            printf("\t" ANSI_COLOR_YELLOW "commit:" ANSI_COLOR_RESET "   saves the state. Adds or marks files removed from the change list,\n");
            printf("\t\t  and saves the message.\n");
            printf("\t\t  Receives a message as input.\n");
            printf("\t" ANSI_COLOR_YELLOW "status:" ANSI_COLOR_RESET "   output current list of changes.\n");
            printf("\t" ANSI_COLOR_YELLOW "log:" ANSI_COLOR_RESET "      output a chain of commits with their messages and hashes.\n");
            printf("\t\t  Accepts the --n (number) flag, which limits the output (number) to commits.\n");
            printf("\t\t  If one commit is specified, then the output will start from it,\n"); 
            printf("\t\t  if 2 in a row, then from the second to the first.\n");
            printf("\t" ANSI_COLOR_YELLOW "diff:" ANSI_COLOR_RESET "     output changes in files in relation to submitted commits\n");
            printf("\t\t  Accepts 2 commits, if the second one is not specified,\n");
            printf("\t\t  then compares it with the current one.\n");
            printf("\t" ANSI_COLOR_YELLOW "checkout:" ANSI_COLOR_RESET " restores the state described in the specified commit or branch\n");
            printf("\t\t  If it is not a branch that is being restored, then all other commands,\n");
            printf("\t\t  except for restoring and creating and changing a branch, become unavailable\n");
            printf("\t" ANSI_COLOR_YELLOW "branch:" ANSI_COLOR_RESET "   creates a new branch or shows lists all branches\n");
            printf("\t" ANSI_COLOR_YELLOW "switch:" ANSI_COLOR_RESET "   switches branch\n");
            printf("\n");
            printf("Examples of use: \n");
            printf("\tAll commands that can accept commits can also accept branch names\n");
            printf("\tmygit " ANSI_COLOR_YELLOW "init" ANSI_COLOR_RESET "\n");
            printf("\tmygit " ANSI_COLOR_YELLOW "add" ANSI_COLOR_RESET "      file1 file2 file3 ..\n");
            printf("\tmygit " ANSI_COLOR_YELLOW "remove" ANSI_COLOR_RESET "   file1 file2 file3 ..\n");
            printf("\tmygit " ANSI_COLOR_YELLOW "commit" ANSI_COLOR_RESET "   \"message\"\n");
            printf("\tmygit " ANSI_COLOR_YELLOW "status" ANSI_COLOR_RESET "\n");
            printf("\tmygit " ANSI_COLOR_YELLOW "log" ANSI_COLOR_RESET "      --n num commit1 commit2\n");
            printf("\tmygit " ANSI_COLOR_YELLOW "diff" ANSI_COLOR_RESET "     commit1 commit2\n");
            printf("\tmygit " ANSI_COLOR_YELLOW "checkout" ANSI_COLOR_RESET " commit\n");
            printf("\tmygit " ANSI_COLOR_YELLOW "branch" ANSI_COLOR_RESET "   branchName\n");
            printf("\tmygit " ANSI_COLOR_YELLOW "switch" ANSI_COLOR_RESET "   branchName\n");
            printf("\n");
        }
        else if (strcmp(argv[1], "init") == 0) {
            if (argc > 2) {
                fprintf(stderr, "Invalid command. Type --help for manual\n");
                return -1;
            } 
            ret_c = adv_cmd_init();
            if (ret_c != 0) return -1;
        }
        else if (strcmp(argv[1], "add") == 0) {
            if (argc < 3) {
                fprintf(stderr, "Invalid command. Type --help for manual\n");
                return -1;
            }
            for (int i = 2; i < argc; ++i) {
                ret_c = adv_cmd_add(argv[i]);
                if (ret_c != 0) return -1;
            }
        }
        else if (strcmp(argv[1], "remove") == 0) {
            if (argc < 3) {
                fprintf(stderr, "Invalid command. Type --help for manual\n");
                return -1;
            }
            for (int i = 2; i < argc; ++i) {
                ret_c = adv_cmd_remove(argv[i]);
                if (ret_c != 0) return -1;
            }
        }
        else if (strcmp(argv[1], "commit") == 0) {
            if (argc < 3 && argc > 3) {
                fprintf(stderr, "Invalid command. Type --help for manual\n");
                return -1;
            }
            ret_c = adv_cmd_commit(argv[2]);
            if (ret_c != 0) return -1;
        }
        else if (strcmp(argv[1], "status") == 0) {
            if (argc > 2) {
                fprintf(stderr, "Invalid command. Type --help for manual\n");
                return -1;
            } 
            ret_c = adv_cmd_status();
            if (ret_c != 0) return -1;
        }
        else if (strcmp(argv[1], "log") == 0) {
            if (argc > 6) {
                fprintf(stderr, "Invalid command. Type --help for manual\n");
                return -1;
            }
            else if (argc == 2) {
                ret_c = adv_cmd_log(NULL, NULL, NULL);
                if (ret_c != 0) return -1;
            }
            else {
                int commit_cnt;
                char commit_start[FILE_NAME_MAX];
                char commit_end[FILE_NAME_MAX];

                int* cnt_ptr = NULL;
                char* start_ptr = NULL;
                char* end_ptr = NULL;

                int i = 2;
                while (i < argc) {
                    if (strcmp(argv[i], "--n") == 0) {
                        if (i+1 >= argc) {
                            fprintf(stderr, "Invalid args. Type --help for manual\n");
                            return -1;
                        }
                        char *endptr;
                        long val = strtol(argv[i+1], &endptr, 0);
                        if (argv[i+1] == endptr || *endptr != '\0') {
                            fprintf(stderr, "Invalid number. Type --help for manual\n");
                            return -1;
                        }
                        commit_cnt = (int)val;
                        cnt_ptr = &commit_cnt;

                        i += 2;
                    }
                    else {
                        if (i+1 < argc && argv[i+1][0] != '-') {
                            strcpy(commit_end, argv[i]);
                            end_ptr = commit_end;
                            strcpy(commit_start, argv[i+1]);
                            start_ptr = commit_start;

                            i += 2;
                        }
                        else {
                            strcpy(commit_start, argv[i]);
                            start_ptr = commit_start;

                            i += 1;
                        }
                    }
                }

                ret_c = adv_cmd_log(start_ptr, cnt_ptr, end_ptr);
                if (ret_c != 0) return -1;                
            }
        }
        else if (strcmp(argv[1], "diff") == 0) {
            if (argc < 3 && argc > 4) {
                fprintf(stderr, "Invalid command. Type --help for manual\n");
                return -1;
            }
            ret_c = adv_cmd_diff(argv[2], argv[3]);
            if (ret_c != 0) return -1;
        }
        else if (strcmp(argv[1], "checkout") == 0) {
            if (argc < 3 && argc > 3) {
                fprintf(stderr, "Invalid command. Type --help for manual\n");
                return -1;
            }
            ret_c = adv_cmd_checkout(argv[2]);
            if (ret_c != 0) return -1;
        }
        else if (strcmp(argv[1], "branch") == 0) {
            if (argc < 3) {
                ret_c = adv_cmd_branch(NULL);
                if (ret_c != 0) return -1;
            }
            else {
                if (argc > 3) {
                    fprintf(stderr, "Invalid command. Type --help for manual\n");
                    return -1;
                } 
                ret_c = adv_cmd_branch(argv[2]);
                if (ret_c != 0) return -1;
            }
        }
        else if (strcmp(argv[1], "switch") == 0) {
            if (argc < 3 && argc > 3) {
                fprintf(stderr, "Invalid command. Type --help for manual\n");
                return -1;
            }
            ret_c = adv_cmd_switch(argv[2]);
            if (ret_c != 0) return -1;
        }
        else {
            printf("Undefined command. Type --help for manual\n");
        }
    }
    else {
        printf("For manual type 'mygit --help'\n");
    }
    return 0;
}
