#ifndef CLAUDE_EXERCISES_H
#define CLAUDE_EXERCISES_H
#define GREETING_BUF_SIZE (64 * sizeof(char))
char *make_greeting(char *name);

void make_greeting_caller(void);
void set_first_dir(SearchPath *p, char *dir);

#endif
