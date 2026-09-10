Assuming C / POSIX `strdup`, and you mean:

```c
char *y = "hello";     // y already exists
char *x = strdup(y);   // now duplicate it into x
```

### Before `x = strdup(y)`

`y` points to the original string. `x` is not assigned yet. No heap copy exists.

```text
STACK                         READ-ONLY DATA / STRING LITERAL
+----------------+            +-----------------------------+
| y = 0x4005f0 --+----------->| 'h' 'e' 'l' 'l' 'o' '\0'    |
| x = ???        |            +-----------------------------+
+----------------+

HEAP
+----------------+
| (empty)        |
+----------------+
```

`???` means uninitialized / indeterminate.

### After `x = strdup(y)`

`strdup` did roughly:

```c
size_t n = strlen(y) + 1;
char *p = malloc(n);
memcpy(p, y, n);
return p;
```

So now `x` points to a new heap block containing a copy of `"hello"`.

```text
STACK                         READ-ONLY DATA / STRING LITERAL
+----------------+            +-----------------------------+
| y = 0x4005f0 --+----------->| 'h' 'e' 'l' 'l' 'o' '\0'    |
| x = 0x55a0f0 --+--+         +-----------------------------+
+----------------+  |
                    |         HEAP
                    |         +------------------------------------------+
                    +-------->| 0x55a0f0: 'h' 'e' 'l' 'l' 'o' '\0'       |
                              +------------------------------------------+
```

Key point: `y` and `x` point to different memory.  
`x` owns the heap copy, so later you must do:

```c
free(x);
x = NULL;
```

`y` is not freed if it points to a string literal.

---

## If `y` itself was created by `strdup`

Example:

```c
char *y = strdup("hello");
char *x = strdup(y);
```

Before `x = strdup(y)`:

```text
STACK
+----------------+
| y = 0x55a000 --+----> HEAP A: 'h' 'e' 'l' 'l' 'o' '\0'
| x = ???        |
+----------------+
```

After `x = strdup(y)`:

```text
STACK
+----------------+
| y = 0x55a000 --+----> HEAP A: 'h' 'e' 'l' 'l' 'o' '\0'
| x = 0x55a100 --+----> HEAP B: 'h' 'e' 'l' 'l' 'o' '\0'
+----------------+
```

Now there are **two independent heap allocations**.  
You must free both:

```c
free(x);
free(y);
```

---

## If `y` is a stack array

```c
char y[] = "hello";
char *x = strdup(y);
```

Before:

```text
STACK
+--------------------------------+
| y: 'h' 'e' 'l' 'l' 'o' '\0'    |
| x = ???                        |
+--------------------------------+
```

After:

```text
STACK
+--------------------------------+
| y: 'h' 'e' 'l' 'l' 'o' '\0'    |
| x = 0x55a0f0 -------------------+----> HEAP: 'h' 'e' 'l' 'l' 'o' '\0'
+--------------------------------+
```

`x` remains valid until you `free(x)`, even after the function containing `y` returns.