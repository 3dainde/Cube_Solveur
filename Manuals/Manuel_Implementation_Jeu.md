# Manuel d'implémentation — Jeu « Cube » (UE 5.8, C++)

> Guide complet pour comprendre, compiler, étendre et **rendre jouable** le mode Cube :
> génération procédurale déterministe par seed, système « Rébus » (formules modulaires),
> champ mortel, déplacement en grille discrète, architecture serveur-autoritaire.
>
> Ce document reflète l'état réel du code dans `Source/Cube/`. Chemins, noms de classes,
> champs et signatures sont vérifiés contre les sources.

---

## 0. TL;DR — de zéro à jouable

1. **Compiler** : `CubeEditor Win64 Development` (voir §3). Le build doit finir `Result: Succeeded`.
2. **Créer 2 Data Assets de difficulté** (`UDA_CubeDifficulty`) : `DA_Facile`, `DA_Impossible` (§6.1).
3. **Dériver un Blueprint** de `AGM_CubeGameMode` → `BP_CubeGameMode` ; y assigner la table
   de difficulté, `RoomClass`, `StartingDifficulty` (§6.2).
4. **Dériver un Blueprint** de `APC_CubePlayerController` → `BP_CubePlayerController` ; créer
   6 `InputAction` + 1 `InputMappingContext` et les assigner (§6.3).
5. **Dériver un Blueprint** de `ACubeRoom` → `BP_CubeRoom` avec un mesh cube + matériau exposant
   le paramètre vectoriel `DebugColor` (§6.4).
6. **Créer une map** `L_Cube` et forcer le GameMode `BP_CubeGameMode` dans *World Settings* (§6.6).
7. **PIE** : lancer, se déplacer avec les 6 touches, atteindre la sortie (§7).

Aucune modification de `DefaultEngine.ini` ni du GameMode par défaut n'est nécessaire : le mode
Cube est un ensemble de **classes parallèles** activées par map/World Settings.

---

## 1. Vue d'ensemble de l'architecture

Le jeu est découpé en **4 couches** strictement séparées :

```
┌──────────────────────────────────────────────────────────────────┐
│ COUCHE 1 — Noyau déterministe (pure, engine-agnostic)              │
│   CubeHash · CubeRandom · CubeRebusLibrary · CubeMath              │
│   Parité bit-à-bit avec le référentiel Python (Scripts/)          │
├──────────────────────────────────────────────────────────────────┤
│ COUCHE 2 — Génération / résolution                                 │
│   CubeGenerator · CubeSolver · FCubeManifest · CubePortalLibrary  │
│   Carve DFS, grilles paires ET impaires, boucle de solvabilité,   │
│   ancrages d'entrée / sortie (Socket_Entree / Socket_Sortie)      │
├──────────────────────────────────────────────────────────────────┤
│ COUCHE 3 — Autorité serveur (vérité du niveau)                     │
│   AGM_CubeGameMode · AGS_CubeGameState · UDA_CubeDifficulty       │
│   Arbitrage mort/victoire, secret serveur en compétition          │
├──────────────────────────────────────────────────────────────────┤
│ COUCHE 4 — Présentation / joueur                                   │
│   ACubeCharacter · APC_CubePlayerController · ACubeRoom ·         │
│   UCubeRoomStreamer · ACubeLevelActor · hooks HUD                 │
└──────────────────────────────────────────────────────────────────┘
```

**Principe directeur** : la *vérité* (manifest, champ mortel, formules) vit côté serveur dans
le GameMode. Le GameState ne réplique que des méta-données **non secrètes** (seed, taille, phase).
En palier `WorldChampionship`, les formules et le champ mortel ne quittent jamais la RAM serveur.

---

## 2. Le noyau déterministe (Couche 1)

### 2.1 Pourquoi la parité bit-à-bit

Toute la létalité et toutes les formules dérivent d'un **hash déterministe** de la seed. Pour que
le serveur et n'importe quel outil de référence (le solveur Python) calculent *exactement* les
mêmes pièges et les mêmes réponses, le hash doit être identique au bit près.

**Périmètre de parité** (garanti, testé) : tout. `CRC32` + `hash_combine` → létalité + formules,
et désormais aussi le *carving* : `FCubeRandom` reproduit au bit près le générateur de numpy
(PCG64 + SeedSequence). Une seed donne le même niveau, le même chemin, les mêmes clés, les mêmes
raccourcis et les mêmes ancrages dans Blender et dans Unreal (seed numérique `7` = seed `"7"`).
Détails et formules : `Solveur_Documentation.md`.

