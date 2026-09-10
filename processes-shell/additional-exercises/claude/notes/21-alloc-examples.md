# Concrete allocs from C

In C, the standard calls the thing an **object**: “a region of data storage in the execution environment, the contents of which can represent values.” That is essentially an `alloc`. Here are concrete examples at each layer.

## 1. Compiler-level allocs (static, compile-time)

These are allocs the compiler invents when translating C. They may never exist as runtime addresses.

### Local variable → stack slot

```c
int f(void) {
    int x = 42;
    return x;
}
```

```text
alloc  = x        (compiler place / stack slot)
loc    = [rbp-4, rbp) or a register, decided by the backend
time   = one activation of f
kind   = automatic storage duration
```

The compiler may keep `x` in a register and never give it a stack address. The `alloc` still exists for analysis; the `loc` may be “register eax.”

### SSA temporary

```c
int y = a + b * c;
```

```text
alloc  = t0 = b * c
alloc  = t1 = a + t0
loc    = register or spill slot
time   = live range of the temp
kind   = compiler temporary
```

These allocs are pure compiler abstractions. They have no C-level name.

### Static object

```c
static int counter = 0;
```

```text
alloc  = counter
loc    = [0x404020, 0x404024)  in .data
time   = program start .. program end
kind   = static storage duration
```

### String literal

```c
const char *s = "hello";
```

```text
alloc  = string_literal_1
loc    = [0x402000, 0x402006)  in .rodata
time   = program start .. program end
kind   = static storage duration, immutable
```

### TLS variable

```c
_Thread_local int tls_counter;
```

```text
alloc  = tls_counter
loc    = per-thread offset in TLS block
time   = thread lifetime
kind   = thread storage duration
```

## 2. Runtime-level allocs (dynamic, heap)

These are allocs that exist as real addresses at runtime.

### `malloc`

```c
int *p = malloc(sizeof(int));
```

```text
alloc  = a0
loc    = [0x1000, 0x1004)   returned by malloc
time   = malloc .. free
kind   = allocated storage duration
owner  = p (and anything p is copied into)
```

`a0` is the heap object. `p` is a pointer whose *value* is the address `0x1000`. The borrow tuple:

```text
(time=[t_malloc, t_free), alloc=a0, loc=[0x1000,0x1004), mode=owner, agent=p)
```

### `calloc`

```c
int *arr = calloc(10, sizeof(int));
```

```text
alloc  = a1
loc    = [0x2000, 0x2028)
time   = calloc .. free
kind   = allocated storage duration, zero-initialized
```

Same shape as `malloc`, but the memory is zeroed. The alloc identity is still new and distinct.

### `realloc`

```c
p = realloc(p, 2 * sizeof(int));
```

This is interesting because `realloc` may:

- **extend in place** → same `loc`, same `alloc`, new lifetime segment
- **move** → same `alloc`? or new `alloc`? Depends on your model

In a strict model:

```text
old: a0 at [0x1000, 0x1004)
new: a1 at [0x3000, 0x3008)   // realloc returned a different pointer
```

The old `a0` dies, a new `a1` is born. Any old borrow of `a0` is invalid.

If it extends in place:

```text
a0: [0x1000, 0x1004) -> [0x1000, 0x1008)
```

Same alloc, expanded loc.

### `alloca` (non-standard but common)

```c
void g(void) {
    char *buf = alloca(64);
    // ...
}
```

```text
alloc  = a2
loc    = [rsp-64, rsp)   on the current stack frame
time   = current function activation
kind   = automatic storage duration, but dynamically sized
```

`alloca` creates a stack alloc whose size is not known at compile time. The compiler must still track it as an alloc so it can adjust the stack pointer on return.

### `mmap`

```c
void *m = mmap(NULL, 4096, PROT_READ|PROT_WRITE,
               MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
```

```text
alloc  = a3
loc    = [0x7f0000000000, 0x7f0000001000)   one page
time   = mmap .. munmap
kind   = allocated storage duration, page-backed
```

At the OS layer this is a VMA and a physical frame. At the C layer it is just another alloc with an address.

## 3. OS-level allocs

### Page mapping

```text
alloc  = page / VMA
loc    = virtual address range
time   = mmap .. munmap
kind   = virtual memory area
```

### Physical frame

```text
alloc  = frame F
loc    = physical address range
time   = page fault .. page reclaim
kind   = physical page
```

### Task / process

```text
alloc  = task_struct
loc    = kernel memory
time   = fork .. exit
kind   = kernel object
```

The same abstract shape `(time, alloc, loc)` applies; only the layer and the meaning change.

## 4. Same address, different allocs

This is the key C example that shows `alloc ≠ address`:

```c
int *p = malloc(sizeof(int));   // a0 at 0x1000
free(p);                        // a0 dies

int *q = malloc(sizeof(int));   // a1 at 0x1000 (reused!)
```

```text
a0 = (t0, [0x1000, 0x1004), heap)   // dead
a1 = (t2, [0x1000, 0x1004), heap)   // alive
```

Same `loc`, different `alloc`. A stale borrow of `a0` must not be treated as a borrow of `a1`. This is exactly the use-after-free bug the borrow model is trying to prevent.

## 5. Same alloc, different locs

```c
int *p = malloc(sizeof(int));   // a0 at 0x1000
p = realloc(p, 2*sizeof(int));  // a0 now at 0x3000 (moved)
```

```text
a0 = (t0, [0x1000, 0x1004), heap)  ->  (t0, [0x3000, 0x3008), heap)
```

Same alloc identity, different loc. In a GC language this is a moving collector; in C it is `realloc`. Either way, the address is an attribute of the alloc, not the alloc itself.

## Summary table for C

```text
C construct              alloc                       loc
--------------------------------------------------------------------
int x; (local)           stack slot / register       rbp-4 or eax
int x; (static)          symbol x                    .data address
"hello"                  string literal              .rodata address
_Thread_local int x      TLS symbol                  TLS offset
malloc(n)                heap object a0              runtime address
calloc(n,m)              heap object a0 (zeroed)     runtime address
realloc(p,n)             same or new heap object     old or new address
alloca(n)                dynamic stack slot          rsp-relative
mmap(...)                VMA / page alloc            virtual address
```

So in C:

- **compiler allocs** are stack slots, registers, SSA temps, static symbols, TLS offsets, literals.
- **runtime allocs** are `malloc`/`calloc`/`realloc`/`alloca`/`mmap` results.
- **OS allocs** are VMAs, pages, frames, tasks.
- In every case, `alloc` is the identity/lifetime, and `loc` is where it currently lives. The C pointer you hold is a value pointing at a `loc`, not the `alloc` itself.