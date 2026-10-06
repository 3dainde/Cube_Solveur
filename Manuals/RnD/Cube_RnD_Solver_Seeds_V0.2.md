# CUBE — Dossier R&D
## Génération déterministe, Solveur de parcours et système de Salles

**Projet :** Cube / Unreal Engine 5.x  
**Document :** R&D technique — V0.2  
**Statut :** conception / pré-implémentation  
**Priorité actuelle :** logique, Seed, routes, Solveur  
**C++ / headers :** volontairement reportés

---

# 1. Objectif

Construire le système logique d'un gigantesque Cube composé de salles cubiques interconnectées.

Le joueur ne doit pas simplement parcourir une grille : il doit **comprendre un système**, identifier une route valide, interpréter les indices des salles et progresser jusqu'à la sortie.

Le système doit permettre :

- une génération entièrement déterministe par `Seed` ;
- la génération de routes précises et reproductibles ;
- des salles possédant des attributs propres ;
- des salles spéciales capables de modifier les règles ;
- des faux chemins contrôlés ;
- des dangers cohérents avec la route ;
- un Solveur capable de vérifier automatiquement qu'une Seed est réellement résoluble ;
- la reproduction exacte d'une partie à partir de sa Seed ;
- une séparation stricte entre **logique du puzzle** et **représentation physique UE5**.

Principe central :

> **La Seed ne doit pas seulement générer un décor. Elle doit générer un problème solvable.**

---

# 2. Modèle mental

Le Cube est modélisé comme un graphe spatial 3D.

Une salle possède une coordonnée logique :

```text
L = (X, Y, Z)
```

avec :

```text
0 <= X < N
0 <= Y < N
0 <= Z < N
```

Pour une taille `N × N × N` :

```text
RoomCount = N³
```

Exemples :

```text
3³  = 27 salles
5³  = 125 salles
7³  = 343 salles
10³ = 1000 salles
26³ = 17576 salles
```

La coordonnée logique est permanente.

La position physique, l'orientation et la rotation visuelle peuvent changer sans modifier l'identité logique de la salle.

---

# 3. Séparation logique / physique

Cette séparation est fondamentale.

## 3.1 État logique

Le Solveur manipule uniquement :

```text
RoomID
LogicalCoord
Doors
RoomType
RoomAttributes
Hazards
Clues
State
Connections
```

Il ne doit pas dépendre :

- du Actor UE5 ;
- du Transform réel ;
- du niveau de détail ;
- de la collision graphique ;
- des animations ;
- de la géométrie finale.

## 3.2 État physique

UE5 traduit ensuite l'état logique en :

```text
World Transform
Rotation
Mesh
Animation
Collision
FX
Audio
Lighting
```

Une rotation visuelle du Cube ne doit donc jamais casser le graphe.

---

# 4. Identité d'une salle

Chaque salle possède un identifiant stable.

Format conceptuel :

```text
RoomID = Encode(X, Y, Z)
```

Une forme simple :

```text
RoomID = X + Y*N + Z*N*N
```

Pour `N = 5` :

```text
(0,0,0) -> 0
(1,0,0) -> 1
(0,1,0) -> 5
(0,0,1) -> 25
```

L'identifiant doit être déterministe et réversible.

---

# 5. Graphe spatial

Chaque salle possède au maximum six voisins :

```text
+X
-X
+Y
-Y
+Z
-Z
```

Une connexion est valide uniquement si la distance de Manhattan vaut 1 :

```text
abs(dx) + abs(dy) + abs(dz) == 1
```

Exemple :

```text
A = (2,3,1)
B = (3,3,1)

distance = 1
=> voisin valide
```

Mais :

```text
A = (2,3,1)
B = (3,4,1)

distance = 2
=> connexion directe interdite
```

Le graphe spatial de base peut donc être généré sans aucune information artistique.

---

# 6. Seed déterministe

Une Seed est un identifiant de génération, pas un simple nombre aléatoire.

Exemple :

```text
Seed_00
Seed_01
...
Seed_49
```

Toutes les décisions procédurales doivent être reproductibles.

Il faut éviter un unique flux RNG pour tout le système.

## 6.1 Sous-Seeds

Concept recommandé :

```text
S_PATH     = Hash(Seed, "PATH")
S_ROOMS    = Hash(Seed, "ROOMS")
S_HAZARDS  = Hash(Seed, "HAZARDS")
S_CLUES    = Hash(Seed, "CLUES")
S_ROTATION = Hash(Seed, "ROTATION")
S_SPECIAL  = Hash(Seed, "SPECIAL")
```

