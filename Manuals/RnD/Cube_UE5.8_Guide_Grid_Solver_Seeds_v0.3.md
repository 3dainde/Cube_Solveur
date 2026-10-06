# CUBE — Guide technique UE5.8
## Grille 3D, Sockets, Solveur, Routes et 50 Seeds

**Version : 0.3 — R&D / architecture de production**  
**Cible : Unreal Engine 5.8**  
**Statut : spécification technique — C++/headers volontairement hors périmètre à ce stade**

---

## 0. Objectif

Construire un Cube labyrinthique 3D composé de cellules/salles cubiques, suffisamment vaste pour donner l'impression d'un espace quasi infini, tout en ne matérialisant dans UE5.8 que les cellules nécessaires autour du joueur.

La cible de référence est une **grille logique 64 × 64 × 64 = 262 144 cellules**, avec une architecture extensible à 128³.

Le système doit permettre :

- de générer une configuration déterministe à partir d'un Seed ;
- de créer une route sûre START → EXIT ;
- de placer des fausses routes et des branches contrôlées ;
- de valider la configuration avec un solveur indépendant du rendu ;
- de disposer d'une entrée/sortie sur chacune des 6 faces externes ;
- d'utiliser des Sockets UE5 pour les connexions entre salles ;
- de produire deux modes : **Facile** et **Impossible** ;
- de modifier la difficulté par les indices placés sur les Sockets ;
- de générer et comparer de nombreux Seeds afin de sélectionner 50 configurations réellement différentes ;
- de streamer localement les salles visibles autour du joueur ;
- de conserver une reproduction exacte d'un niveau à partir de son Seed.

Référence visuelle : architecture géométrique, répétitive et désorientante dans l'esprit du film *Cube*, sans reproduire ses éléments protégés.

---

# 1. Principe fondamental

Le système doit séparer **la logique du Cube** de **sa représentation Unreal**.

```text
                         SEED
                           │
                           ▼
                  GENERATEUR LOGIQUE
                           │
                           ▼
                    GRILLE 64³
                           │
                 ┌─────────┴─────────┐
                 ▼                   ▼
             ROUTE BUILDER        SOLVEUR
                 │                   │
                 └─────────┬─────────┘
                           ▼
                     VALIDATION
                           │
                           ▼
                  WORLD MANIFEST
                           │
                           ▼
                    UE5.8 STREAMING
                           │
                           ▼
                 SALLES PROCHES DU JOUEUR
```

**Règle majeure :** le solveur ne dépend jamais des Actors actuellement spawnés.

La grille complète peut donc contenir 262 144 cellules logiques sans créer 262 144 Actors Unreal.

---

# 2. Grille logique

## 2.1 Dimensions

Valeur de production initiale :

```text
GridSizeX = 64
GridSizeY = 64
GridSizeZ = 64
```

Soit :

```text
64 × 64 × 64 = 262 144 cellules
```

L'architecture doit toutefois accepter :

```text
32³
64³   ← cible principale
96³
128³  ← extension future
```

Ne jamais coder `64` directement dans les systèmes.

## 2.2 Coordonnée logique

Chaque cellule possède :

```text
X, Y, Z
```

avec :

```text
0 <= X < GridSizeX
0 <= Y < GridSizeY
0 <= Z < GridSizeZ
```

Un identifiant stable peut être calculé à partir de la coordonnée.

Exemple conceptuel :

```text
CellID = X + Y * SizeX + Z * SizeX * SizeY
```

Le CellID ne doit pas dépendre du nom ou de l'adresse mémoire d'un Actor.

---

# 3. Cellule / salle

Une cellule logique représente une salle potentielle.

Elle doit contenir uniquement les données nécessaires au gameplay et à la génération.

Structure conceptuelle :

```text
CubeCell
├── CellID
├── Coordinate
├── RoomType
├── Orientation
├── Connections[6]
├── RequiredEntry
├── AllowedExits
├── Hazard
├── ClueType
├── ClueData
├── Difficulty
├── IsCriticalPath
├── IsDecoy
├── Checkpoint
└── StateRules
```

