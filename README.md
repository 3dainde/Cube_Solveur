# Cube — Procedural Puzzle Solver R&D

Research and development around a deterministic, graph-based procedural
generation and validation pipeline for Unreal Engine 5.8 ~ 6.0. The project explores
how to generate large structured spaces from a seed and verify their
consistency independently from rendering.

## Project status

Design and pre-implementation stage. Work currently focuses on the internal
data model, generation pipeline, and an independent validation layer. Engine
integration (Actors, meshes, streaming) follows once the core logic is
stable.

## Core principle

> A seed must produce a verifiable, reproducible structure, not just a visual
> layout.

Generation and verification are treated as two separate concerns: one system
builds a candidate structure, another checks it independently before it is
allowed to reach Unreal.

## High-level architecture

```text
Seed
  |
  v
Deterministic generation layer
  |
  v
Independent validation layer
  |
  v
Engine-facing manifest
  |
  v
Unreal Engine 5.8 (instancing / streaming)
```

The validation layer has no dependency on spawned Actors, meshes, collision,
animation, or rendering state. It operates purely on internal data so results
stay reproducible across runs and platforms.

## Design principles

- **Determinism first.** The same seed must always produce the same
  validated result. No system may depend on non-deterministic ordering,
  timing, or floating-point drift across platforms.
- **Separation of concerns.** Generation, validation, and presentation are
  isolated layers with narrow interfaces. Presentation state never feeds back
  into generation or validation logic.
- **Independent verification.** Every generated candidate is checked by a
  dedicated validation component before it is considered usable, rather than
  being trusted by construction.
- **Engine-agnostic core.** The generation and validation logic is written to
  be testable and runnable without launching the engine, so it can be
  exercised in isolated builds and automated checks.
- **Scalability.** The internal representation must support scaling the
  underlying structure up or down without hard-coded assumptions baked into
  individual components.

## Repository layout

```text
Source/     Engine-side and gameplay integration code
Content/    Unreal assets
Config/     Project configuration
Manuals/    Internal documentation and planning notes
```

Detailed design specifications for this project are kept outside this README
and are not part of the public-facing documentation.

## Status of the toolchain

The generation and validation logic is being developed incrementally, with
automated checks used to confirm structural consistency before any engine
integration work begins. Further technical details will be documented as the
implementation matures.

## License

No open-source license has been selected for this project yet. Until a
license is added, all rights remain reserved by the copyright holder. Public
repository access does not grant permission to reuse, redistribute, or
commercialize the code or design.