Conséquence :

Si on change les dangers, le chemin ne change pas.

Si on change les indices, le chemin ne change pas.

Cette indépendance est essentielle pour le développement et le debug.

---

# 7. La route comme objet de conception

Le point le plus important de ce document est que la route doit être **explicitement générée avant les salles détaillées**.

On ne demande pas :

> « Génère 100 salles puis cherche une solution. »

On demande :

> « Génère une route cible, puis construis l'environnement autour de cette route. »

Cela permet au designer de contrôler la progression.

---

# 8. Route cible

Une route est une séquence ordonnée :

```text
P = [R0, R1, R2, ..., RL]
```

avec :

```text
R0   = START
RL   = EXIT
```

et chaque paire successive doit être voisine.

Exemple :

```text
START
  |
R01
  |
R02
  |
R03
 /  \
R04 R_fake
 |
R05
 |
EXIT
```

Le chemin critique est donc connu du générateur.

---

# 9. Génération d'une route

Le générateur de route reçoit un profil.

Exemple :

```text
RouteProfile:
    MinLength
    MaxLength
    RequiredTurns
    MinVerticalChanges
    MaxVerticalChanges
    RequiredBranches
    RequiredSpecialRooms
    RequiredRoomTypes
    MaxRepeatedDirection
    MinFalsePathDepth
```

Pour une première Seed :

```text
MinLength = 80
MaxLength = 150
RequiredTurns = 20
MinVerticalChanges = 5
RequiredBranches = 15
```

Ces valeurs sont des paramètres de R&D et doivent être calibrées par playtest.

---

# 10. Route dirigée par segments

Pour donner une identité à une Seed, on peut définir des segments.

Exemple :

```text
SEGMENT_01 : apprentissage
SEGMENT_02 : orientation
SEGMENT_03 : danger
SEGMENT_04 : rotation
SEGMENT_05 : déduction
SEGMENT_06 : combinaison
SEGMENT_07 : final
```

Chaque segment impose des contraintes.

Exemple :

```text
Segment 01
- longueur : 10
- aucun danger mortel
- indices simples

Segment 02
- longueur : 15
- changements de direction
- première rotation

Segment 03
- longueur : 20
- dangers
- faux chemins

Segment 04
- longueur : 15
- orientation inversée
- salle pivot

Segment 05
- longueur : 20
- indices dépendants du contexte
```

La Seed devient ainsi une **séquence de situations de gameplay**, et non une suite aléatoire.

---

# 11. Route imposée par le designer

Le système doit également accepter une route explicitement décrite.

Exemple conceptuel :

```text
START
-> TUTORIAL
-> TUTORIAL
-> TURN_LEFT
-> CROSS
-> HAZARD
-> PUZZLE
-> ROTATE
-> FAKE_BRANCH
-> SAFE_ROOM
-> CHECKPOINT
-> EXIT
```

Le générateur traduit ensuite cette description en coordonnées 3D.

C'est important pour pouvoir créer des salles artistiquement spécifiques.

Le designer pourra dire :

```text
Room 17 = Electrical
Room 18 = Vertical
Room 19 = Puzzle
Room 20 = Boss-like hazard
```

sans perdre le contrôle du chemin.

---

# 12. Route abstraite vs route physique

Il faut distinguer deux concepts.

## Route abstraite

```text
START
-> TURN
-> HAZARD
-> CLUE
-> ROTATE
-> EXIT
```

## Route physique

```text
(0,0,0)
(1,0,0)
(1,1,0)
(1,1,1)
(2,1,1)
...
```

L'abstraction permet au designer de définir le gameplay.

La conversion vers des coordonnées est ensuite réalisée par le générateur.

---

# 13. Système de Room Archetypes

Une salle ne doit pas être seulement `Safe / Dangerous`.

Elle possède un archétype.

Exemples :

```text
START
NORMAL
TUTORIAL
TURN
JUNCTION
DEAD_END
HAZARD
PUZZLE
CLUE
ROTATION
CHECKPOINT
AMBUSH
TRANSITION
SAFE
EXIT
```

Chaque archétype possède des contraintes.

Exemple :

```text
ROTATION:
    RequiresRotation = true
    AllowedNextDirections = [...]
    ClueMode = OrientationDependent
```

---

# 14. Attributs de salle

Une salle peut posséder :