Les informations purement visuelles ne doivent pas être utilisées par le solveur.

---

# 4. Les six directions et les Sockets

Chaque cellule utilise six directions logiques :

```text
+X = East
-X = West
+Y = North
-Y = South
+Z = Top
-Z = Bottom
```

Chaque salle physique possède les Sockets correspondants :

```text
Socket_East
Socket_West
Socket_North
Socket_South
Socket_Top
Socket_Bottom
```

Une connexion entre deux cellules est bidirectionnelle :

```text
A +X  <────>  B -X
```

Une connexion n'est valide que si les deux côtés correspondent.

Le système doit pouvoir distinguer :

```text
OPEN
CLOSED
LOCKED
HAZARD
ONE_WAY
CONDITIONAL
```

Le Socket est donc à la fois un **point d'attache physique** et une représentation d'une transition logique.

---

# 5. Blueprint de salle

Créer une base :

```text
BP_CubeRoom
```

Elle contient au minimum :

```text
Root
├── RoomMesh / Geometry
├── Socket_East
├── Socket_West
├── Socket_North
├── Socket_South
├── Socket_Top
└── Socket_Bottom
```

Ajouter des Tags permettant au générateur d'identifier les pièces.

Exemples :

```text
CubeRoom
CubeRoom.Normal
CubeRoom.Start
CubeRoom.Exit
CubeRoom.Turn
CubeRoom.Junction
CubeRoom.DeadEnd
CubeRoom.Hazard
CubeRoom.Puzzle
CubeRoom.Rotation
CubeRoom.Checkpoint
```

Le Tag identifie la **famille fonctionnelle** ; les données de Seed déterminent l'utilisation réelle de la cellule.

Éviter de faire dépendre la logique du nom de l'Actor.

---

# 6. Types de salles

Le générateur doit disposer d'un catalogue de types, extensible sans modifier le solveur.

Types initiaux :

```text
START
NORMAL
TURN
JUNCTION
DEAD_END
HAZARD
PUZZLE
CLUE
ROTATION
CHECKPOINT
TRANSITION
EXIT
```

Une même géométrie peut recevoir plusieurs fonctions logiques.

Exemple : une salle visuellement normale peut être marquée :

```text
RoomType = HAZARD
```

ou :

```text
RoomType = CLUE
```

sans changer nécessairement son mesh.

---

# 7. Les entrées/sorties du grand Cube

Les six faces externes du Cube disposent chacune d'une ouverture contrôlée.

```text
-X  WEST
+X  EAST
-Y  SOUTH
+Y  NORTH
-Z  BOTTOM
+Z  TOP
```

Le système doit permettre de définir dynamiquement :

```text
StartFace
ExitFace
```

et éventuellement de rendre plusieurs faces accessibles selon le mode de jeu.

Le premier prototype doit supporter au minimum :

```text
1 START
1 EXIT
4 autres faces configurables
```

Le solveur doit considérer les limites de la grille comme des contraintes réelles : aucune connexion ne doit sortir du volume sans passer par une face externe autorisée.

---

# 8. Route Blueprint

La route ne doit pas être créée uniquement par hasard.

Le générateur doit accepter une **intention de route**.

Exemple :

```text
START
 ↓
TUTORIAL
 ↓
TURN
 ↓
NORMAL
 ↓
HAZARD
 ↓
PUZZLE
 ↓
ROTATION
 ↓
FAKE_BRANCH
 ↓
CHECKPOINT
 ↓
EXIT
```

Cette description est transformée en route 3D.

La Route Blueprint peut définir :

```text
minLength
maxLength
requiredTurns
requiredVerticalChanges
requiredRoomTypes
requiredHazards
requiredCheckpoints
branchCount
falsePathDepth
clueDifficulty
```

Le générateur cherche ensuite une réalisation compatible dans la grille.

---

# 9. Génération de la route sûre

