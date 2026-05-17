CC = gcc

CFLAGS = -g3 -O0 -Wall -Wextra -Wpedantic -Werror \
         -fno-omit-frame-pointer \
         -fsanitize=address,undefined,leak -I.

LDFLAGS = -L. -fsanitize=address,undefined,leak

LDLIBS = -lm
OBJ = main.o funcs.a hashmap.o cai.a utils.a errors.o vector.o


mygit: $(OBJ)
	$(CC) $(CFLAGS) -o mygit $(OBJ) $(LDFLAGS) $(LDLIBS)

main.o: main.c
	$(CC) $(CFLAGS) -c main.c -o main.o

funcs.a: baseGitFunc.o advancedGitFunc.o
	ar rcs funcs.a baseGitFunc.o advancedGitFunc.o

advancedGitFunc.o: GeneralFunc/advancedGitFunc.c
	$(CC) $(CFLAGS) -c GeneralFunc/advancedGitFunc.c -o advancedGitFunc.o

baseGitFunc.o: GeneralFunc/baseGitFunc.c 
	$(CC) $(CFLAGS) -c GeneralFunc/baseGitFunc.c -o baseGitFunc.o

vector.o: DataStructures/vector.c
	$(CC) $(CFLAGS) -c DataStructures/vector.c -o vector.o
 
errors.o: ErrorsDefenition/errors.c 
	$(CC) $(CFLAGS) -c ErrorsDefenition/errors.c -o errors.o

utils.a: hashFunc.o workWithFiles.o branches.o myers.o
	ar rcs utils.a hashFunc.o workWithFiles.o branches.o myers.o

hashFunc.o: Utils/hashFunc.c 
	$(CC) $(CFLAGS) -c Utils/hashFunc.c -o hashFunc.o

workWithFiles.o: Utils/workWithFiles.c 
	$(CC) $(CFLAGS) -c Utils/workWithFiles.c -o workWithFiles.o

branches.o: Utils/branches.c
	$(CC) $(CFLAGS) -c Utils/branches.c -o branches.o

myers.o: Utils/myers.c
	$(CC) $(CFLAGS) -c Utils/myers.c -o myers.o

cai.a: commit.o fileEntry.o index.o 
	ar rcs cai.a commit.o fileEntry.o index.o 

commit.o: CommitAndIndex/commit.c 
	$(CC) $(CFLAGS) -c CommitAndIndex/commit.c -o commit.o
    
fileEntry.o: CommitAndIndex/fileEntry.c 
	$(CC) $(CFLAGS) -c CommitAndIndex/fileEntry.c -o fileEntry.o

index.o: CommitAndIndex/index.c
	$(CC) $(CFLAGS) -c CommitAndIndex/index.c -o index.o

hashmap.o: DataStructures/hashmap.c
	$(CC) $(CFLAGS) -c DataStructures/hashmap.c -o hashmap.o

clean:	
	rm *.o *.a