### 2.2 `CubeHash.h`

- `CubeHash::Crc32(const uint8*, int32, uint32 Crc = 0)` — CRC32 IEEE 802.3, polynôme `0xEDB88320`,
  table calculée à la compilation (`constexpr`, thread-safe).
- `CubeHash::Crc32Ascii(const char*)` — CRC32 des constantes (`"PATH"`, `"TRAP"`, `"SIZE"`).
- `CubeHash::Crc32Str(const FString&)` — CRC32 des octets **UTF-8** de la chaîne.
- `CubeHash::Combine(uint32 a, uint32 b)` — `boost::hash_combine` :
  `a ^ (b + 0x9E3779B9 + (a<<6) + (a>>2))`, arithmétique **uint32** (débordement 32 bits).

### 2.3 `CubeRebusLibrary` (`UBlueprintFunctionLibrary`)

| Fonction | Rôle |
|---|---|
| `IsLethal(Seed, X,Y,Z, Rho, SafeSet, N)` | Piège mortel déterministe (voir formule ci-dessous). Une cellule du `SafeSet` n'est jamais mortelle. |
| `IsLethalCell(...)` | Variante Blueprint prenant un `FCubeCoordinate`. |
| `ComputeSafeShortcuts(Seed, PathRoomIDs, N, Rho, Reach = 2)` | Raccourcis sains (PathFinder_Annexes) : détours non mortels à ≤ `Reach` salles du chemin, qui font **gagner** des pas. Linéaire. |
| `MakeFormula(Seed, PathIndex, Pos, SafeDirIndex, Tier, bEnableDecoy)` | Fabrique une formule modulaire dont le résultat pointe la direction sûre. |
| `EvaluateFormula(F, Pos)` | Évalue la réponse sénaire d ∈ {0..5}. |
| `RouteToRLE(PathRoomIDs, N)` | Compresse la route en tokens (direction + longueur). |
| `IndexToDirection(i)` / `DirectionToIndex(Dir)` | Conversion index sénaire ↔ `ECubeDirection`. |

**Champ mortel** (le cœur de la parité) :

```
A      = Combine( Crc32Str(Seed), Crc32Str("TRAP") )
B      = (X*73856093) ^ (Y*19349663) ^ (Z*83492791)     // produits en uint32 (pas de débordement int32)
HFinal = Combine(A, B)
lethal = (HFinal % 10000) < floor(Rho * 10000)           // seuil calculé en double
```

### 2.4 `ECubeDirection` (dans `CubeCore.h`)

```
None=0,  East=1(+X),  West=2(-X),  North=3(+Y),  South=4(-Y),  Top=5(+Z),  Bottom=6(-Z)
```
Axes de **grille**. Dans le monde UE, Y est inversé (voir §2.5) : North = −Y monde, South = +Y monde.
Index sénaire (0..5) = `Dir - 1`. Le résultat d'une formule ∈ {0..5} désigne donc directement la
direction sûre via `IndexToDirection(d+1)`.

### 2.5 Encodage des coordonnées (`FCubeMath`)

- `EncodeCoord(Cell, Nx, Ny)` → `ID = X + Y*Nx + Z*Nx*Ny`.
- `DecodeCoord(ID, Nx, Ny)` → `FCubeCoordinate`.
- `CellToWorld(Cell, N, RoomSize)` — centre la grille : X/Y sur `(N-1)/2`, Z au sol `+0.5`,
  le tout × `RoomSize`, **Y inversé** (Blender est en main droite, UE en main gauche) :
  `((X-h)·S, -(Y-h)·S, (Z+0.5)·S)`. Sans cette inversion, le niveau UE était le miroir du niveau Blender.
- `WorldToCell(World, N, RoomSize)` — inverse, avec `RoundToInt`.
- `DirectionDelta`, `DirectionFromDelta`, `Opposite`, `DirectionToWorld` — conversions de directions.

---

## 3. Compilation (build)

**Cibles** (`Source/*.Target.cs`) — réglées pour UE 5.8 :

```csharp
// Cube.Target.cs
DefaultBuildSettings = BuildSettingsVersion.V7;

// CubeEditor.Target.cs
DefaultBuildSettings = BuildSettingsVersion.V7;
IncludeOrderVersion  = EngineIncludeOrderVersion.Unreal5_8;
```

