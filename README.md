# DMultiSets

# Computational Verification of the Combinatorial Hypothesis

For a given multiset of natural numbers \(A\), we denote
\[
\sum A = \sum_{a \in A} a.
\]
For example, if
\[
A = \{1, 2, 2, 2, 10, 10\},
\]
then
\[
\sum A = 27.
\]
For two multisets, we write \(A \supseteq B\) if every element appears in \(A\) at least as many times as it does in \(B\). For the purposes of this project, let us adopt the following definitions.

## Definition: \(d\)-Bounded Multiset

A multiset \(A\) is called **\(d\)-bounded** (for a natural number \(d\)) if it is finite and all its elements belong to \(\{1, 2, \dots, d\}\) (allowing repetitions).

## Definition: Non-Controversial Pair

A pair of \(d\)-bounded multisets \(A, B\) is called **non-controversial** if for all \(A' \subseteq A\) and \(B' \subseteq B\) the following equivalence holds:
\[
\sum A' = \sum B' \iff \bigl(A' = B' = \emptyset\bigr) \quad \text{or} \quad \bigl(A' = A \text{ and } B' = B\bigr).
\]
In other words, although \(\sum A = \sum B\), the sums of any non-trivial (i.e., non-empty and not equal to the whole set) subsets of \(A\) and \(B\) must be different.

## Problem Statement

For a fixed \(d \ge 3\) (we will not consider smaller \(d\)) and given multisets \(A_0, B_0\), we wish to find non-controversial \(d\)-bounded multisets \(A \supseteq A_0\) and \(B \supseteq B_0\) that maximize the value \(\sum A\) (equivalently, \(\sum B\)). We denote this maximum value by
\[
\alpha(d, A_0, B_0).
\]
We define \(\alpha(d, A_0, B_0) = 0\) if \(A_0\) and \(B_0\) are not \(d\)-bounded or if they do not have any \(d\)-bounded non-controversial supersets.

### Example 1

\[
\alpha(d, \emptyset, \emptyset) \ge d(d-1).
\]

**Proof Sketch:** Consider the multisets:
- \(A = \{d, d, \dots, d\}\) (with \(d\) repeated \(d-1\) times),
- \(B = \{d-1, d-1, \dots, d-1\}\) (with \(d-1\) repeated \(d\) times).

These sets satisfy the conditions since
\[
\sum A = d(d-1) = \sum B.
\]

### Example 2

\[
\alpha(d, \emptyset, \{1\}) \ge (d-1)^2.
\]

**Proof Sketch:** Consider the multisets:
- \(A = \{1, d, d, \dots, d\}\) (with \(d\) repeated \(d-2\) times),
- \(B = \{d-1, d-1, \dots, d-1\}\) (with \(d-1\) repeated \(d-1\) times).

These sets satisfy the conditions since
\[
\sum A = 1 + d(d-2) = (d-1)^2 = \sum B.
\]

It can be proven that these examples are optimal, i.e.,
\[
\alpha(d, \emptyset, \emptyset) = d(d-1)
\]
and
\[
\alpha(d, \emptyset, \{1\}) = (d-1)^2.
\]

Nevertheless, in this project we aim to verify these results computationally for as large values of \(d\) as possible, and also to compute the values of \(\alpha\) for other prescribed multisets \(A_0\) and \(B_0\).

