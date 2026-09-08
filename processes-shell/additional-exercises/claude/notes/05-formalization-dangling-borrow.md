## Formal definition

Recall the setup: $\mathrm{Alloc}$ is the set of allocation instances, $L(a) = [\text{birth}(a), \text{death}(a))$ is $a$'s lifetime interval, and a **borrow** is an edge $p \xrightarrow{t} a$ meaning "at time $t$, the pointer variable $p$ holds an address inside $a$'s storage." The governing safety invariant, stated before, was:

$$
\forall p, a, t.\quad p \xrightarrow{t} a \implies t \in L(a)
$$

**A dangling borrow is precisely a witness to the negation of this invariant** — a triple $(p, a, t)$ such that $p$ points into $a$ but $t \notin L(a)$, i.e. $t \geq \text{death}(a)$ (it can't be $t < \text{birth}(a)$ under this framing, since $p$ can't hold $a$'s address before $a$ exists — that degenerate case is a different pathology, "wild pointer," not dangling). So:

$$
\text{Dangling}(p, a, t) \iff \big(p \xrightarrow{t} a\big) \ \wedge\ \big(t \geq \text{death}(a)\big)
$$

In words: **$p$ still refers to $a$'s address, but $a$'s lifetime has already ended.** The pointer's *value* (the address) hasn't changed — nothing erased it — but the *referent* it points to no longer exists as a valid object. This is the key conceptual thing to hold onto: dangling-ness is not a property of the bit pattern in $p$ at all (the address is perfectly well-formed, often even still readable) — it's a property of the **relationship between two independent timelines**, $p$'s continued existence and $a$'s ended existence, falling out of sync.

## The two ways the invariant gets violated — same formula, two different failure mechanisms

Because the invariant compares $t$ (when the borrow is *used*) against $\text{death}(a)$ (when the referent *ends*), there are exactly two causal routes to violating it, corresponding to which side moves:

1. **The borrow outlives the owner (Exercise 1's failure mode).** $a$'s death is fixed by something outside anyone's control at the use site — `buf`'s frame pops at `return`, deterministically — and the borrow $p \xrightarrow{} a$ is used *after* that fixed point. Formally: $\text{death}(a)$ is reached, then $t$ (the use) occurs later. The bug is in the *borrower's* scheduling.

2. **The owner dies prematurely while a borrow is still live (the `realloc`/`free`-while-aliased failure mode from Exercise 6/3).** Here $t$ (the intended use) was scheduled validly at borrow-creation time, but something *shortens* $L(a)$ out from under it — an explicit `free(a)` call, or `realloc` moving the storage (which is a death-of-old-block-plus-birth-of-new-block, even though the pointer *variable* name doesn't change). Formally: $\text{death}(a)$ moves earlier than originally planned, overtaking a $t$ that was fixed. The bug is in the *owner's* scheduling.

Both collapse to the identical formal violation $t \geq \text{death}(a)$ — the taxonomy ("did the borrow move late, or did death move early") is a causal-narrative distinction useful for debugging, not a distinction the invariant itself needs to make.

## Why this differs from "uninitialized pointer" — worth separating cleanly

An uninitialized `char *p;` (never assigned) has an *arbitrary* bit pattern — there is no $a \in \mathrm{Alloc}$ such that $p$ was ever validly derived from $a$'s address at all; it's not that $a$ died, it's that the reference relation $p \xrightarrow{t} a$ was **never established** for any $a$ in the first place. This is sometimes called a "wild pointer" to distinguish it from dangling: dangling requires a *genuine prior* borrow relationship that then outlived its referent; wild has no such history to point to. Both are unsafe to dereference, but for formally distinct reasons — one is a temporal-ordering violation on an otherwise-legitimate edge, the other is an edge that was fabricated with no corresponding node.