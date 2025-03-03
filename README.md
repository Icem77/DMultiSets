# DMultiSets

# Computational Verification of a Combinatorial Hypothesis

This project aims to computationally verify a combinatorial hypothesis concerning multisets of natural numbers. It blends theoretical combinatorial analysis with practical computation to explore optimal constructions under specific sum constraints.

## Overview

Given a multiset of natural numbers A, we define its sum as:  
**∑A = ∑₍ₐ ∈ A₎ a**.  
For example, if A = {1, 2, 2, 2, 10, 10}, then **∑A = 27**. For two multisets, we write **A ⊇ B** if every element appears in A at least as many times as in B.

## Definitions

- **d-Bounded Multiset:**  
  A multiset A is called *d-bounded* for a natural number d if it is finite and every element of A belongs to the set {1, …, d} (allowing repetitions).

- **Undisputed Multisets:**  
  A pair of d-bounded multisets A and B is called *undisputed* if for every A′ ⊆ A and B′ ⊆ B, the equality **∑A′ = ∑B′** holds **if and only if** either  
  - A′ = B′ = ∅ (both subsets are empty), **or**  
  - A′ = A and B′ = B (both subsets are the entire multisets).  

  In other words, although **∑A = ∑B**, the sums of any non-empty proper sub-multisets of A and B must be different.

## Problem Statement

For a fixed **d ≥ 3** (values smaller than 3 are not considered) and given initial multisets A₀ and B₀, the goal is to find undisputed d-bounded multisets **A ⊇ A₀** and **B ⊇ B₀** that maximize the value of **∑A** (equivalently, **∑B**). We denote this maximal value by **α(d, A₀, B₀)**. By definition, if A₀ and B₀ are not d-bounded or if there exist no undisputed d-bounded supersets, then **α(d, A₀, B₀) = 0**.

## Examples

### Example 1: α(d, ∅, ∅) ≥ d(d − 1)

**Proof Sketch:**  
- Let **A = { d, d, …, d }** (with d − 1 occurrences).  
- Let **B = { d − 1, d − 1, …, d − 1 }** (with d occurrences).

These multisets satisfy the conditions with **∑A = d(d − 1) = ∑B**.

### Example 2: α(d, ∅, {1}) ≥ (d − 1)²

**Proof Sketch:**  
- Let **A = { 1, d, d, …, d }** (with one occurrence of 1 and d − 2 occurrences of d).  
- Let **B = { d − 1, d − 1, …, d − 1 }** (with d − 1 occurrences).

These multisets satisfy the conditions with **∑A = 1 + d(d − 2) = (d − 1)² = ∑B**.

It can be proven that the above examples are optimal, meaning that:  
- **α(d, ∅, ∅) = d(d − 1)**  
- **α(d, ∅, {1}) = (d − 1)²**

## Project Goals

- **Computational Verification:**  
  Verify the combinatorial hypothesis computationally for as large values of d as possible.

- **Generalization:**  
  Compute the values of **α** for other specified initial multisets A₀ and B₀.

## Conclusion

This project combines combinatorial theory with computational methods to rigorously verify properties of multisets under sum constraints. By exploring optimal constructions of undisputed multisets, it provides both theoretical insights and practical verification of the hypothesis.

Happy computing!
