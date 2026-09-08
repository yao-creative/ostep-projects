## Critique

**Answer to the lifetime question, precisely.** `set_first_dir` stores `dir` — a borrowed pointer — directly into `p->directories[0]`, with no copy. This is safe *only* as long as

$$
\text{lifetime}(\texttt{p}) \subseteq \text{lifetime}(\text{whatever } \texttt{dir} \text{ points at})
$$

In your test, `dir` happens to be a string literal, which has **static storage duration** — it lives for the entire program, so this particular call can never dangle no matter how long `p` survives. But the function's contract doesn't know or enforce that. Swap the call site to:
```c
void make_path_entry(SearchPath *p) {
    char local_dir[16];
    strcpy(local_dir, "dir");
    set_first_dir(p, local_dir);   // borrow of a stack frame
}   // local_dir's frame dies HERE
```
and `p->directories[0]` is now a dangling pointer the instant `make_path_entry` returns — the exact Exercise-1 failure, just laundered through an extra struct field so it's less visible at the call site. `set_first_dir`'s signature (`char *dir`) gives the caller no signal about which contract is expected; that ambiguity is the actual design defect, independent of whether today's caller happens to dodge it.

### Bugs in your code, in order

1. `char *dir = "dir"` — missing semicolon.
2. `printf("malloc error")` — missing semicolon; also should be `fprintf(stderr, ...)` per your own earlier convention (error messages belong on `stderr`, not `stdout`).
3. `path.directory_count = 0;` inside `init_path` — `path` is a parameter of type `SearchPath *`, so this must be `path->directory_count = 0;`. `path.directory_count` won't compile against a pointer.
4. `set_first_dir(p, dir);` — `p` is declared `SearchPath p;` (a value, not a pointer), but `set_first_dir` expects `SearchPath *`. Must be `set_first_dir(&p, dir);`.
5. `free(p->directories);` — same issue in reverse direction: `p` is a value here, so this must be `free(p.directories);` (`p->directories` doesn't compile — `->` requires a pointer operand).
6. Your inline comment is subtly imprecise: *"`p->directories[0]` is not freed even though `p->directories` is freed."* More precisely — after `free(p.directories)`, the array `p.directories` pointed at is gone; `p.directories` itself becomes a **dangling pointer to the array**, and indexing it (`p.directories[0]`) is undefined behavior regardless of what value was sitting in slot 0. It isn't that "slot 0 survives while the container dies" — the container's death makes *every* read through it illegal, including reading out the (perfectly valid, since it's a string literal) `dir` value that used to live there. The bug is in touching the array at all post-free, not in the string literal's own validity.
7. `init_path` allocates a fixed `10` slots with no way to grow — fine for an exercise, but note it silently caps you; a real version would need the realloc-on-demand pattern from your earlier `lex_line`.

### Rewrite — `SearchPath` genuinely owns its entries

```c
int set_first_dir(SearchPath *p, const char *dir) {
    char *owned = strdup(dir);           // COPY: p must not depend on dir's lifetime
    if (owned == NULL) return 1;
    if (p->directories[0] != NULL) free(p->directories[0]);  // release whatever was there
    p->directories[0] = owned;
    return 0;
}
```
Now $p$'s validity no longer references $\text{lifetime}(\texttt{dir})$ at all — the moment `strdup` returns, `p` owns an independent allocation, and `dir`'s caller-side storage can die immediately afterward with zero effect on `p`.

---

## The memory-layout drawingThe right-hand panel makes the exact distinction your comment was reaching for: `p`'s own stack bits don't change at all (still `0x2000`), and the string literal at `0x3000` is genuinely still fine — static storage never dies. What actually goes bad is the *array itself* at `0x2000`: `free()` reclaimed that container, so `p.directories[0]` is an illegal read not because the thing it used to point to died, but because the pointer-holding cell you'd be reading it out of no longer belongs to you. That's a subtlety worth keeping distinct from Exercise 1's version — there, the referent died under a live pointer; here, the *container of the pointer* died while the referent it pointed to (in this one specific case) happened to survive regardless.