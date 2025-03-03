# DMultiSets

# Computational Verification of a Combinatorial Hypothesis 🧮

This project aims to computationally verify a combinatorial hypothesis concerning multisets of natural numbers. It blends theoretical combinatorial analysis with practical computation to explore optimal constructions under specific sum constraints.

## Overview 🧮

Given a multiset of natural numbers A, we define its sum as:  
**∑A = ∑₍ₐ ∈ A₎ a**.  
For two multisets, we write **A ⊇ B** if every element appears in A at least as many times as in B.

## Definitions 🧮

- **d-Bounded Multiset:**  
  A multiset A is called *d-bounded* for a natural number d if it is finite and every element of A belongs to the set {1, …, d} (allowing repetitions).

- **Undisputed Multisets:**  
  A pair of d-bounded multisets A and B is called *undisputed* if for every A′ ⊆ A and B′ ⊆ B, the equality **∑A′ = ∑B′** holds **if and only if** either  
  - A′ = B′ = ∅ (both subsets are empty), **or**  
  - A′ = A and B′ = B (both subsets are the entire multisets).  

  In other words, although **∑A = ∑B**, the sums of any non-empty proper sub-multisets of A and B must be different.

## Problem Statement 🧮

For a fixed **d ≥ 3** (values smaller than 3 are not considered) and given initial multisets A₀ and B₀, the goal is to find undisputed d-bounded multisets **A ⊇ A₀** and **B ⊇ B₀** that maximize the value of **∑A** (equivalently, **∑B**). We denote this maximal value by **α(d, A₀, B₀)**. By definition, if A₀ and B₀ are not d-bounded or if there exist no undisputed d-bounded supersets, then **α(d, A₀, B₀) = 0**.

## Hypothesis 🧮

It can be proven that some cases have optimal solutions given by the formula, for example:  
- **α(d, ∅, ∅) = d(d − 1)**  
- **α(d, ∅, {1}) = (d − 1)²**

## Project Goals 🧮

- **Computational Verification:**  
  Verify the combinatorial hypothesis computationally for as large values of d as possible.

- **Generalization:**  
  Compute the values of **α** for other specified initial multisets A₀ and B₀.

# Task Description 🧮

The task is to write two additional implementations of the same computation:
- A non-recursive (but still single-threaded) version.
- A parallel (multi-threaded) version that achieves the best possible scalability with number of threads.

## Objective 🧮

The goal of this task is **not** to conduct a theoretical analysis of the mathematical problem or perform micro-optimizations on bit-level operations; the implementations for operations on multisets are provided.

## Parallelization Challenges 🧮

- **Unpredictable Workload Distribution:**  
  It is difficult to predict which branches of recursion will be the most computationally intensive, making it impossible to divide the work evenly in advance.

- **Synchronization Overhead:**  
  Approaches that synchronize with other threads on every iteration would be too slow.

- **Heuristic Approach:**  
  We adopt a heuristic strategy where threads try to split the work at the beginning of computations, and then try to take new subtask from the common pool every time they finish
  one.

## Input 🧮

The program reads three lines from standard input:

- **First Line:** Contains four numbers: **t**, **d**, **n**, and **m**, which represent:
  - **t:** The number of auxiliary threads.
  - **d:** The parameter d.
  - **n:** The number of forced elements in **A₀**.
  - **m:** The number of forced elements in **B₀**.

- **Second Line:** Contains **n** numbers from the interval 1 to **d**: these are the elements of the multiset **A₀**.

- **Third Line:** Contains **m** numbers from the interval 1 to **d**: these are the elements of the multiset **B₀**.

The task is to compute **α(d, A₀, B₀)** using **t** auxiliary threads. (It is allowed to exclude the main thread from **t** if no calculations from `sumset.h` are performed in it.)

## Output 🧮

The output should display the solution **A, B** (i.e., the undisputed d-bounded multisets **A ⊇ A₀** and **B ⊇ B₀** that maximize **∑A = ∑B**):

- **First Line:** Output a single number representing **∑A**.
- **Second Line:** Output the description of the multiset **A**.
- **Third Line:** Output the description of the multiset **B**.

The multiset description consists of listing the elements along with their multiplicities, separated by single spaces.

In the case that there is no solution, the output should be a zero sum and two empty multisets, i.e.: "0\n\n\n"
