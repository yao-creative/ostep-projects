Close in shape, but "relation of relations" overstates the nesting — it's cleaner (and more accurate) to say you have **one indexed family of two relations**, both living over the same product $\mathbf{T} \times \mathrm{Loc} \times \mathrm{Alloc}$, distinguished by a constraint on cardinality. Let me be precise about where your formula needs adjusting.

## Fixing the formula

You wrote $\mathbf{T} \times (\mathrm{Loc} \times \mathrm{Alloc})$ — that's a **set**, the domain candidate, not yet a relation. A relation is a *subset* of that product (or, for ownership specifically, a subset satisfying a functional constraint). So:

$$
\mathrm{Borrow} \subseteq \mathbf{T} \times \mathrm{Loc} \times \mathrm{Alloc}
$$

is the general reference-relation — a triple $(t, l, a) \in \mathrm{Borrow}$ means "at time $t$, location $l$ holds an address inside allocation $a$." This single set is the base object everything else is carved out of.

**Ownership is not a different relation sitting "on top of" this one** — it's the **same relation**, restricted by an extra axiom that forces single-valuedness in its second coordinate, for fixed $t$:

$$
\mathrm{Owner} := \big\{ (t,l,a) \in \mathrm{Borrow} \;\big|\; \forall l'.\ (t,l',a) \in \mathrm{Borrow} \wedge \text{"$l'$ is designated"} \implies l' = l \big\}
$$

More simply, phrased the way it's most useful to actually use: slice $\mathrm{Borrow}$ at a fixed time $t$ to get a relation $\mathrm{Borrow}_t \subseteq \mathrm{Loc} \times \mathrm{Alloc}$ (this is the standard **currying** move — $\mathbf{T} \times \mathrm{Loc} \times \mathrm{Alloc} \cong \mathbf{T} \to \mathcal{P}(\mathrm{Loc} \times \mathrm{Alloc})$, treating time as indexing a *family* of relations rather than a third tuple-slot sitting flat alongside the other two). Then:

$$
\mathrm{owner}_t : \mathrm{Alloc} \rightharpoonup \mathrm{Loc} \quad\text{is a function} \quad\Longleftrightarrow\quad \mathrm{Borrow}_t^{-1} \text{ restricted to owning-locations is single-valued}
$$

So the structure is: **one relation, sliced by time into a family, and within each slice, one designated element per allocation is elevated to function-status (owner) while the rest remain relation-status (borrowers).** Not two separate relations nested inside each other — one relation, with a distinguished sub-selection.

## Why "relation of relations" is the wrong shape, specifically

A genuine relation-of-relations would mean elements of the outer relation are *themselves* relations — e.g., something in $\mathcal{P}(\mathrm{Loc}\times\mathrm{Alloc}) \times \mathcal{P}(\mathrm{Loc}\times\mathrm{Alloc})$, comparing two whole snapshots to each other (which is actually a fair description of something else you touched on earlier — comparing $\mathrm{Borrow}_{t_1}$ against $\mathrm{Borrow}_{t_2}$ to detect *transfer*, i.e. "did ownership change hands between these two snapshots" is genuinely a relation *between* two relations). But the base object itself — "does this pointer, at this time, point into this block" — is a first-order ternary relation, full stop. Ownership isn't a second layer of relation-ness stacked on it; it's a **property of the first relation's fibers** (how many locations map to a given allocation in that slice), which is a much lighter-weight kind of structure than "relation of relations" implies — you're quantifying over the fiber size, not building a new relation whose elements are relations.

$$
\underbrace{\mathrm{Borrow} \subseteq \mathbf{T}\times\mathrm{Loc}\times\mathrm{Alloc}}_{\text{one ternary relation, the whole aliasing graph over time}} \quad\supset\quad \underbrace{\mathrm{Owner}}_{\substack{\text{the sub-relation whose fibers}\\ \text{over } (t,a) \text{ have size} \le 1}}
$$

That's the exact formal shape: not nesting, but a global relation with a local cardinality constraint carving out a distinguished functional part of itself.