## "Parse, don't validate" as an algebraic statement

**Setup — name the two objects first, since the whole distinction is about which object a function lands in.**

Let $A$ be the raw representation — in your shell, $A = (\Sigma^*)^*$, arbitrary token arrays, no shape guarantees. Let $B$ be the semantic domain you actually want to compute with — your `Command` type, a genuine **coproduct** (disjoint sum) of variants:

$$
B \;=\; \text{Exit} \;+\; \text{Cd}(\Sigma^*) \;+\; \text{Path}(\Sigma^{*m}) \;+\; \text{External}(\Sigma^*,\Sigma^{*k})
$$

Here $+$ is the coproduct: an element of $B$ is tagged as belonging to exactly one summand, and — this is the load-bearing fact — **each summand's own domain is already the arity-correct shape.** `Cd` isn't decorated with "$\Sigma^*$, but check the length is 1 before use"; its domain literally *is* $\Sigma^*$, a single string. There is no term of $B$ representing "cd with two arguments," the same way there's no natural number representing "3 but negative."

### Validate, formalized

Validation is a **predicate**, i.e. a function into the two-element set (booleans):

$$
\text{valid} : A \to \mathbf{2}
$$

This defines a **subobject** of $A$ via pullback along $\text{true} : 1 \to \mathbf{2}$:

$$
\begin{array}{ccc}
A_{\text{valid}} & \rightarrowtail & A \\
\downarrow & & \downarrow{\scriptstyle\text{valid}} \\
1 & \xrightarrow{\ \text{true}\ } & \mathbf{2}
\end{array}
$$

The monomorphism $A_{\text{valid}} \rightarrowtail A$ is real, mathematically — but notice its codomain is still $A$. After you call `valid(a)` and get back "true," what you're *holding in your hand* is still a value of type $A$ (a raw token array), plus a fact that happens to be true about it, floating separately, unattached to the type. Every downstream function that consumes this value has signature $A \to \dots$, not $A_{\text{valid}} \to \dots$ — because C (and most languages) has no way to write $A_{\text{valid}}$ as a distinct type unless you use a dependent/refinement type $\{a : A \mid \text{valid}(a)\}$. Without that, the proof is discarded the instant the `if` statement's branch ends, and anyone downstream must re-derive it (re-check the arity) to use it safely. That re-derivation is exactly the redundant defensive check I flagged in `handle_chdir` earlier.

### Parse, formalized

Parsing is a **partial function into $B$**, expressed totally as a map into the coproduct of $B$ with a unit object $1$ (the error case):

$$
\text{parse} : A \to 1 + B
$$

This is a Kleisli arrow for the Maybe/Option monad — nothing more exotic than that. The key structural fact is that $B$ **embeds into $A$** via some injection $\iota : B \rightarrowtail A$ (every `Command` can be rendered back as some token array), and $\text{valid}$ **factors through** $\text{parse}$:

$$
\text{valid} \;=\; \text{isRight} \circ (\mathrm{id}_1 + \iota)^{-1}\text{-detecting} \circ \text{parse}
$$

More plainly: if you know how to `parse`, you get `valid` for free by just checking which side of the coproduct you landed on. The converse fails — knowing `valid(a) = true` does **not** hand you a term of $B$; you'd have to run essentially the same case-analysis work again to reconstruct it. This asymmetry — parse subsumes validate, not vice versa — is the entire content of the slogan. It's a **strict factorization**, one direction only.

### Why this kills arity checks downstream — the universal property

By the universal property of the coproduct, defining a total function $\text{execute} : B \to \text{Result}$ is *equivalent* to independently defining one function per summand:

$$
\text{execute} \;\cong\; [\,\text{execute}_{\text{exit}},\ \text{execute}_{\text{cd}},\ \text{execute}_{\text{path}},\ \text{execute}_{\text{ext}}\,]
$$

where $\text{execute}_{\text{cd}} : \Sigma^* \to \text{Result}$ takes **one string**, full stop — the universal property is precisely what guarantees each branch's domain matches its summand exactly, with no leftover cases to guard against. There is no fifth branch for "`Cd` with zero or two arguments" because the coproduct has no such summand for `execute` to be defined on. The arity check didn't get deleted — it got **moved once**, into `parse`'s construction of the coproduct injection, and then it's a mathematical impossibility for it to need re-checking, rather than a matter of programmer discipline to remember to re-check it.

That's the whole algebra: *validate* is a predicate you must remember to re-consult; *parse* is an injection into a type that makes the alternative simply not exist.