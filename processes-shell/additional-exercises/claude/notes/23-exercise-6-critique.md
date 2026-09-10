## Untangling "same operation, different danger" — this is the crux, and it's simpler than it feels

You're right that these are **syntactically the identical operation** — both are a plain `=` between two `char *`-typed things, and no `strcpy` appears in either. That identity is real, not something you're misreading. What's confusing you is thinking the *danger* comes from the assignment mechanics (does it copy a pointer-value vs. alias a container) — it doesn't. **Every plain pointer assignment in C copies the address value, full stop, always.** `tokens[0] = "ls"`, `first = tokens[0]`, and `p->directories[0] = dir` are all doing *exactly the same kind of copy* — a scalar 8-byte value gets duplicated into a new slot. There is no version of `=` on pointers that "aliases the container" instead of copying — that was never on the table in either exercise.

## So what actually distinguishes safe from dangerous?

**Not the assignment. The lifetime of what's being pointed at, at the far end of the copied address.** Ask, for each case: *does the referent's lifetime outlast every future use of the copy?*

- `tokens[0] = "ls";` — `"ls"` is a string literal, **static storage duration**, alive for the entire program. Any number of copies of its address, made at any time, remain valid forever. Doesn't matter that you didn't `strcpy` the bytes — you don't need to, because the referent never dies, so holding its address indefinitely is always fine.
- `p->directories[0] = dir;` (the dangerous version, where `dir` came from `char local_dir[16]` inside a callee) — `local_dir` has **automatic storage duration**, alive only until that function returns. The copied address becomes worthless the instant that frame pops, regardless of how many places hold a copy of it.

**The type signature difference you noticed (`SearchPath *p` vs `char **tokens`) is a red herring — it has zero bearing on the danger.** Both are "a pointer to something that holds `char *` values." What actually mattered in the dangerous exercise was never the function signature — it was *where `dir`'s own storage lived*, which was one call-frame up from where the assignment happened. If `dir` in that exercise had instead been a string literal (as it happened to be in your very first draft of that code!), `set_first_dir(p, dir)` would have been just as safe as `tokens[0] = "ls"` is here — same function, same signature, safe or dangerous purely depending on what the caller passed in. You actually flagged this yourself, several turns back, when I pointed out that your original `dangling_borrow` test case couldn't demonstrate the bug *because* `dir` was a literal — that's this exact same fact, from the other direction.

## Side-by-side memory states

```
═══════════════════ SAFE: tokens[0] = "ls"; first = tokens[0]; ═══════════════════

STACK                              STATIC (.rodata)
+------------------+
| tokens: 0x2000   |----+
| first:  0x9000   |----|-----------------------------+
+------------------+    |                              |
                         v                              v
HEAP                   +-------------------+      +----------+
+--------------------- | tokens[] @ 0x2000 |      | "ls\0"   |
| [0] = 0x9000  -------+                   |      | @ 0x9000 |
| [1..] = ...          +-------------------+      +----------+
+---------------------

first = 0x9000  ─────────────────────────►  points at STATIC data
                                             lifetime = whole program
                                             copy made anytime = always valid


═══════════════ DANGEROUS: p->directories[0] = dir  (dir = local_dir, a stack buffer) ═══════════════

  --- WHILE populate_from_stack is still on the stack ---

STACK (grows down)
+---------------------------+
| dangling_borrow frame     |
|   p.directories = 0x2000  |----+
+---------------------------+    |
| populate_from_stack frame |    |
|   local_dir @ 0x7000      |<---+---+
|   bytes: "dir\0"          |        |
+---------------------------+        |
                                      |
HEAP                                  |
+-------------------+                 |
| directories[]      |                 |
| @ 0x2000            |                 |
| [0] = 0x7000  ------+-----------------+
+-------------------+

  --- AFTER populate_from_stack RETURNS ---

STACK
+---------------------------+
| dangling_borrow frame     |
|   p.directories = 0x2000  |----+
+---------------------------+    |
| [[ FRAME POPPED —         |    |
|    0x7000 now RECLAIMED, |    |
|    contents undefined ]]  |    |
+---------------------------+    |
                                  |
HEAP                              |
+-------------------+             |
| directories[]      |             |
| @ 0x2000            |             |
| [0] = 0x7000  ------+-------------+---> points into DEAD stack region
+-------------------+                    same bit pattern as before —
                                          but nothing there anymore
```

Notice the two "HEAP" boxes are drawn **identically structured** in both diagrams — `directories[0]`/`first` holding some address, copied in by plain assignment, no `strcpy` either time. The only thing that differs between "safe" and "dangerous" is what's drawn on the *right-hand side*, at the far end of the arrow: a `.rodata` box that's permanent, versus a stack-frame box that gets hatched-out the moment its owning function returns. The copy operation itself never changes; the danger is 100% a fact about the target, never about the mechanics of `=`.