```text
RoomID
Coordinate
Archetype
Difficulty
DangerLevel
ClueType
RequiredAction
AllowedEntries
AllowedExits
Orientation
RotationRule
TimeLimit
FailureCost
Checkpoint
IsOnCriticalPath
IsDecoy
```

Exemple :

```text
Room_184

Archetype = ROTATION
Difficulty = 4
DangerLevel = 2

Entry = -X

RequiredAction = ROTATE_90

Exit = +Z

ClueType = ORIENTATION_DEPENDENT

IsOnCriticalPath = true
```

---

# 15. Attributs obligatoires vs décoratifs

Cette distinction est importante.

## Gameplay-critical

Les attributs qui modifient la solution :

```text
Doors
Hazards
Clue
Rotation
RequiredAction
State
Connections
```

## Presentation-only

Les attributs qui ne modifient pas la solution :

```text
Material
LightColor
MeshVariant
ParticleFX
SoundSet
DecorationSet
```

Le Solveur ne doit jamais dépendre des attributs décoratifs.

---

# 16. Salles spéciales

Certaines salles peuvent modifier temporairement les règles.

Exemples :

### ROTATOR

Change l'orientation du Cube.

### MIRROR

Inverse certaines directions.

### LOCK

Bloque une sortie jusqu'à accomplissement d'une action.

### KEY

Produit une information nécessaire plus tard.

### CHECKPOINT

Mémorise la progression.

### RESET

Réinitialise une partie de l'état.

### TELEPORT

Change la position logique du joueur selon une règle connue.

### CONDITIONAL

Une sortie devient disponible selon un état précédent.

Ces salles transforment le problème d'un simple pathfinding en **state-space search**.

---

# 17. Pourquoi le Solveur doit gérer un état

Un simple BFS sur `(X,Y,Z)` ne suffit plus dès qu'une salle peut modifier l'état du monde.

Le véritable nœud du Solveur devient :

```text
State = (
    RoomID,
    Orientation,
    Inventory,
    Flags,
    CheckpointState,
    PuzzleState
)
```

Deux passages dans la même salle peuvent donc être différents.

Exemple :

```text
(Room_42, Orientation=0)
(Room_42, Orientation=1)
```

sont deux états différents.

---

# 18. Solveur — objectif

Le Solveur doit répondre à plusieurs questions :

1. Existe-t-il une solution ?
2. Quelle est la solution minimale ?
3. Combien de solutions existent ?
4. Combien de faux chemins concurrents existent ?
5. Le chemin critique est-il réellement nécessaire ?
6. Une route peut-elle être contournée ?
7. Existe-t-il un raccourci illégal ?
8. La Seed est-elle trop triviale ?
9. La Seed est-elle excessivement ambiguë ?
10. Le temps théorique est-il cohérent avec la cible ?

---

# 19. Solveur V0 — BFS

Pour une première version sans état complexe :

```text
Queue = [START]
Visited = {START}
Parent[START] = None
```

Pour chaque salle :

```text
for each neighbor:
    if neighbor is valid
    and neighbor not visited:
        Parent[neighbor] = current
        Visited.add(neighbor)
        Queue.push(neighbor)
```

Lorsque `EXIT` est trouvé :

```text
reconstruct path using Parent[]
```

Complexité :

```text
O(V + E)
```

Sur une grille 3D, chaque salle possède au maximum 6 voisins.

---

# 20. Solveur V1 — recherche avec contraintes

Lorsque les salles possèdent des règles :

```text
StateNode = (
    RoomID,
    Orientation,
    Flags,
    Inventory
)
```

Transition :

```text
CurrentState
    |
    +-- Action A -> State A
    +-- Action B -> State B
    +-- Action C -> State C
```

Une transition doit être validée par :

```text
CanEnter()
CanUseDoor()
CanSurvive()
CanPerformAction()
CanSatisfyPrerequisite()
```

---

# 21. Pseudocode du Solveur

```text
SOLVE(World, StartState):

    OPEN = queue()
    VISITED = set()
    PARENT = map()

    OPEN.push(StartState)
    VISITED.add(Hash(StartState))

    while OPEN not empty:

        State = OPEN.pop()

        if IsGoal(State):
            return ReconstructPath(State, PARENT)

        Actions = GetAvailableTransitions(State)

        for Action in Actions:

            NextState = Simulate(State, Action)

            if not IsValid(NextState):
                continue

            Key = Hash(NextState)

            if Key in VISITED:
                continue

            VISITED.add(Key)
            PARENT[Key] = State
            OPEN.push(NextState)

    return NO_SOLUTION
```