Le système doit d'abord construire le **chemin critique**.

```text
START → C1 → C2 → C3 → ... → EXIT
```

Chaque transition doit être adjacente :

```text
ManhattanDistance(A,B) = 1
```

Le chemin doit éviter toute répétition sauf si une mécanique particulière autorise explicitement une boucle.

La route peut être construite par :

1. contraintes de départ ;
2. génération pseudo-aléatoire déterministe ;
3. recherche/backtracking ;
4. vérification des contraintes ;
5. validation par le solveur.

Le générateur ne considère jamais une route comme valide simplement parce qu'il l'a créée.

**Le solveur doit la prouver.**

---

# 10. Solveur

Le solveur est d'abord un système logique indépendant d'UE5.

## V0 — graphe statique

État minimal :

```text
State = CellID
```

Algorithme recommandé : BFS pour obtenir une validation simple et déterministe.

Objectifs :

```text
START → EXIT existe ?
longueur du chemin
nombre de solutions
cellules critiques
branches
impasses
```

## V1 — état de jeu

Quand les mécaniques seront ajoutées :

```text
State =
(
 CellID,
 Orientation,
 Inventory,
 Flags,
 CheckpointState,
 PuzzleState
)
```

Le solveur pourra alors gérer :

- portes verrouillées ;
- clés ;
- interrupteurs ;
- rotations ;
- salles conditionnelles ;
- pièges ;
- états persistants ;
- indices nécessaires.

La transition devient alors :

```text
State + Action → NewState
```

---

# 11. Positionnement des blocs par le solveur

Le solveur n'est pas seulement un outil de recherche après génération.

Le pipeline recherché est :

```text
Route Blueprint
      ↓
Placement contraint
      ↓
Création du graphe
      ↓
Solveur
      ↓
Validation
```

Si le graphe ne contient pas la route souhaitée, la configuration est rejetée ou corrigée.

Le solveur doit donc être considéré comme le **juge mathématique de la génération**.

À terme, une génération plus avancée pourra utiliser les résultats du solveur pour choisir le prochain placement de cellule.

---

# 12. Fausses routes

Les fausses routes sont nécessaires pour éviter un simple couloir.

Une branche peut avoir :

```text
Depth
DangerLevel
ClueSimilarity
RecoveryPossible
DeadEnd
FalseExit
```

Exemple :

```text
        ┌── FAKE ── DEAD END
        │
START ──┼── CRITICAL ───────── EXIT
        │
        └── FAKE ── HAZARD ── RETURN
```

Le générateur doit contrôler le nombre et la profondeur des branches.

Une difficulté supérieure ne signifie pas simplement « plus de pièces ».

---

# 13. Facile / Impossible

Utiliser le même moteur de génération avec deux profils de contraintes.

## EASY

Paramètres possibles :

```text
routeLength       faible à moyenne
falseBranches     faible
branchDepth       faible
clueDifficulty    faible
hazards           faible
ambiguity         faible
```

## IMPOSSIBLE

```text
routeLength       élevée
falseBranches     élevée
branchDepth       élevée
clueDifficulty    élevée
hazards           élevée
ambiguity         élevée
stateComplexity   élevée
```

**Impossible ne doit pas signifier aléatoire ou injuste.**

Le solveur doit toujours pouvoir démontrer qu'une solution conforme aux règles existe.

---

# 14. Indices sur les Sockets

Les indices sont traités comme une couche indépendante du graphe.

Architecture :

```text
ClueProvider
├── Braille
├── MathFormula
├── Symbol
└── FutureProvider
```

Le type définit la manière dont l'information est encodée ; il ne doit pas modifier directement le chemin.

Un indice peut renseigner :

```text
direction
orientation
danger
ordre
condition
identité d'une salle
```

Le système doit permettre de remplacer ultérieurement le Braille par les formules mathématiques ou un autre langage sans refaire le générateur.

---

# 15. Difficulté par les indices

La difficulté peut être pilotée indépendamment de la géométrie.

