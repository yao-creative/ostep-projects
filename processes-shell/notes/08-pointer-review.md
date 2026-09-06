**Intent: pointer/reference model in C**, especially how `*`, `**`, `&`, and string literals relate.

The cleanest way to understand C is to separate **values**, **objects**, and **addresses**.

### 1. Start with ordinary variables

```c
int x = 42;
```

There is an object `x` containing the value `42`.

You can think of:

$$
x : \texttt{int}
$$

and

$$
\&x : \texttt{int*}
$$

`&x` means **"the address of the object x."**

```c
int *p = &x;
```

Now:

```text
p        → address of x
*p       → value stored at that address
```

So:

```c
printf("%d\n", x);   // 42
printf("%d\n", *p);  // 42
```

The important algebra is:

$$
p = \&x
$$

therefore

$$
*p = x
$$

`&` and unary `*` are approximately inverse operations when applied to a valid object/pointer pair:

$$
*(\&x) = x
$$

and, for a valid pointer `p`:

$$
\&(*p) = p
$$

---

# 2. Then what does `**` mean?

`**` is **not really one operator**. It's two applications of `*`.

```c
int x = 42;
int *p = &x;
int **q = &p;
```

The types form:

$$
x : \texttt{int}
$$

$$
p : \texttt{int*}
$$

$$
q : \texttt{int**}
$$

And the relationships are:

$$
p = \&x
$$

$$
q = \&p
$$

Therefore:

$$
*q = p
$$

and:

$$
**q = x
$$

So you can visualize the type transformation as:

$$
\texttt{int}
\xleftarrow{*}
\texttt{int*}
\xleftarrow{*}
\texttt{int**}
$$

Reading `*` from the **expression** perspective:

```c
q       // int**
*q      // int*
**q     // int
```

Each `*` follows one pointer layer.

---

# 3. Why does C need `**`?

Because sometimes you don't merely want to modify the thing a pointer points to.

You want to modify the **pointer itself**.

For example:

```c
void change(int **p) {
    static int y = 100;
    *p = &y;
}

int x = 42;
int *p = &x;

change(&p);
```

Before:

$$
p = \&x
$$

Inside `change`:

```c
p
```

is a pointer to the caller's pointer.

Therefore:

```c
*p
```

is the caller's actual `int *`.

So:

```c
*p = &y;
```

changes the caller's `p`.

Afterwards:

$$
p = \&y
$$

This is the fundamental reason `**` occurs so much in C APIs.

---

# 4. Strings make this confusing

C has **no built-in string type**.

Instead, a C string is conventionally:

> a contiguous sequence of `char` ending with `'\0'`.

For example:

```c
char s[] = "hello";
```

Conceptually the object is:

$$
s = [h,e,l,l,o,\backslash0]
$$

There are **6 chars**, including the null terminator.

The type is:

```c
char[6]
```

not `char *`.

But arrays have a special rule: in most expressions, an array is converted into a pointer to its first element.

So:

```c
s
```

usually becomes:

```c
&s[0]
```

with type:

```c
char *
```

Hence:

```c
printf("%c\n", *s);
```

prints:

```text
h
```

because:

$$
*s = s[0] = h
$$

And:

```c
printf("%c\n", *(s + 1));
```

prints:

```text
e
```

because:

$$
*(s+1) = s[1]
$$

---

# 5. `char *` is therefore extremely important

When you write:

```c
char *s = "hello";
```

`s` is a pointer to the first character.

Conceptually:

$$
s \rightarrow [h,e,l,l,o,\backslash0]
$$

Then:

```c
*s
```

means the first character:

```text
'h'
```

and:

```c
s + 1
```

means the address of the second character.

So:

```c
*(s + i)
```

means:

$$
\text{character at index }i
$$

This is why:

```c
s[i]
```

is defined in terms of pointer arithmetic:

$$
s[i] \equiv *(s+i)
$$

---

# 6. The really important distinction: `char[]` vs `char *`

Compare:

```c
char a[] = "hello";
char *b = "hello";
```

They look similar but mean different things.

### `char a[]`

```c
char a[] = "hello";
```

creates an actual array object:

$$
a : \texttt{char[6]}
$$

The characters are stored **inside `a`**.

You can modify them:

```c
a[0] = 'H';
```

Now `a` contains:

```text
Hello
```

### `char *b`

```c
char *b = "hello";
```

creates a pointer:

$$
b : \texttt{char*}
$$

which points at the string literal.

You should **not modify the literal**:

```c
b[0] = 'H'; // undefined behavior
```

The pointer itself is mutable:

```c
b = "world";
```

but the literal it points to isn't something you should write into.

---

# 7. Now `char **`

This becomes natural once you think in layers.

```c
char c = 'A';

char *p = &c;

char **q = &p;
```

Types:

$$
c : \texttt{char}
$$

$$
p : \texttt{char*}
$$

$$
q : \texttt{char**}
$$

Then:

```c
*q
```

is `p`.

And:

```c
**q
```

is `c`.

So:

$$
**q = c
$$

---

# 8. Strings + `char **`

This is where C programming gets interesting.

Suppose:

```c
char *s = "hello";
```

Then:

$$
s : \texttt{char*}
$$

If we take its address:

```c
&s
```

we get:

$$
\&s : \texttt{char**}
$$

So:

```c
void f(char **p) {
    *p = "world";
}
```

and:

```c
char *s = "hello";

f(&s);
```

Inside `f`:

```c
*p = "world";
```

changes `s` itself.

Before:

$$
s \rightarrow \text{"hello"}
$$

After:

$$
s \rightarrow \text{"world"}
$$

This is the same fundamental mechanism as `int **`; the fact that the thing ultimately points to characters doesn't change the pointer mechanics.

---

# 9. A very useful hierarchy

For any type `T`:

| Declaration | Meaning                                    |
| ----------- | ------------------------------------------ |
| `T x`       | object containing a `T`                    |
| `T *p`      | pointer to a `T`                           |
| `T **p`     | pointer to a pointer to a `T`              |
| `T ***p`    | pointer to a pointer to a pointer to a `T` |

For `char`:

$$
\begin{aligned}
\texttt{char} &\quad\text{character}\\
\texttt{char*} &\quad\text{pointer to character}\\
\texttt{char**} &\quad\text{pointer to pointer to character}\\
\texttt{char***} &\quad\text{pointer to pointer to pointer to character}
\end{aligned}
$$

For strings, the terminology can obscure this because `char *` is **used by convention as a C-string representation**.

But fundamentally:

$$
\boxed{\texttt{char *} = \text{pointer to char}}
$$

not "string" as a primitive type.

---

# 10. The `&` vs `*` rule I'd memorize

When reading an expression:

**`&` goes outward to an address.**

```c
&x
```

means:

> Where is `x`?

**`*` goes inward through an address.**

```c
*p
```

means:

> What is stored at the address in `p`?

Thus:

```c
int x = 42;
int *p = &x;
int **q = &p;
```

gives the chain:

$$
q
\overset{*}{\longrightarrow}
p
\overset{*}{\longrightarrow}
x
$$

and the reverse direction:

$$
x
\overset{\&}{\longrightarrow}
p
\overset{\&}{\longrightarrow}
q
$$

That's the core mental model. Once this is solid, things like `char **argv`, `char ***`, `malloc`, `strsep`, linked lists, and C APIs that "modify your pointer" become much less mysterious.