Ce Solveur devient la référence de validation du générateur.

---

# 22. Solveur et génération doivent être séparés

Architecture :

```text
Seed
 |
 +--> Generator
 |      |
 |      +--> World
 |
 +--> Solver
        |
        +--> Solution
        +--> Metrics
        +--> Validation
```

Le générateur ne doit pas se déclarer lui-même valide.

Le Solveur doit pouvoir démontrer la validité.

---

# 23. Génération en deux passes

## PASS A — génération structurelle

Créer :

```text
Grid
Rooms
TargetRoute
Branches
SpecialRooms
```

Puis :

```text
Solve()
```

## PASS B — enrichissement

Ajouter :

```text
Hazards
Clues
Rotations
RoomVariants
Decorative parameters
```

Puis :

```text
Solve()
```

Si la solution disparaît :

```text
Seed = INVALID
```

Cette méthode évite qu'une salle artistique casse accidentellement le puzzle.

---

# 24. Faux chemins

Les faux chemins doivent être générés intentionnellement.

Ils ne doivent pas être de simples impasses aléatoires.

Un faux chemin peut avoir :

```text
Depth = 4
ClueSimilarity = 0.8
DangerLevel = 2
RecoveryPossible = true
```

Exemple :

```text
Critical:
A -> B -> C -> D -> E

Decoy:
A -> X -> Y -> Z -> DeadEnd
```

Le faux chemin doit sembler plausible selon les informations disponibles au joueur.

---

# 25. Profondeur des faux chemins

Paramètre :

```text
FalsePathDepth
```

Exemple de plage R&D :

```text
Easy    : 1–2
Normal  : 3–5
Hard    : 5–10
Expert  : 8–15
```

Un faux chemin trop court n'est pas réellement trompeur.

Un faux chemin trop long augmente artificiellement le temps de jeu sans forcément augmenter la profondeur cognitive.

---

# 26. Branches contrôlées

Le générateur doit connaître le nombre de branches au niveau des décisions.

Métrique :

```text
BranchCount
```

et surtout :

```text
AmbiguousBranchCount
```

Une branche est ambiguë si plusieurs sorties restent cohérentes avec les informations actuellement disponibles au joueur.

C'est cette métrique qui doit être utilisée pour calibrer la difficulté.

---

# 27. Indices

Un indice doit être lié à la logique de la route.

Niveaux possibles :

```text
LEVEL 0
Direction explicite

LEVEL 1
Symbole de direction

LEVEL 2
Symbole dépendant de l'orientation

LEVEL 3
Indice nécessitant une transformation

LEVEL 4
Indice cryptographique / relationnel
```

Principe :

```text
Clue = F(
    RoomID,
    Orientation,
    RouteContext,
    SeedClue
)
```

Le tableau de directions ne doit pas être directement exposé comme une solution brute.

---

# 28. RouteContext

Pour des énigmes avancées, l'indice peut dépendre du contexte.

Exemple :

```text
PreviousDirection
CurrentOrientation
RoomType
PreviousRoomType
SeedRule
```

Ainsi deux salles identiques visuellement peuvent fournir des indices différents.

---

# 29. Préparation des Seeds

Une Seed de production doit produire un manifeste.

Exemple :

```text
Seed_23

RouteProfile:
    Length = 118
    VerticalChanges = 11
    Turns = 37
    Branches = 24

Difficulty:
    Navigation = 4
    Clues = 3
    Hazards = 4
    Orientation = 5

SpecialRooms:
    Rotation = 8
    Checkpoint = 2
    Puzzle = 6

FalsePaths:
    Count = 17
    AverageDepth = 5.2
```

Ce manifeste permet de reproduire exactement la Seed et surtout de savoir **quelle route et quelles salles doivent être produites**.

---

# 30. Familles de Seeds

Une distribution possible :

```text
Seed 00–09
Apprentissage / navigation

Seed 10–19
Orientation / rotations

Seed 20–29
Faux chemins / déduction

Seed 30–39
Indices avancés

Seed 40–49
Combinaison complète
```

Cette classification est un outil de production, pas une note de qualité.

---

# 31. Validation automatique

Chaque Seed doit produire un rapport.

Exemple :

```text
SEED VALIDATION

Seed: 23

SolutionExists: TRUE
SolutionLength: 118

AlternativeSolutions: 1

FalsePaths: 17
AverageFalsePathDepth: 5.2

AmbiguousBranches: 8

DeadEnds: 31

RequiredRotations: 6

RequiredSpecialRooms: 14

IllegalShortcuts: 0

UnreachableRooms: 3

EstimatedTraversalTime: 54 min

STATUS: VALID
```

