#ifndef SOL_GENNAIO_PARSER_H
#define SOL_GENNAIO_PARSER_H

typedef struct Command{
    char set;
    char *name;
    int n;
}Command;

void printList(List *L);
void createCommand(List **L, char set, char *name, int n);
List* parser(int argc, char* argv[]);

extern char* savePath;
extern int activater; // Ho attivato r
extern int activateR; // Ho attivato R
extern int activatef; // Ho attivato f
extern int activatep; // Ho attivato p
extern int activateD; // Ho attivato D
extern int activatew; // Ho attivato w
extern int activateW; // Ho attivato W
extern int activateStandard;
extern int timeMillis;
extern char *socketNameConfig;

#endif //SOL_GENNAIO_PARSER_H