Exemple : même route, deux modes :

```text
EASY
Socket clue → information directe

IMPOSSIBLE
Socket clue → information encodée / ambiguë
```

Le solveur doit connaître la représentation logique de l'indice, même si le joueur doit la décoder.

Ainsi, le jeu peut être difficile pour le joueur sans rendre la génération imprévisible pour le système.

---

# 16. Seed

Chaque niveau est entièrement reproductible avec :

```text
Seed
GameMode
GridSize
RouteProfile
```

Le Seed ne doit pas utiliser un seul flux pseudo-aléatoire pour tout le système.

Utiliser des sous-seeds déterministes :

```text
PATH
ROOM
HAZARD
CLUE
ROTATION
DECOY
```

Conceptuellement :

```text
S_path     = Hash(Seed, "PATH")
S_room     = Hash(Seed, "ROOM")
S_hazard   = Hash(Seed, "HAZARD")
S_clue     = Hash(Seed, "CLUE")
S_rotation = Hash(Seed, "ROTATION")
S_decoy    = Hash(Seed, "DECOY")
```

Modifier les indices ne doit donc pas modifier la route critique.

Modifier les dangers ne doit pas déplacer aléatoirement les salles.

---

# 17. Génération de 50 Seeds

Ne pas simplement prendre :

```text
Seed 0 → niveau
Seed 1 → niveau
...
Seed 49 → niveau
```

Cela ne garantit aucune diversité.

Créer d'abord un grand nombre de candidats :

```text
Seed 0000
Seed 0001
...
Seed NNNN
```

Pour chaque candidat :

1. générer la route ;
2. générer les branches ;
3. placer les salles ;
4. générer les indices ;
5. générer les dangers ;
6. exécuter le solveur ;
7. mesurer la configuration ;
8. comparer sa similarité aux Seeds déjà retenus.

Ne conserver que les candidats satisfaisant les contraintes.

---

# 18. Métrique de diversité des Seeds

Chaque Seed doit produire une signature logique.

Exemple :

```text
RouteLength
TurnCount
VerticalTransitionCount
BranchCount
BranchDepth
HazardDistribution
RoomTypeDistribution
StartFace
ExitFace
OrientationPattern
ClueDistribution
CriticalPathSignature
```

Deux Seeds trop similaires sont rejetés.

La comparaison ne doit pas être uniquement basée sur le Seed numérique.

Deux nombres complètement différents peuvent produire des niveaux presque identiques.

Le but est donc :

```text
SEED ≠ diversité

CONFIGURATION = diversité
```

---

# 19. Sélection des 50 Seeds officiels

Pipeline :

```text
                  10 000+ candidats
                         │
                         ▼
                 VALIDATION SOLVEUR
                         │
                         ▼
                  candidats valides
                         │
                         ▼
                FILTRE DE SIMILARITÉ
                         │
                         ▼
                DIFFICULTÉ / PROFIL
                         │
                         ▼
                  50 SEEDS OFFICIELS
```

Chaque Seed officiel doit produire un **manifest** permettant de reconstruire exactement le niveau.

---

# 20. Manifest d'un Seed

Conceptuellement :

```text
SeedManifest
├── Seed
├── Version
├── GridSize
├── Mode
├── StartCell
├── StartFace
├── ExitCell
├── ExitFace
├── CriticalPath
├── RoomDefinitions
├── Connections
├── Clues
├── Hazards
├── Rotations
├── Branches
└── ValidationMetrics
```

Le manifest est l'artefact de debug principal.

Il permettra également de comparer deux Seeds sans lancer UE5.

---

# 21. Streaming physique autour du joueur

La grille 64³ reste logique.

UE5 ne doit matérialiser que la zone utile.

Principe :

```text
Grid 64³

      logique complète
          │
          ▼
   Player Cell = N
          │
     ┌────┴────┐
     ▼         ▼
   N+1       N+2
```

Le rayon exact sera déterminé par les tests de performance.