---

# 32. Critères de rejet

Une Seed est rejetée si :

```text
NO SOLUTION

OU

SolutionLength < Minimum

OU

IllegalShortcut > 0

OU

CriticalRoom inaccessible

OU

RequiredRoom impossible

OU

Clue contradiction

OU

Danger bloque toutes les routes

OU

Ambiguity > Maximum

OU

FalsePathDepth < Minimum

OU

RouteProfile non respecté
```

---

# 33. Exploit detection

Le Solveur logique doit tester les transitions illégales :

```text
Door bypass
Collision bypass
Clipping
Teleport
Rotation during transition
Invalid room transition
State reset exploit
Checkpoint exploit
Physics bypass
```

Pour la logique pure :

```text
if Distance(RoomA, RoomB) != 1:
    reject transition
```

Pour UE5, ces règles devront ensuite être vérifiées par l'autorité serveur.

---

# 34. Salle et route : contrat technique

Une salle critique doit déclarer :

```text
RequiredEntry
RequiredExit
RequiredState
ProvidedState
FailureState
Clue
```

Exemple :

```text
Room: ROTATION_04

RequiredEntry:
    Direction = West

RequiredState:
    Orientation = 2

Action:
    RotateClockwise

ProvidedState:
    Orientation = 3

Exit:
    North

FailureState:
    ReturnToCheckpoint
```

Ce format permet de construire ensuite les salles dans UE5 sans modifier le Solveur.

---

# 35. Route Blueprint de production

Avant de construire les meshes, le designer devrait pouvoir créer :

```text
START
 |
TUTORIAL
 |
CLUE
 |
TURN
 |
HAZARD
 |
JUNCTION
 | \
 |  DECOY
 |
ROTATION
 |
PUZZLE
 |
CHECKPOINT
 |
FINAL
 |
EXIT
```

Puis le générateur transforme ce Blueprint en :

```text
Seed
+
Coordinates
+
Room Attributes
+
Connections
+
Clues
+
Hazards
```

---

# 36. Ce que le système doit permettre au designer

Le designer doit pouvoir dire :

> « À la 20e salle, je veux une salle verticale avec deux sorties, dont une fausse, puis une rotation de 90° et un indice dépendant de l'orientation. »

Le système doit pouvoir produire cela à partir de données, sans modifier le Solveur.

Autre exemple :

```text
At Step 35:
    RoomType = ELECTRICAL
    RequiredAction = WAIT
    Hazard = ELECTRIC_PULSE
    Clue = ORIENTATION
    NextRoom = ROTATION
```

---

# 37. Pipeline complet

```text
SEED
  |
  v
SEED PROFILE
  |
  v
ROUTE BLUEPRINT
  |
  v
3D ROUTE GENERATOR
  |
  v
ROOM GRAPH
  |
  v
SPECIAL ROOM ASSIGNMENT
  |
  v
SOLVER
  |
  +---- INVALID ---> REGENERATE
  |
  v
HAZARD GENERATION
  |
  v
CLUE GENERATION
  |
  v
SOLVER
  |
  +---- INVALID ---> REGENERATE
  |
  v
SEED VALIDATED
  |
  v
UE5 PHYSICAL GENERATION
  |
  v
ART / FX / AUDIO
```

---

# 38. Première implémentation recommandée

Ne pas commencer par `26³`.

Commencer avec :

```text
3 × 3 × 3
```

Puis :

```text
5 × 5 × 5
```

Puis :

```text
7 × 7 × 7
```

L'objectif du prototype n'est pas la taille.

L'objectif est de prouver :

```text
Seed -> Route -> Rooms -> Solver -> Validated Seed
```

---

# 39. Tests unitaires conceptuels

### Test 01 — reproductibilité

```text
Seed X
Generate()
Generate()

Result A == Result B
```

### Test 02 — indépendance des dangers

```text
PATH(seed) == PATH(seed + hazard variation)
```

### Test 03 — solution

```text
Solver(World).HasSolution == true
```

### Test 04 — chemin valide

Chaque transition :

```text
ManhattanDistance == 1
```

### Test 05 — sortie atteignable

```text
START -> EXIT
```

### Test 06 — aucun raccourci

Toute transition hors graphe :

```text
INVALID
```

### Test 07 — salle critique

Toutes les salles imposées par le RouteProfile sont présentes et accessibles.

