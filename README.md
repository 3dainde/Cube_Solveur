# Cube — Modular 3D Cube Solver R&D

Research and development for a modular, deterministic 3D cube system built
around interconnected rooms, procedural routes, and an independent puzzle
solver for Unreal Engine 5.8 ~ 6.0.

> **Core principle:** the seed must generate a solvable problem, not only a
> visually interesting level.

## Project status

This repository is in the design and pre-implementation stage. The current
priority is the logical model, deterministic generation, route construction,
solver validation, and seed analysis. Unreal Actors, final room meshes, and
production headers are intentionally kept separate from the solver design.

## Goals

The system is intended to:

- Generate a complete cube layout deterministically from a seed.
- Represent the cube as a logical 3D graph of rooms.
- Build a guaranteed route from `START` to `EXIT`.
- Add controlled branches, decoy paths, clues, and hazards.
- Validate every generated seed automatically.
- Support multiple difficulty profiles, including `EASY` and `IMPOSSIBLE`.
- Stream only the rooms near the player while keeping the full logical graph
  available to the solver.
- Reproduce exactly the same configuration from the same seed.
- Provide a foundation for selecting a diverse set of official seeds.

The visual direction is geometric, repetitive, and disorienting. It is an
original modular-cube system and does not reproduce protected film elements.

## Architecture

The project is split into a logical generation layer and an Unreal
representation layer:

```text
Seed
  |
  v
Deterministic generators
  |
  +--> Logical grid / room graph
  +--> Guaranteed route
  +--> Room archetypes, clues, hazards, and state rules
  |
  v
Independent solver
  |
  v
Validation and world manifest
  |
  v
UE5.8 world builder and local streaming
  |
  v
Spawned rooms near the player
```

The solver never depends on currently spawned Actors, meshes, collision, level
of detail, animation, lighting, or physical transforms. Unreal translates the
validated logical manifest into a playable world.

## Logical cube model

The initial production target is a `64 × 64 × 64` logical grid:

```text
64³ = 262,144 potential cells
```

The design must remain configurable for smaller prototypes and future
extensions such as `32³`, `96³`, or `128³`. Grid dimensions must not be
hard-coded into individual systems.

Each room has a permanent logical coordinate:

```text
L = (X, Y, Z)
```

with:

```text
0 <= X < SizeX
0 <= Y < SizeY
0 <= Z < SizeZ
```

A stable room identifier can be encoded from the coordinate:

```text
RoomID = X + Y * SizeX + Z * SizeX * SizeY
```

The identifier is logical and deterministic. It must not depend on an Actor
name, memory address, or the room's current physical transform.

Each room can connect in six directions:

```text
+X  East       -X  West
+Y  North      -Y  South
+Z  Top        -Z  Bottom
```

A direct connection is valid only between Manhattan-neighbouring cells:

```text
abs(dx) + abs(dy) + abs(dz) == 1
```

## Room data

A logical room contains gameplay and generation data, not presentation-only
state:

```text
Room
├── RoomID and logical coordinate
├── Room archetype and orientation
├── Connections[6]
├── Required entry and allowed exits
├── Route and checkpoint metadata
├── Clue data
├── Hazard data
├── Difficulty profile
├── Decoy / critical-path flags
└── State rules
```

Presentation data is resolved later into:

```text
World transform, rotation, mesh, collision, FX, audio, and lighting
```

Room archetypes may include normal rooms, checkpoints, clue rooms, hazard
rooms, branches, decoys, and special rooms that modify puzzle rules.

## Deterministic generation

Every procedural decision must be reproducible. A single shared random stream
is discouraged because changing one generator would shift every later result.
Instead, derive independent sub-seeds from the master seed:

```text
S_PATH    = Hash(Seed, "PATH")
S_ROOMS   = Hash(Seed, "ROOMS")
S_CLUES   = Hash(Seed, "CLUES")
S_HAZARDS = Hash(Seed, "HAZARDS")
S_LAYOUT  = Hash(Seed, "LAYOUT")
```

This keeps generation stable when one subsystem changes and makes failures
easier to reproduce.

## Route generation

The route is a design object, not an accidental by-product of random room
placement.

The generation pipeline first creates a safe `START → EXIT` route. It can
then enrich the graph with controlled branches and false paths while
preserving solvability and the intended difficulty.

Two route representations are useful:

- **Abstract route:** the ordered logical rooms and constraints that define the
  puzzle.
- **Physical route:** the room archetypes, orientations, sockets, clues, and
  hazards placed into the Unreal world.

The abstract route is authoritative. Physical layout must never silently
change the logical solution.

## Solver design

### Solver V0 — static graph search

The first solver version validates a static graph. It checks connectivity
between the start and exit, traverses only valid six-direction connections,
and records the discovered route.

Breadth-first search (BFS) is the baseline because it is deterministic,
simple to test, and produces a shortest path in an unweighted graph.

Conceptual algorithm:

```text
queue := [start]
visited := {start}
previous := {}

while queue is not empty:
    current := queue.pop_front()

    if current == exit:
        reconstruct route using previous
        return SOLVED

    for neighbour in valid neighbours(current):
        if neighbour is not visited:
            visited.add(neighbour)
            previous[neighbour] = current
            queue.push_back(neighbour)

return UNSOLVABLE
```

### Solver V1 — state-aware search

The production solver must search more than a room coordinate. Its state may
include:

```text
(RoomID, RouteContext, Inventory, Flags, RuleState)
```

This is required when clues, checkpoints, room effects, locked exits, or
special rooms change which moves are legal. Two visits to the same room may
therefore be different solver states.

The solver remains independent from Unreal rendering and should be usable in
headless tests and seed-generation tools.

## Generation and validation passes

Generation is intentionally divided into passes:

### Pass A — structural generation

1. Create the grid and stable room identifiers.
2. Select start and exit boundaries.
3. Build the guaranteed route.
4. Add legal graph connections and required constraints.
5. Run the solver.

### Pass B — enrichment

1. Assign room archetypes.
2. Add clues and route context.
3. Add hazards and controlled decoy branches.
4. Apply the selected difficulty profile.
5. Run the solver again.
6. Check for exploits and invalid shortcuts.
7. Emit the validated world manifest.

If a seed fails validation, it must be rejected or regenerated. Validation must
not be replaced with a silent fallback.

## Validation criteria

A candidate seed should be rejected when it has any of the following:

- No valid route from `START` to `EXIT`.
- A route that violates room, socket, or state rules.
- An unintended shortcut that bypasses required puzzle constraints.
- A critical-path room with incompatible entry or exit data.
- A hazard configuration that makes the intended route impossible.
- Duplicate or insufficiently diverse structure when compared with accepted
  seeds.
- A physical manifest that does not match the logical graph.

Useful R&D metrics include route length, number and depth of branches,
solution count, clue density, hazard density, state-space size, and solver
time.

## Difficulty profiles

The same logical framework can expose different player experiences:

- **Easy:** clearer clues, safer branches, fewer misleading routes, and more
  forgiving state rules.
- **Impossible:** ambiguous clues, deeper decoys, stricter state transitions,
  and more demanding route interpretation.

Difficulty should be generated as data and validated by the solver, not
implemented as visual decoration alone.

## Unreal Engine integration

Each physical room should expose six matching sockets:

```text
Socket_East
Socket_West
Socket_North
Socket_South
Socket_Top
Socket_Bottom
```

The world builder consumes the validated manifest and maps logical neighbours
to compatible room sockets. The stream manager loads only the local region
around the player. The complete logical graph remains available without
requiring every room to be spawned as an Actor.

Recommended responsibilities:

| Component | Responsibility |
| --- | --- |
| Grid / graph model | Coordinates, IDs, neighbours, connections |
| Route generator | Guaranteed route and controlled branches |
| Room generator | Archetypes and room attributes |
| Clue generator | Clues and route context |
| Hazard generator | Hazards consistent with rules |
| Solver | Reachability, state search, and solution reconstruction |
| Validator | Seed acceptance, exploit detection, and metrics |
| World builder | Logical manifest to UE5 room instances |
| Stream manager | Local room loading and unloading |

## Development roadmap

1. Implement configurable coordinates, directions, and stable room IDs.
2. Build a small logical grid and neighbour validation.
3. Generate and serialize a guaranteed route.
4. Implement and test solver V0 with BFS.
5. Add route context and state-aware solver V1.
6. Add room archetypes, clues, hazards, and controlled decoys.
7. Add seed validation, exploit detection, and diversity metrics.
8. Produce a world manifest for Unreal.
9. Integrate sockets, room spawning, and local streaming.
10. Compare seed families and select the official seed set.
11. Profile memory, generation time, solver time, and streaming behavior.

## Testing strategy

The solver and generator should be testable without launching Unreal. Important
tests include:

- Coordinate-to-`RoomID` encoding and decoding.
- Six-direction neighbour rules.
- Deterministic output for identical seeds.
- Different output for different seeds.
- Guaranteed route reachability.
- Correct route reconstruction.
- State transitions and special-room rules.
- Rejection of disconnected or exploited seeds.
- Agreement between the logical graph and world manifest.
- Stable serialization and replay from a saved seed.

## Scope boundaries

The following are intentionally outside the first solver milestone:

- Final art direction and materials.
- Complete room mesh production.
- Audio and cinematic polish.
- Multiplayer synchronization.
- Final UI and player onboarding.
- Large-scale performance tuning before the logical contract is stable.

## Related design documents

The detailed R&D specifications are:

- `Cube_RnD_Solver_Seeds_V0.2.md`
- `Cube_UE5.8_Guide_Grid_Solver_Seeds_v0.3.md`

These documents describe the intended grid, sockets, routes, seed families,
streaming model, solver versions, and validation pipeline in greater detail.

## License

No open-source license has been selected for this project yet. Until a license
is added, all rights remain reserved by the copyright holder. Do not assume
that public repository access grants permission to reuse, redistribute, or
commercialize the code or design.