Il doit être possible de définir :

```text
VisibleRadius
PreloadRadius
UnloadRadius
```

Exemple conceptuel :

```text
N       = actif
N+1     = actif
N+2     = préchargé
N+3     = non matérialisé
```

Il ne faut pas supposer que toutes les cellules à distance égale sont nécessaires : le streaming peut suivre le graphe et la direction de déplacement.

---

# 22. Optimisation UE5.8

Priorités :

1. aucune dépendance entre le solveur et les Actors ;
2. données de grille compactes ;
3. Spawn/Destroy limité ;
4. réutilisation des salles ;
5. ISM/HISM pour éléments répétitifs ;
6. Nanite lorsque pertinent ;
7. chargement anticipé ;
8. aucune génération coûteuse à chaque Tick.

Le changement de cellule du joueur est un événement important pour le streaming.

Éviter un recalcul complet de la grille à chaque déplacement.

---

# 23. Validation automatique d'un Seed

Un Seed est valide uniquement si :

```text
START existe
EXIT existe
START ≠ EXIT
route valide
route praticable
route conforme au profil
aucune transition illégale
aucune cellule critique inaccessible
branches conformes
indices générables
hazards conformes
```

Mesures recommandées :

```text
L = longueur du chemin critique
B = nombre de branches
D = profondeur moyenne/maximale des fausses routes
A = ambiguïté
H = danger
R = rotations
C = complexité des indices
E = exploitabilité
T = temps estimé
```

---

# 24. Validation des exploits

Le système doit tester au niveau logique :

- connexion impossible entre deux cellules ;
- sortie hors grille ;
- accès à une cellule interdite ;
- passage à travers une porte fermée ;
- contournement d'un verrou ;
- reset d'état ;
- checkpoint exploitable ;
- téléportation non autorisée ;
- rotation pendant une transition ;
- transition non prévue.

Les exploits purement physiques seront testés plus tard dans UE5.

---

# 25. Séparation des responsabilités

## Route Generator
Construit la topologie souhaitée.

## Room Generator
Attribue les types et paramètres des salles.

## Clue Generator
Construit les indices.

## Hazard Generator
Construit les dangers.

## Solver
Prouve l'existence et analyse les solutions.

## Validator
Décide si le Seed respecte le contrat.

## World Builder
Transforme le manifest logique en Actors/Instances UE5.

## Stream Manager
Matérialise uniquement les salles nécessaires autour du joueur.

Aucun de ces systèmes ne doit devenir responsable de tous les autres.

---

# 26. Ordre de développement UE5.8

## Phase 1 — grille

Créer :

```text
BP_CubeRoom
BP_CubeGenerator
```

Créer une grille miniature :

```text
4³ ou 8³
```

avec les six Sockets.

Objectif : vérifier les connexions physiques.

## Phase 2 — grille logique

Implémenter la représentation des cellules sans dépendre du rendu.

## Phase 3 — route

Créer START → EXIT avec une route déterministe.

## Phase 4 — solveur

Implémenter BFS et afficher :

```text
Route found
Path length
Visited cells
Branches
```

## Phase 5 — streaming

Passer progressivement à :

```text
16³ → 32³ → 64³
```

sans changer le solveur.

## Phase 6 — fausses routes

Ajouter branches et impasses contrôlées.

## Phase 7 — indices

Créer l'interface ClueProvider.

Braille et formule mathématique restent interchangeables/TBD.

## Phase 8 — difficultés

Ajouter EASY et IMPOSSIBLE.

## Phase 9 — génération massive

Générer des milliers de candidats et sélectionner 50 Seeds.

## Phase 10 — polish

Optimisation, matériaux, animation, audio, VFX, interface et présentation.

---

# 27. Règle d'or du projet

Le système ne doit jamais fonctionner ainsi :

```text
Random → Spawn → Espérer qu'un chemin existe
```

Il doit fonctionner ainsi :