---

# 40. Métriques de R&D

Pour chaque Seed :

```text
L  = solution length
F  = false path count
D  = average false path depth
B  = ambiguous branch count
R  = required room count
H  = hazard exposure
O  = orientation complexity
C  = clue complexity
E  = exploit count
T  = estimated time
S  = number of valid solutions
```

Ces métriques servent à **valider des contraintes de conception**, pas à attribuer une note au joueur.

---

# 41. Modèle de temps

Modèle initial :

```text
T =
    L * t_move
  + N_decision * t_decode
  + N_rotation * t_rotate
  + N_failure * t_failure
  + T_search
```

Le temps de déplacement pur ne doit pas devenir le principal facteur.

Le cœur de l'expérience est :

```text
observation
+
compréhension
+
décision
+
navigation
```

Les valeurs devront ensuite être remplacées par des mesures issues de playtests.

---

# 42. Ce qui reste volontairement hors périmètre

Pour cette phase :

```text
C++ header
C++ implementation
Replication
RPC
Steam
UI finale
Animation finale
Meshes
Materials
VFX
Audio
```

On construit d'abord le cerveau logique du système.

---

# 43. Prochaine étape technique

La prochaine implémentation doit être un **Solveur indépendant du moteur**.

Entrées :

```text
GridSize
Seed
RoomGraph
RoomAttributes
StartState
GoalState
```

Sorties :

```text
SolutionPath
SolutionLength
AlternativeSolutions
VisitedStates
DeadEnds
AmbiguousBranches
InvalidTransitions
ValidationReport
```

Ensuite seulement, le Solveur pourra être branché à Unreal.

---

# 44. Architecture logique cible

```text
CubeGenerator
    |
    +-- SeedSystem
    |
    +-- RouteGenerator
    |
    +-- RoomGenerator
    |
    +-- SpecialRoomGenerator
    |
    +-- HazardGenerator
    |
    +-- ClueGenerator
    |
    +-- Solver
    |
    +-- Validator
    |
    +-- SeedReport
```

Le principe important est que `Solver` et `Validator` restent indépendants de la génération visuelle.

---

# 45. Conclusion R&D

Le système doit être pensé comme un **générateur de problèmes spatiaux déterministes**, et non comme un générateur procédural de salles.

La chaîne de production correcte est :

```text
DESIGN INTENTION
      ↓
ROUTE BLUEPRINT
      ↓
SEED
      ↓
3D ROUTE
      ↓
ROOM ATTRIBUTES
      ↓
SOLVER
      ↓
VALIDATION
      ↓
UE5
```

La conséquence principale est que les futures salles pourront être conçues avec des attributs précis sans perdre le contrôle du parcours.

Le Solveur devient le garant logique du système :

> **Si le Solveur ne peut pas démontrer que la route existe et respecte les contraintes, la Seed n'est pas valide.**

---

# ANNEXE A — Exemple de Seed complète

```text
Seed = 23

Grid = 7x7x7

START = (0,3,3)
EXIT  = (6,5,1)

RouteLength = 118

Segments:
    01 Tutorial      10 steps
    02 Navigation    15 steps
    03 Hazard        20 steps
    04 Rotation      15 steps
    05 Deduction     20 steps
    06 Combination   28 steps
    07 Final         10 steps

SpecialRooms:
    Rotation     = 8
    Puzzle       = 6
    Checkpoint   = 2
    Hazard       = 14

FalsePaths:
    Count = 17
    Depth = 3..9

Clues:
    Level 1 = 35%
    Level 2 = 40%
    Level 3 = 20%
    Level 4 = 5%

Validation:
    Solution = TRUE
    IllegalShortcut = 0
    RequiredRoomsReachable = TRUE
```

---

# ANNEXE B — Format conceptuel d'une Room

```text
Room
{
    RoomID
    LogicalCoord

    Archetype

    Doors[6]

    IsOnCriticalPath
    IsDecoy

    Difficulty

    Hazard
    Clue

    RequiredAction

    Orientation
    RotationRule

    EntryConstraint
    ExitConstraint

    StateFlags
    InventoryEffects

    Checkpoint
}
```

---

# ANNEXE C — Règle fondamentale

Ne jamais coder :

```text
"la salle 42 mène toujours à la salle 43"
```

Coder :

```text
"selon la Seed et l'état courant,
la transition valide de la salle 42
est calculée par le graphe."
```

La route doit être une **propriété générée et validée**, pas une collection de références codées en dur.
