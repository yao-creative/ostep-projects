## Formalizing what ownership attaches to

**The base structure: allocation instances and time.** Let $\mathbf{T}$ be a totally ordered set representing program execution steps (instruction count, or just "before/after" — total order is all we need). Let $\mathrm{Alloc}$ be the set of *allocation instances* — not variables, not source-code declarations, but concrete events "this storage came into existence." Each $a \in \mathrm{Alloc}$ has a lifetime

$$
L(a) = [\text{birth}(a),\ \text{death}(a)) \subseteq \mathbf{T}
$$

Ownership is then a function

$$
\mathrm{owner} : \mathrm{Alloc} \to \mathrm{Agent}, \qquad \mathrm{owner}(a) = \text{the agent obligated to trigger death}(a)
$$

where "agent" is either *a specific stack frame instance* (for automatic storage — death is automatic, triggered by `return`) or *whatever code path currently holds the free-obligation* (for heap storage — death is `free()`, and the obligation can be handed off). The critical word is **instance**: $\mathrm{owner}$ is not defined on function definitions or variable names, it's defined on the allocation event itself. Two calls to `make_greeting` produce two different elements of $\mathrm{Alloc}$ with two different, non-overlapping $L(a)$'s, even though they came from "the same" line of source.

**Borrowing as a graph.** Define a reference relation $\to\ \subseteq \mathrm{Alloc} \times \mathrm{Alloc} \times \mathbf{T}$, where $a \xrightarrow{t} b$ means "at time $t$, some pointer stored inside $a$ points into $b$'s storage." The single safety invariant of the entire discipline is:

$$
\forall a, b, t.\quad a \xrightarrow{t} b \ \implies\ t \in L(b)
$$

*The referent must still be alive at every moment the reference is used.* Everything we've discussed — dangling pointers, use-after-free, the whole borrow-containment idea from before — is this one universally-quantified statement. A "borrow" is just an edge in this graph; "ownership" is which node is responsible for the endpoint's death event.

## Is it a monotonic-stack-parseable problem? — yes for automatic storage, no in general for heap

You're right that there's a parseable structure here, but it's stronger than "monotonic" — it's **laminarity** (non-crossing nesting), and it holds *only* for automatic storage, which is exactly why this half of the problem is easy and the other half is hard.

**Automatic storage is laminar.** Because a callee's frame is always created after and destroyed before its caller's frame returns (function calls nest, they never "overlap sideways"), the set of lifetime intervals $\{L(F) : F \text{ a stack frame}\}$ satisfies: for any two frames $F_1, F_2$,

$$
L(F_1) \cap L(F_2) = \varnothing \quad \text{or} \quad L(F_1) \subseteq L(F_2) \quad \text{or} \quad L(F_2) \subseteq L(F_1)
$$

— never a partial overlap. This is precisely the interval structure of a **matched-parenthesis / Dyck language**, which is why it's checkable by a **pushdown automaton** (your "parser" intuition is exactly right here): push on frame entry, pop on frame exit, and a borrow is only safe if its target frame is still on the stack when the borrow is used. This is literally what a compiler's lexical-scope lifetime checker does, and it's why "returning a pointer to a local" is a *syntactically* detectable error in principle — no aliasing analysis needed, just stack-depth bookkeeping.

**Heap storage is not laminar, and that's the whole difficulty.** Two `malloc`'d objects' lifetimes can cross arbitrarily — object $a$ born, then $b$ born, then $a$ dies, then $b$ dies is completely legal, unlike frames. There is no automatic destruction event to anchor an automaton against; death only happens when *some* piece of code decides to call `free`. Checking the safety invariant $a \xrightarrow{t} b \Rightarrow t \in L(b)$ for arbitrary heap reference graphs is, in general, exactly **pointer-aliasing analysis**, which is undecidable in the general case (it reduces to reachability questions equivalent to the halting problem for programs with unrestricted mutable aliasing). This is *why* Rust doesn't try to solve the general graph problem at runtime or even via arbitrary static analysis — instead it restricts the *language* itself (affine/linear typing: each value has exactly one owner, borrows are tracked as a syntactic capability, no two mutable aliases coexist) so that the reference graph is forced back into something laminar-like (region/lifetime nesting) by construction, making it checkable again by roughly the same stack-style discipline as the automatic-storage case. So: your instinct is correct and is in fact the actual research lineage (region inference, e.g. Tofte–Talpin) — it's just that it only works for free on the stack; on the heap you need to *impose* the discipline (via a type system or, in C, via convention and comments) rather than discover it by parsing.

## Why Exercise 1 is actually wrong — concrete trace, not "just a different ref"

Your reading was "it's just a different ref outside, or a copy on return" — that phrasing assumes the compiler does *something* to make the data survive, either by giving you a new reference to live data or by copying it. **Neither happens.** No copy is made; `return buf;` in C for an array does not copy the array's contents anywhere — arrays decay to a pointer to their own storage, and that's the pointer you get back, unchanged, still pointing at the same stack slot. There is no hidden allocation, no hidden copy-out. This is the single most common false intuition to correct here, so let's trace it in time.

$$
\begin{array}{ll}
t_0: & \texttt{main} \text{ calls } \texttt{make\_greeting("Bob")} \\
t_1: & \text{frame } F \text{ for } \texttt{make\_greeting} \text{ is pushed}; \ \texttt{buf} \text{ is allocated inside } F,\ L(\texttt{buf}) \text{ begins} \\
t_2: & \texttt{snprintf} \text{ writes "hello, Bob"} \text{ into } \texttt{buf}\text{'s bytes} \\
t_3: & \texttt{return buf;} \Rightarrow \text{ decays to the address of } \texttt{buf}\text{'s first byte; frame } F \text{ is popped} \Rightarrow L(\texttt{buf}) \text{ ends} \\
t_4: & \texttt{outside\_ref} \text{ is assigned that address, back in } \texttt{main}\text{'s frame}
\end{array}
$$

At $t_4$, $\texttt{outside\_ref} \xrightarrow{t_4} \texttt{buf}$, but $t_4 \notin L(\texttt{buf})$ — the safety invariant is violated *at the exact moment the assignment happens*, before you've even read anything. The risk isn't hypothetical or about "eventually stale data" — the referent is already dead by the time the pointer exists.

**What makes this dangerous rather than an immediate visible crash:** stack memory isn't zeroed or unmapped on frame-pop; popping a frame is just "decrement the stack pointer," it doesn't erase bytes. So `printf("%s", outside_ref)` immediately after will very often print `"hello, Bob"` correctly — the bytes are still physically sitting there, untouched. The failure only becomes visible once *something else* pushes a new frame that reuses that same stack region and overwrites those bytes — e.g. calling any other function in between. That's the actual risk: not "will it break" but "it will break nondeterministically, depending on what any other code does afterward, with no relationship visible at the call site to `make_greeting`." That's strictly worse than a guaranteed crash, because it passes testing by accident and fails in production when the call stack shape changes slightly.

Concretely, this breaks:
```c
char *r = make_greeting("Bob");
some_other_function();      // reuses the popped stack region
printf("%s\n", r);          // garbage, or another function's locals, printed as a "string"
```

Does that trace clarify why "copy on return" isn't what's happening? If so, take a shot at Exercise 2 — it's the heap-side sibling of this exact bug, where the fix genuinely does require an explicit copy, and I want to see whether you now reach for `strdup` because you understand *which* lifetime is expiring, not just as memorized ritual.