```
STATE: mid-loop, right after i=3's strdup(dirs[3]) has returned NULL
       (loop is about to move on to i=4 — nothing has stopped it)

STACK                                   HEAP
+----------------------+
| path ----------------+------------+
| dirs  ---------------+--------+   |
| count = 5            |        |   |
+----------------------+        |   |
                                 |   v
                                 |  +--------------------------------------+
                                 |  | path->directories[]  (5 slots, ptrs)  |
                                 |  +--------------------------------------+
                                 |  | [0] --------> "usr"   (strdup'd, owned)
                                 |  | [1] --------> "local" (strdup'd, owned)
                                 |  | [2] --------> "bin"   (strdup'd, owned)
                                 |  | [3] = NULL   <----- strdup FAILED here, explicit sentinel
                                 |  | [4] = ??????  <----- NOT YET VISITED by the loop
                                 |  +--------------------------------------+
                                 |
                                 v
                                +----------------------------------+
                                | dirs[] (caller's array, borrowed) |
                                +----------------------------------+
                                | [0] --> "usr"
                                | [1] --> "local"
                                | [2] --> "bin"
                                | [3] --> "sbin"     <- strdup tried to copy this, malloc inside strdup failed
                                | [4] --> "opt"      <- loop hasn't reached this index yet
                                +----------------------------------+


  ── WITH plain malloc(count * sizeof(char*)) ──

      directories[4] right now:
      +------------------------+
      | ?? ?? ?? ?? ?? ?? ?? ?? |   <- whatever bit pattern was already sitting
      +------------------------+      in that heap byte range before malloc handed
                                       it to you. Could be a leftover pointer from
                                       some earlier, unrelated free()'d block.
                                       NOT necessarily zero, NOT necessarily NULL.

  ── WITH calloc(count, sizeof(char*)) instead ──

      directories[4] right now:
      +------------------------+
      | 00 00 00 00 00 00 00 00 |  <- guaranteed zero bytes = NULL, from the moment
      +------------------------+      of allocation, before the loop ever ran.
```

**What each row is showing:**

- **`[0..2]`** — three completed, successful `strdup` calls. Each is its **own independent heap allocation**, disjoint from `dirs[i]`'s storage (the string data was copied, not aliased) — this is the "owned entries" state from Exercise 3's fix, already in place here.
- **`[3]` — the failure slot itself.** `strdup` returned `NULL`, and the assignment `path->directories[3] = strdup(dirs[3]);` wrote that `NULL` in, unconditionally. This is a **defined, safe value** — not garbage, not an unowned address. It's the reason `free(path->directories[3])` later in `clear_path` is harmless: `free(NULL)` is a guaranteed no-op by the C standard.
- **`[4]`** — this is where the `malloc` vs `calloc` distinction actually bites. Under plain `malloc`, this slot is genuinely undefined bits — could coincidentally look like a valid-looking address, could be anything — until the loop reaches `i=4` on its very next iteration and overwrites it. Under `calloc`, it's already `NULL`, so even if something *external* somehow observed the struct at this exact instant (it can't, in this single-threaded code, but in a hypothetical version where the loop could be interrupted or partially retried), it would see a safe sentinel rather than indeterminate bytes.

The diagram is really making one point concrete: **the difference between `malloc` and `calloc` only shows up in the row you haven't visited yet** — everything the loop has already touched (`[0..3]`) looks identical either way, since assignment overwrites whatever `malloc`/`calloc` initially put there. `calloc`'s guarantee is entirely about the *not-yet-written* tail of the array, which is exactly the region Exercise 4's question was probing.