```text
INTENTION
   ↓
CONTRAINTES
   ↓
GENERATION
   ↓
SOLVEUR
   ↓
VALIDATION
   ↓
MANIFEST
   ↓
UE5 WORLD
```

Le Seed est une **recette reproductible**, pas simplement une valeur aléatoire.

---

# 28. Architecture cible

```text
CubeSystem
│
├── CubeGrid
│   └── 64³ logical cells
│
├── SeedSystem
│   ├── Seed
│   └── SubSeeds
│
├── RouteSystem
│   ├── RouteProfile
│   ├── RouteBlueprint
│   └── CriticalPath
│
├── RoomSystem
│   ├── RoomDefinitions
│   └── RoomTypes
│
├── ClueSystem
│   └── ClueProviders
│
├── HazardSystem
│
├── Solver
│   ├── Graph Solver
│   └── State Solver
│
├── Validator
│
├── SeedSelector
│   └── Diversity Analysis
│
├── WorldBuilder
│   └── BP_CubeRoom / Instances
│
└── StreamManager
```

---

# 29. Ce qui reste volontairement ouvert

Les points suivants ne doivent pas être figés prématurément :

- taille physique exacte d'une salle ;
- forme finale des pièces ;
- rotation réelle du Cube ;
- mécanique exacte des portes ;
- langage d'indices final ;
- Braille vs mathématiques ;
- nature précise des dangers ;
- nombre exact de cellules visibles ;
- comportement des six faces externes ;
- règles définitives du mode Impossible ;
- éventuelles mécaniques de boucles/retours.

Ces éléments doivent être exposés comme paramètres du système et non enterrés dans le solveur.

---

# 30. Prochaine étape recommandée

Ne pas commencer immédiatement par le Cube 64³.

Construire d'abord un **prototype logique minimal** capable de :

```text
1. créer une grille
2. placer START / EXIT
3. générer une route
4. créer des connexions
5. exécuter BFS
6. afficher la route trouvée
7. ajouter une fausse branche
8. valider le Seed
9. reproduire exactement le même résultat
```

Puis seulement augmenter :

```text
8³ → 16³ → 32³ → 64³
```

Le solveur et le générateur doivent rester identiques quelle que soit la taille de la grille.

---

# 31. Contrat pour les futures IA travaillant sur le projet

Toute modification du système doit respecter les règles suivantes :

1. Ne pas casser `BP_CubeRoom` ou les contrats de Socket existants.
2. Ne jamais introduire de hasard non déterministe dans la génération.
3. Ne jamais faire dépendre le solveur du mesh.
4. Ne jamais considérer un Seed valide sans validation du solveur.
5. Ne pas modifier la route critique en modifiant uniquement les indices ou les hazards, sauf intention explicite.
6. Conserver la possibilité de reproduire un Seed exactement.
7. Ne pas transformer la grille logique 64³ en 262 144 Actors.
8. Toute nouvelle mécanique d'état doit pouvoir être représentée dans le State du solveur avant d'être considérée comme fiable.
9. Les systèmes de génération doivent rester séparables pour permettre des tests unitaires.
10. Les performances doivent être mesurées dans UE5.8, pas supposées.

---

# Conclusion

La cible technique est un **Cube logique 64³**, extensible à 128³, dont la totalité existe dans les données mais dont seule une petite portion est matérialisée autour du joueur.

Le **Route Generator** construit une intention de parcours. Le **Solver** vérifie ou recherche les solutions. Le **Validator** décide si le Seed respecte le contrat. Le **World Builder** transforme ensuite ce résultat en salles Unreal reliées par leurs Sockets.

Les 50 niveaux officiels ne sont donc pas 50 nombres arbitraires : ce sont **50 configurations sélectionnées parmi une population beaucoup plus importante de Seeds**, après validation et analyse de diversité.

L'architecture doit permettre de conserver une propriété fondamentale :

> **Chaque Cube généré peut être reconstruit exactement à partir de son Seed, et le système peut démontrer mathématiquement que son parcours respecte les règles définies.**