> ⚠️ **Piège connu** : avec `BuildSettingsVersion.V5` / `Unreal5_5`, UBT échoue avec
> *« CubeEditor modifies the values of properties … not allowed »* (niveaux de warning
> incompatibles avec l'éditeur pré-compilé). Le correctif est exactement ci-dessus.

**Compiler en ligne de commande** :

```bash
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" \
  CubeEditor Win64 Development \
  -Project="C:\...\Cube\Cube.uproject" -WaitMutex
```

Résultat attendu : `Result: Succeeded`, binaire `UnrealEditor-Cube.dll`.

**Lancer l'éditeur** :

```bash
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" \
  "C:\...\Cube\Cube.uproject"
```

Le message *« License not activated »* d'Unreal Build Accelerator est bénin (mode standalone).

---

## 4. Génération / résolution (Couche 2)

### 4.1 `UCubeGenerator`

```cpp
FCubeManifest GenerateCube(FString Seed, int32 GridSize);
FCubeManifest GenerateCubeAdvanced(FString Seed, int32 GridSize,
        float PathLen = 0.45f, float Verticality = 0.18f,
        int32 NKeys = 2, int32 MaxAttempts = 0);           // 0 = jusqu'au succès (borne 100 000)
static FCubeManifest Generate(const FString& Seed, int32 GridSize, double PathLen,
        double Verticality, int32 NKeys, int32 MaxAttempts = 0); // sans UObject (tests, threads)
```

- **Carving** : DFS sur réseau pair, jonction d'exit pour grilles impaires
  (`hi = (N-2) & ~1`, connecteurs tant que `cx < N-2`). Tirages par `FCubeRandom`
  (PCG64 + SeedSequence de numpy) : même chemin que la R&D Python.
- **Boucle de solvabilité** : chaque tentative est seedée par `Combine(PathHash, attempt)` ;
  une tentative dont les clés ne tiennent pas est rejetée (sous-seed suivante), puis
  `UCubeSolver::Solve` valide. Sur les seeds 1 à 50, la tentative 0 suffit toujours.
- **Solveur** : BFS sur l'espace d'états (salle, inventaire, flags) ; portes et clés appliquées.
  Moins d'1 ms par seed en 64³.
- Résultat : `FCubeManifest` (`bIsValid`, `GridSize`, `Seed`, `Attempt`, `StartRoomID`, `ExitRoomID`,
  `Rooms`, `Cells` (grille complète), **`CriticalPath`** — Start→Exit en `RoomID`, = chemin du
  solveur —, `SolutionPath` (états), `SolutionLength`, `NumStates`).

### 4.2 Grilles paires ET impaires

Le générateur gère `N ∈ [8, 64]` (le GameMode borne à `[10, 64]`), pairs et impairs. Validé par `Scripts/test_generator.py`
(36 niveaux, N jusqu'à 50, 0 échec).

---

## 5. Autorité serveur & flux de jeu (Couche 3)

### 5.1 `AGS_CubeGameState` (répliqué, non secret)

| Champ | Réplication | Rôle |
|---|---|---|
| `Seed` | `OnRep_Seed` | Seed du niveau |
| `GridSize` | `Replicated` | N |
| `RoomSize` | `Replicated` | Pitch métrique (cm), défaut 500 |
| `Difficulty` | `Replicated` | Palier courant |
| `Phase` | `OnRep_Phase` | `Booting/Generating/Playing/Solved/Failed` |
| `OnPhaseChanged` | `BlueprintAssignable` | Délégué diffusé à chaque changement de phase |

### 5.2 `AGM_CubeGameMode` (la vérité)

Constructeur : fixe `GameStateClass`, `DefaultPawnClass = ACubeCharacter`,
`PlayerControllerClass = APC_CubePlayerController`.

`BeginPlay → BuildLevel()` :
1. Choix du `UDA_CubeDifficulty` via `StartingDifficulty` dans `DifficultyTable`.
2. `ResolveSeed()` (forcée via `ForcedSeed`, sinon dérivée).
3. `PickGridSize(Seed)` (hash-based, borné par le Data Asset).
4. `Generator->GenerateCubeAdvanced(...)` → `Manifest`.
5. Indexation : `PathRoomIDs`, `SafeSet`, `PathIndexByRoom`.
6. `ShortcutSet = ComputeSafeShortcuts(...)` si `bEnableShortcuts && Rho>0`.
7. Renseigne le GameState, passe en `Playing`.
8. **Ancrages** : spawne (ou réutilise) un `ACubeLevelActor` à l'origine, calcule
   `Socket_Entree` / `Socket_Sortie` (centre de la face extérieure du premier / dernier cube,
   X = vers la passerelle) et accroche `EntryBridgeClass` / `ExitBridgeClass` par leur socket
   `Socket_Cube` (si renseignées).
9. Configure le `UCubeRoomStreamer` (si `RoomClass` défini) et **place les joueurs au départ**.

Méthodes clés :

```cpp
ECubeCellState GetCellState(FCubeCoordinate Cell) const;
bool GetRoomRebus(FCubeCoordinate Cell, FCubeFormula& OutFormula, ECubeDirection& OutSafeHint) const;
void NotifyRoomEntered(AController* Controller, FCubeCoordinate Cell); // serveur uniquement
```

- **`GetRoomRebus`** : pour une cellule du chemin (hors sortie), calcule la direction sûre
  (vers la case suivante du `CriticalPath`) puis la formule via `MakeFormula`. C'est la source
  du HUD en paliers locaux.
- **`NotifyRoomEntered`** (autoritaire) : si sortie → `Phase = Solved` ; si cellule mortelle →
  log + **renvoi au départ** (checkpoint minimal, extensible via un PlayerState).

### 5.3 `UDA_CubeDifficulty` (calibrage)

Champs : `Difficulty`, `Rho`, `GridSizeMin/Max`, `NKeys` (0 à 3), `PathLen`, `Verticality`,
`ShortcutReach` (portée des raccourcis, 2 par défaut), `FormulaTier`, `bEnableDecoy`,
`bCompassEnabled`. **Un asset par palier.**

Table de référence conseillée :

| Palier | Rho | Grille | Compass | Decoy | Tier |
|---|---|---|---|---|---|
| Facile | 0.00 | 10–14 | ✅ | ❌ | 1 |
| Impossible | 0.15–0.30 | 10–50 | ❌ | ✅ | 2 |
| WorldChampionship | 0.30+ | 20–64 | ❌ | ✅ | 3 (secret serveur) |

---

## 6. Mise en place éditeur — pas à pas (Couche 4)

> Tout ci-dessous se fait dans l'éditeur, en **Blueprint/Assets**, sans toucher au C++.

### 6.1 Data Assets de difficulté

1. *Content Browser → Add → Miscellaneous → Data Asset → `DA_CubeDifficulty`.*
2. Nommer `DA_Facile`. Régler `Difficulty=Facile`, `Rho=0`, `GridSizeMin/Max=10/14`,
   `bCompassEnabled=true`, `FormulaTier=1`.
3. Dupliquer en `DA_Impossible` : `Difficulty=Impossible`, `Rho=0.2`, `10/50`,
   `bCompassEnabled=false`, `bEnableDecoy=true`, `FormulaTier=2`.

### 6.2 Blueprint GameMode

1. *Blueprint Class → parent `GM_CubeGameMode` → `BP_CubeGameMode`.*
2. Class Defaults :
   - `DifficultyTable` : ajouter `Facile → DA_Facile`, `Impossible → DA_Impossible`.
   - `StartingDifficulty = Facile` (pour débuter).
   - `RoomClass = BP_CubeRoom` (créé en §6.4).
   - `StreamRadius = 3`, `bEnableShortcuts = true`.
   - `ForcedSeed` : mettre une valeur (ex. `demo`) pour un niveau reproductible en test.

### 6.3 Blueprint PlayerController + Enhanced Input

1. Créer 6 `InputAction` (type **Digital/bool**) :
   `IA_MoveEast, IA_MoveWest, IA_MoveNorth, IA_MoveSouth, IA_MoveTop, IA_MoveBottom`.
2. Créer un `InputMappingContext` `IMC_Cube` et y mapper des touches, ex. :
   - East → `E` ou `→`, West → `A`/`←`, North → `W`/`↑`, South → `S`/`↓`,
     Top → `Space`, Bottom → `Left Ctrl`.
3. *Blueprint Class → parent `PC_CubePlayerController` → `BP_CubePlayerController`.*
4. Class Defaults : assigner `MappingContext = IMC_Cube` et les 6 `IA_Move*`.
5. HUD (facultatif ici) : implémenter les events `OnFormulaUpdated` et `OnCompassUpdated`
   (voir §6.5).

> Le C++ lie automatiquement chaque action à un pas discret via `ETriggerEvent::Triggered`.
> Un appui = une tentative de pas ; le serveur valide les bornes et arbitre la mort.

### 6.4 Blueprint Room + matériau DebugColor

1. *Blueprint Class → parent `ACubeRoom` → `BP_CubeRoom`.*
2. Sélectionner `ShellMesh`, lui donner un mesh (ex. `Cube` moteur), échelle adaptée à
   `RoomSize` (500 cm par défaut → cube de 4–4.5 m pour laisser des couloirs).
3. Créer un matériau `M_Room` avec un paramètre **vectoriel nommé exactement `DebugColor`**
   branché sur *Base Color* (et/ou *Emissive*). L'assigner au `ShellMesh` (slot 0).
   Le C++ crée un MID et pousse la couleur d'état (turquoise/vert/rouge/… voir §2.3 & code).

### 6.5 HUD (WBP) — optionnel mais recommandé

Dans `BP_CubePlayerController`, implémenter :
- `OnFormulaUpdated(Formula, bHasFormula)` → afficher `A1..A4`, `Mod`, `Tier` dans un WBP.
- `OnCompassUpdated(SafeHint, bCompassEnabled)` → afficher une flèche si `bCompassEnabled`.
- Écran de fin : lier `AGS_CubeGameState::OnPhaseChanged` → afficher Victoire (`Solved`) /
  Échec (`Failed`).

Le rafraîchissement est automatique : le pawn appelle `RefreshRoomHUD()` à chaque changement
de cellule (déplacement, spawn, respawn).

### 6.6 Map & activation

1. Créer un niveau vide `L_Cube` (juste une `PlayerStart` et une lumière).
2. *World Settings → GameMode Override = `BP_CubeGameMode`.*
3. Sauver. **Ne pas** modifier `Project Settings → Maps & Modes` (le GameMode par défaut
   du projet reste inchangé, conformément à l'architecture parallèle).

---

## 7. Tester (chaque étape)

### 7.1 Tests runnables (hors éditeur) — la base de vérité

```bash
python Scripts/test_parity.py      # parité hash/létalité/formules : delta attendu = 0 (23000 cas)
python Scripts/test_generator.py   # solvabilité paires+impaires : 0 échec (N jusqu'à 50)
```

Ces deux scripts valident la Couche 1 et la Couche 2 **indépendamment de l'éditeur**. Ils sont
la garantie que le noyau déterministe reste correct après toute modification.

### 7.2 Tests d'automation UE (Couche 1 en moteur)

`Source/Cube/Private/Tests/CubeRebusTests.cpp` (sous `#if WITH_DEV_AUTOMATION_TESTS`) :
`Cube.Rebus.HashRegression`, `Cube.Rebus.LethalRegression`, `Cube.Rebus.FormulaRegression`,
`Cube.Generator.OddGridSolvable`, `Cube.Rebus.ShortcutStrictness`, `Cube.Clue.*`,
`Cube.Rebus.StrictUniquePath`.

`Source/Cube/Private/Tests/CubeSolverParityTest.cpp` : **`Cube.Solver.Parity`** — seeds 1 à 50,
parité bit à bit avec la R&D (tirage aléatoire, cellules, chemin, états, ancrages, raccourcis).

Éditeur : *Tools → Session Frontend → Automation → filtrer « Cube » → Start*.
Ligne de commande :

```bash
UnrealEditor-Cmd.exe "C:\...\Cube.uproject" -ExecCmds="Automation RunTests Cube;Quit" \
  -unattended -nop4 -nullrhi
```

### 7.3 Test en jeu (PIE)

1. Ouvrir `L_Cube`, *Play*.
2. Se déplacer avec les 6 touches ; le pawn saute de cellule en cellule (grille discrète).
3. Suivre le chemin sûr (turquoise) ; une cellule rouge renvoie au départ.
4. Atteindre la sortie (or) → phase `Solved`.

---

## 8. Réseau & sécurité

- **Serveur-autoritaire** : `ServerMove` (Reliable) valide chaque pas ; le client ne fait
  jamais autorité sur sa position (`CurrentCell` est `ReplicatedUsing=OnRep_CurrentCell`).
- **Secret serveur** : en `WorldChampionship`, ne jamais répliquer formules/champ mortel.
  `GetRoomRebus` s'appuie sur le GameMode (présent seulement côté serveur/standalone) ; un
  client pur reçoit un HUD vierge — c'est voulu.
- Le GameState ne porte que des méta-données publiques.

---

## 9. Dépannage

| Symptôme | Cause | Correctif |
|---|---|---|
| `… not allowed, has build products in common with UnrealEditor` | `BuildSettingsVersion.V5` | Passer à `V7` + `Unreal5_8` (§3) |
| `C2027 utilisation du type non défini 'ACubeRoom'` | `TSubclassOf<ACubeRoom>` déréférencé sans définition | `#include "CubeRoom.h"` dans le `.cpp` |
| Aucune salle ne s'affiche | `RoomClass` non assignée sur le GameMode | Renseigner `RoomClass = BP_CubeRoom` (§6.2) |
| Salles toutes grises | matériau sans paramètre `DebugColor` | Ajouter le paramètre vectoriel `DebugColor` (§6.4) |
| Le pawn ne bouge pas | `IMC`/`IA` non assignés | Vérifier `BP_CubePlayerController` (§6.3) |
| HUD vide en client réseau | GameMode absent côté client | Normal en compétition ; répliquer un sous-ensemble si palier local |

---

## 10. Cartographie des fichiers

```
Source/Cube/Public|Private/
  CubeCore.h                      ECubeDirection, FCubeCoordinate, FCubeMath (+ CellToWorld/WorldToCell)
  CubeLogicTypes.h                FCubeLogicalRoom, FCubeManifest (+ CriticalPath)
  CubeHash.h                      CRC32 + Combine (parité)
  CubeRandom.*                    FCubeRandom : PCG64 + SeedSequence (parité numpy)
  CubePortalLibrary.*             FCubePortal : ancrages d'entrée / sortie, transform de passerelle
  CubeViewLibrary.*               Salles à instancier (aperçu, enveloppe visible, coupe Z)
  CubeLevelActor.*                Socket_Entree / Socket_Sortie répliqués + aperçu instancié
  CubeRebusTypes.h                ECubeDifficulty, ECubeCellState, FCubeFormula, FCubeRoomData…
  CubeRebusLibrary.*              Cœur algorithmique (létalité, raccourcis, formules)
  CubeGenerator.*                 Génération + boucle de solvabilité
  CubeSolver.*                    Résolution (validation d'un manifest)
  DA_CubeDifficulty.h             Calibrage par palier
  GS_CubeGameState.*              État répliqué non secret
  GM_CubeGameMode.*               Autorité serveur, arbitrage, GetRoomRebus
  CubeCharacter.*                 Pawn grille discrète (ServerMove, CurrentCell répliqué)
  PC_CubePlayerController.*       Enhanced Input + hooks HUD
  CubeRoom.*                      Acteur salle (mesh + couleur d'état)
  CubeRoomStreamer.*              Streaming/pooling autour du joueur (WorldSubsystem)
  Tests/CubeRebusTests.cpp        Tests d'automation (régression, solvabilité, indices)
  Tests/CubeSolverParityTest.cpp  Parité bit à bit générateur/solveur/ancrages (seeds 1-50)

Scripts/
  cube_solveur_blender_3d.py      Référentiel Python (source de vérité de la parité)
  test_parity.py                  Parité C++↔Python (23000 cas)
  test_generator.py               Solvabilité grilles paires+impaires
```

---

## 11. Feuille de route (Étape 6+)

- **UMG Rébus** : saisie de la réponse sénaire → conversion en `ECubeDirection` → pas.
- **Écrans Victoire/Échec** branchés sur `OnPhaseChanged`.
- **Clés/Portes** : exploiter `KeyBit`/`GateBit` de `FCubeLogicalRoom` pour verrouiller des
  segments du chemin.
- **PlayerState** : vies, score, checkpoints enrichis (au lieu du renvoi-au-départ simple).
- **Palier WorldChampionship** : validateur serveur des réponses, anti-triche, seeds signées.
- **Habillage** : remplacer les couleurs DEBUG par un vrai matériau de salle + VFX de piège.

---

*Dernière vérification du manuel contre les sources : voir `git log` de la branche courante.*
