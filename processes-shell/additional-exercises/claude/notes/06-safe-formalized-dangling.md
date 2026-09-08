Close, but let's be precise about what Curry-Howard actually buys you here versus what you're informally reaching for — I think you're overloading "exists" as bare existential quantification when what you actually want is a **dependent** existential, and that distinction is the whole content of the formalization.

## Where your formulation slips

$(p, t, \text{False})$ as a witness of "exists" doesn't quite parse: an existential $\exists x. \phi(x)$ under Curry-Howard corresponds to a **dependent pair** $(x, \text{proof of } \phi(x))$ — the second component must be an actual proof *of the predicate applied to the first component*, not a bare truth-value token. "False" isn't a proof of anything; it's the statement that no proof exists. So writing the dangling triple as ending in a literal `False` conflates "the invariant fails here" with "here is evidence the invariant fails," which are different Curry-Howard citizens: the first is a *type* (a proposition), the second is a *term inhabiting its negation*.

## Setting it up correctly

Let $\mathrm{Alloc}$, $L(a)$, and the borrow relation $p \xrightarrow{t} a$ be as before. Define the **safety predicate at a point** as a family of propositions indexed by $(p, a, t)$:

$$
\mathrm{Safe}(p, a, t) \;:\equiv\; t \in L(a)
$$

This is a proposition (a type, under Curry-Howard), not a boolean value — it either has an inhabitant (a proof) or is empty. Your triple's third slot shouldn't hold `True`/`False`; it should hold *an element of this type*, or the type should be empty. Then:

$$
\mathrm{Dangling}(p,a,t) \;:\equiv\; \big(p \xrightarrow{t} a\big) \times \neg\,\mathrm{Safe}(p,a,t)
$$

where $\times$ is the product type (conjunction) and $\neg \phi :\equiv \phi \to \bot$ under Curry-Howard — negation is "a function from proofs of $\phi$ to the empty type," i.e. "$\phi$'s proof type is uninhabited." So a dangling borrow is a **pair**: (a proof that the borrow edge exists) $\times$ (a proof that no proof of $t \in L(a)$ can exist — concretely here, a proof that $t \geq \text{death}(a)$, which directly refutes membership in the half-open interval). That refutation is not "False" as an atomic value — it's itself a constructive argument, e.g. a term of type $t \geq \text{death}(a)$ combined with the interval definition to derive $t \in L(a) \to \bot$.

## Why the dependent-pair framing is the point, not pedantry

This connects back to something you asked much earlier in this thread and is worth closing the loop on explicitly: this is exactly the "parse, don't validate" pattern again, one level up in generality. $\mathrm{Safe}$ as a bare boolean predicate is the *validate* style — you get a yes/no and the "yes" evidence is thrown away, forcing re-derivation downstream. $\mathrm{Safe}$ as a Curry-Howard proposition, where a well-typed program can only construct a borrow *term* by simultaneously constructing a *proof term* of $t \in L(a)$, is the *parse* style — this is, concretely, what Rust's borrow checker is: a compile-time search for an inhabitant of $\mathrm{Safe}(p,a,t)$ for every borrow in the program, rejecting compilation when no such inhabitant can be constructed. "Dangling borrow" in Rust is a compile error precisely because the *type* $\neg\,\mathrm{Safe}(p,a,t)$ was proven inhabited (or $\mathrm{Safe}$'s inhabitant couldn't be found) at compile time, rather than the failure being a runtime fact discovered by a segfault, as it is in C — where the type-level machinery to even *ask* the question doesn't exist, so $\mathrm{Dangling}$ is a proposition about program behavior we're reasoning about externally, never one the compiler itself checks.