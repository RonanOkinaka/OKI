<div align="center">

# OKI: An ECS Framework
</br>
<code>OKI</code> (pronounced "oh-kee") is a header-only library for C++17 inspired by <code>entt</code>, which was created by user skypjack. It implements a similar entity-component-system (ECS) architecture, which is used to organize high-performance, cache-local computations -- such as those used in games or simulations.
</div>

## Goals

This project is a learning opportunity for me. I wanted a deeper understanding of both optimization and library design, so I set out to make a project that could help me practice both.

So, what *is* an ECS architecture? Many programmers have at one point been told to "prefer composition over inheritance." This philosophy lays the groundwork for ECS. The idea is to avoid monolithic classes by separating conceptual models ("entities") from the data that models them ("components") and the logic that operates on them ("systems").

This is a benefit in and of itself, but comes with other welcome merits. This is especially true for performance. Cache locality is of particular concern given the modern memory wall, and an ECS architecture allows behaviors to be implemented in a cache-friendly way by nearly guaranteeing tight loops. One single system iterates in a straight line over every relevant piece of data every time, for (ideally!) excellent performance exhibited by both the instruction and data cache. Moreover, a carefully considered set of components will follow the structure-of-arrays pattern and reap the performance gains that come with it.

## Building

At the moment, this "library" is header-only (due in large part to the heavy use of template classes). 

So, building is easy: `#include "oki/oki_ecs.h"`, use the library, then compile your code.

There are a set of tests using `catch2`, which `CMake` will download and install automatically. So, the steps are similarly simple:
- `cmake -S . -B build`
- `cd build`
- `cmake --build .`
- `ctest`
