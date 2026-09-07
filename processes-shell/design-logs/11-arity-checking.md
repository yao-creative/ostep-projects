## Formalizing the arity check

**Setup — the invariant you're actually leaning on.** `lex_line` guarantees `tokens` is **NULL-terminated with no NULLs before the terminator** (every non-empty `strsep` result is a real string; the terminator is placed exactly once, at the end). Define the indicator sequence

$$
b_i = [\,\text{tokens}[i] \neq \text{NULL}\,] \in \{0,1\}, \qquad i = 0, 1, 2, \dots
$$

The invariant says exactly this: $b$ is **monotone non-increasing** — $b_0 \geq b_1 \geq b_2 \geq \cdots$ — because once you hit the terminator, every position after it is also past-the-end (conceptually; you never legally read past it, but the *logical* structure is "all 1s then all 0s forever"). A monotone $\{0,1\}$-sequence has exactly one degree of freedom: its **falling edge**, the unique index $n$ where $b_{n-1} = 1$ and $b_n = 0$. That $n$ *is* the length of the token list. This is the formal content of "NULL-terminated array $\cong$ finite list" — the whole infinite tail collapses to one number.

**Why two probes fully determine membership in $\{n = k\}$.** Because $b$ is monotone, testing equality against a target $k$ doesn't require checking $b$ everywhere — it only requires checking the two positions that *bracket* $k$:

$$
n = k \iff b_{k-1} = 1 \ \wedge\ b_k = 0 \qquad (k \geq 1)
$$

This is a **trichotomy**, not a binary check: the complement of $\{n=k\}$ in $\mathbb{N}$ splits as $\{n < k\} \sqcup \{n > k\}$, and monotonicity says $\{n<k\} \iff b_{k-1}=0$ while $\{n>k\} \iff b_k = 1$ — so the *same two bits* that confirm equality also fully classify both failure modes, with no case left unaccounted for. That's your intuition about "the complement space is smaller" made precise: it's not smaller because you got lucky, it's smaller because monotonicity means $\mathbb{N}\setminus\{k\}$ is fully characterized by two point-samples rather than requiring you to rule out infinitely many values individually.

For `cd`, the target is $k=2$ (command name occupies slot 0, one argument occupies slot 1, so a valid `cd` has total length $n=2$):

$$
n = 2 \iff \underbrace{\text{tokens[1]} \neq \text{NULL}}_{b_1 = 1} \ \wedge\ \underbrace{\text{tokens[2]} = \text{NULL}}_{b_2 = 0}
$$

and your rejection branch is exactly De Morgan's law applied to that conjunction: $\lnot(b_1 \wedge \lnot b_2) = \lnot b_1 \vee b_2$, i.e. `tokens[1] == NULL || tokens[2] != NULL`. One more safety fact worth stating explicitly: C's `||` **short-circuits**, so `tokens[2]` is only dereferenced when `tokens[1] != NULL` is already known — which, combined with the NULL-termination invariant, guarantees slot 2 is a legally-allocated read (the array always has room for at least one more slot past any non-NULL entry, since something has to terminate it). The safety of the bounds access and the correctness of the arity logic are proven by the *same* fact about the data.

## What this pattern is called

This is **pattern matching on the shape of a list** — specifically, destructuring against the two constructors of the list grammar $\text{List} = \text{Nil} \mid \text{Cons}(x, \text{List})$. In a language with real algebraic data types, writing `case tokens of [x] -> ...` (OCaml/Haskell-style singleton-list pattern) *is* this check — the compiler generates exactly "head exists, tail is `Nil`." What you're doing in C is manually inlining that pattern match via sentinel probes, because C has no native sum types for lists. Equivalently, in automata terms: recognizing "exactly $k$ tokens" is recognizing the regular language $\Sigma^k$, and the two-probe check is the minimal DFA transition test — "in an accepting state after $k$ symbols, and the next symbol is EOF."

## Why you can't `switch` on the command name

`switch` in C requires its controlling expression to have **integral type**, and each `case` label must be a **compile-time constant integer expression**. `char *` is neither: `strcmp(tokens[0], "cd")` is a runtime function call producing an `int`, but the thing you actually want to compare — the *string itself* — isn't an integer at all, and `==` on `char*` would compare pointer identity, not contents, which is meaningless here (two `"cd"` literals in different places aren't guaranteed to be the same address). So string-keyed dispatch is structurally excluded from `switch` in C; you're stuck with an `if`/`else if` chain of `strcmp` calls, or, if you wanted to optimize later, a lookup table of `{name, tag}` pairs searched by loop or `bsearch`. Contrast this with `execute_command`'s `switch (cmd.tag)` — that one works precisely *because* `classify_command` already did the string-matching work and reduced everything down to an `enum`, which **is** an integral type. That's not a coincidence: it's the parse step converting an untyped string-keyed space into a typed integer-keyed space, which is a big part of *why* parsing into a coproduct is worth doing — it's what makes `switch`, and the compiler's exhaustiveness/dispatch-table optimization for it, available to you at all.