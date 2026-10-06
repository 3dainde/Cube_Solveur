# Guide pas à pas dans Unreal Engine — rendre le jeu « Cube » jouable

> Checklist à suivre **dans l'ordre**, clic par clic, dans l'éditeur UE 5.8.
> Coche chaque case au fur et à mesure. À la fin, tu joues en PIE.
> Le code C++ est déjà compilé — tu ne touches ici qu'aux **assets et Blueprints**.
>
> Détails techniques et dépannage : voir `Manuel_Implementation_Jeu.md`.

---

## Récap express (ce que tu vas faire) (voir plus bas)

| # | Étape | Résultat |
|---|---|---|
| A | Ouvrir le projet compilé | Éditeur ouvert |
| B | 2 Data Assets de difficulté | `DA_Facile`, `DA_Impossible` |
| C | Matériau de salle `DebugColor` | `M_Room` + `MI_Room` |
| D | Blueprint de salle | `BP_CubeRoom` |
| E | 6 InputActions + 1 Mapping Context | `IA_Move*`, `IMC_Cube` |
| F | Blueprint PlayerController | `BP_CubePlayerController` |
| G | Blueprint GameMode | `BP_CubeGameMode` |
| H | Niveau + activation du mode | `L_Cube` jouable |
| I | Test PIE | Tu te déplaces et gagnes |

> Astuce : crée d'abord un dossier propre. Content Browser → clic droit → *New Folder* →
> `Content/Cube`. Range tout dedans.

---

## A. Ouvrir le projet

- [ ] Ouvre `Cube.uproject` (double-clic) **ou** via l'éditeur déjà lancé.
- [ ] Si UE propose de recompiler des modules, accepte (« Yes »).
- [ ] **Vérif** : dans *Content Browser*, active *Settings → Show C++ Classes*.
      Tu dois voir le dossier **C++ Classes → Cube** avec `GM_CubeGameMode`,
      `PC_CubePlayerController`, `CubeCharacter`, `CubeRoom`, `DA_CubeDifficulty`, etc.

---

## B. Créer les Data Assets de difficulté

### B.1 — DA_Facile
- [ ] *Content Browser → Add (+) → Miscellaneous → Data Asset*.
- [ ] Dans la liste des classes, choisis **`DA_CubeDifficulty`**.
- [ ] Nomme l'asset **`DA_Facile`**. Double-clique pour l'ouvrir.
- [ ] Règle les champs :
  - `Difficulty` = **Facile**
  - `Rho` = **0.0**  *(aucune mort)*
  - `Grid Size Min` = **10**, `Grid Size Max` = **14**
  - `NKeys` = **0**
  - `Path Len` = **0.35**
  - `Verticality` = **0.15**
  - `Formula Tier` = **1**
  - `Enable Decoy` = **décoché**
  - `Compass Enabled` = **coché**
  - *(catégorie **Cube|Clue**)* `Strict Unique Path` = **décoché**
  - *(catégorie **Cube|Clue**)* `Clue Interval` = **5**  *(un indice tous les 5 pas)*
  - *(catégorie **Cube|Clue**)* `Show Coordinates` = **coché**  *(Facile affiche X/Y/Z)*
- [ ] *Save*.

### B.2 — DA_Impossible
- [ ] Clic droit sur `DA_Facile` → *Duplicate* → nomme **`DA_Impossible`**.
- [ ] Ouvre-le et change :
  - `Difficulty` = **Impossible**
  - `Rho` = **0.2**
  - `Grid Size Min` = **10**, `Grid Size Max` = **50**
  - `NKeys` = **2**
  - `Formula Tier` = **2**
  - `Enable Decoy` = **coché**
  - `Compass Enabled` = **décoché**
  - `Show Coordinates` = **décoché**  *(le joueur n'a plus les coordonnées)*
  - `Strict Unique Path` = **décoché** par défaut. **Coché** = mode « chemin unique strict » :
    toute zone saine qui court-circuiterait le chemin devient mortelle (voir étape I).
- [ ] *Save*.

> **Vérif B** : tu as 2 assets `DA_Facile` et `DA_Impossible` dans `Content/Cube`.

> **Note — 3ᵉ difficulté** : le code expose aussi `WorldChampionship` (`ECubeDifficulty`), palier
> où formules et champ mortel restent secrets côté serveur. Tu peux créer un `DA_WorldChampionship`
> sur le même modèle et l'ajouter à la `Difficulty Table` (étape G) quand tu passeras en réseau.

---

## C. Créer le matériau de salle (paramètre `DebugColor`)

Le C++ colore chaque salle selon son état via un paramètre vectoriel **nommé exactement
`DebugColor`**. Sans lui, toutes les salles restent grises.

- [ ] *Add (+) → Material* → nomme **`M_Room`**. Double-clique.
- [ ] Dans le graphe : clic droit → cherche **`VectorParameter`** → ajoute-le.
- [ ] Sélectionne le nœud, dans *Details* mets **Parameter Name = `DebugColor`** (respecte
      la casse **exactement**).
- [ ] Relie la sortie du `DebugColor` au **Base Color** du nœud résultat.
- [ ] (Option) Relie-la aussi à **Emissive Color** (avec un `Multiply` par ~2) pour que les
      couleurs ressortent sans éclairage.
- [ ] *Apply* puis *Save*.
- [ ] Clic droit sur `M_Room` → *Create Material Instance* → nomme **`MI_Room`** (on assignera
      celui-ci au mesh).

> **Vérif C** : `M_Room` a bien un paramètre `DebugColor` visible dans `MI_Room`.

---

## D. Créer le Blueprint de salle `BP_CubeRoom`

- [ ] *Add (+) → Blueprint Class*. En bas, déplie *All Classes*, cherche **`CubeRoom`**,
      sélectionne-le comme parent. Nomme **`BP_CubeRoom`**.
- [ ] Ouvre `BP_CubeRoom`. Dans le *Components*, sélectionne **`ShellMesh`**.
- [ ] Dans *Details → Static Mesh*, choisis un cube : `Engine → BasicShapes → Cube`
      *(active « Show Engine Content » si besoin)*.
- [ ] *Details → Materials → Element 0* = **`MI_Room`**.
- [ ] **Échelle** : la grille place les salles tous les `RoomSize` = 500 cm. Le cube moteur fait
      100 cm. Mets *Scale* du ShellMesh à **~4.0** (soit 4 m) pour laisser un interstice visible
      entre salles. Ajuste à ton goût (3.5–4.5).
- [ ] *Compile* → *Save*.

> **Vérif D** : `BP_CubeRoom` montre un cube coloré (couleur par défaut du MI) dans le viewport.

---

## E. Enhanced Input — 6 actions + 1 contexte

### E.1 — Les 6 InputActions
Pour **chacune** des 6 : *Add (+) → Input → Input Action*. Laisse **Value Type = Digital (bool)**.
Nomme-les exactement :
- [ ] `IA_MoveEast`
- [ ] `IA_MoveWest`
- [ ] `IA_MoveNorth`
- [ ] `IA_MoveSouth`
- [ ] `IA_MoveTop`
- [ ] `IA_MoveBottom`

### E.2 — Le Mapping Context
- [ ] *Add (+) → Input → Input Mapping Context* → nomme **`IMC_Cube`**. Ouvre-le.
- [ ] Ajoute 6 *Mappings* (bouton +), un par action, avec une touche :

| Action | Touche suggérée | Sens |
|---|---|---|
| `IA_MoveEast` | **D** | +X |
| `IA_MoveWest` | **A** | −X |
| `IA_MoveNorth` | **W** | +Y grille (= −Y monde UE) |
| `IA_MoveSouth` | **S** | −Y grille (= +Y monde UE) |
| `IA_MoveTop` | **Space** | +Z (monter) |
| `IA_MoveBottom` | **Left Ctrl** | −Z (descendre) |

- [ ] *Save*.

> Note : la grille est convertie vers UE avec Y inversé, pour que le niveau soit identique à celui
> de Blender (et non son miroir). North (+Y grille) va donc vers **−Y monde**. Si W doit rester
> « vers l'avant » selon ta caméra, échange simplement les touches de North et South.

> **Vérif E** : 6 `IA_Move*` + `IMC_Cube` avec 6 mappings.

---

## F. Blueprint PlayerController `BP_CubePlayerController`

- [ ] *Add (+) → Blueprint Class* → parent **`PC_CubePlayerController`** → nomme
      **`BP_CubePlayerController`**. Ouvre-le.
- [ ] Onglet *Class Defaults* (bouton en haut). Dans la catégorie **Cube|Input**, assigne :
  - `Mapping Context` = **`IMC_Cube`**
  - `IA Move East` = **`IA_MoveEast`**
  - `IA Move West` = **`IA_MoveWest`**
  - `IA Move North` = **`IA_MoveNorth`**
  - `IA Move South` = **`IA_MoveSouth`**
  - `IA Move Top` = **`IA_MoveTop`**
  - `IA Move Bottom` = **`IA_MoveBottom`**
  - `Mapping Priority` = **0**
- [ ] *Compile* → *Save*.

> **Vérif F** : les 7 champs de *Cube|Input* sont renseignés (aucun « None »).

---

## G. Blueprint GameMode `BP_CubeGameMode`

- [ ] *Add (+) → Blueprint Class* → parent **`GM_CubeGameMode`** → nomme **`BP_CubeGameMode`**.
      Ouvre-le → *Class Defaults*.
- [ ] Catégorie **Cube|Difficulty** :
  - `Difficulty Table` : clique **+** deux fois, crée les entrées :
    - Clé **Facile** → Valeur **`DA_Facile`**
    - Clé **Impossible** → Valeur **`DA_Impossible`**
  - `Starting Difficulty` = **Facile**
- [ ] Catégorie **Cube|Geometry** :
  - `Room Size` = **500.0**
  - `Forced Seed` = **`demo`**  *(niveau reproductible pour tester ; vide = aléatoire)*
- [ ] Catégorie **Cube|PathFinder** : `Enable Shortcuts` = **coché**.
- [ ] Catégorie **Cube|Streaming** :
  - `Room Class` = **`BP_CubeRoom`**
  - `Stream Radius` = **3**
- [ ] Catégorie **Cube|Portal** (optionnel) :
  - `Level Actor Class` = **`CubeLevelActor`** (déjà par défaut) : porte `Socket_Entree` et `Socket_Sortie`.
  - `Entry Bridge Class` / `Exit Bridge Class` = ton Blueprint de passerelle. Il doit avoir un socket
    **`Socket_Cube`** (axe X vers l'extérieur du cube) et **Replicates** coché. Vide = pas de passerelle.
- [ ] **(Important)** Le pawn et le controller sont déjà fixés en C++, mais **remplace le
      controller par ton BP** pour que l'input marche :
  - Catégorie *Classes* → `Player Controller Class` = **`BP_CubePlayerController`**.
  - Laisse `Default Pawn Class` = **`CubeCharacter`** (ou un BP dérivé si tu en fais un).
- [ ] *Compile* → *Save*.

> **Vérif G** : `Difficulty Table` a 2 lignes, `Room Class` = `BP_CubeRoom`,
> `Player Controller Class` = `BP_CubePlayerController`.

---

## H. Créer le niveau et activer le mode

- [ ] *File → New Level → Empty Level* (ou Basic). *Save As* → **`L_Cube`** dans `Content/Cube`.
- [ ] Place au moins : une **Directional Light** (ou Skylight) et un **Player Start**
      (*Place Actors → Basic → Player Start*), n'importe où — la position réelle du joueur est
      pilotée par le code (cellule de départ).
- [ ] *Window → World Settings*. Dans **Game Mode → GameMode Override**, choisis
      **`BP_CubeGameMode`**.
- [ ] *Save* le niveau.

> ⚠️ Ne touche **pas** à *Project Settings → Maps & Modes* : le mode Cube s'active **par
> niveau** (World Settings), pas globalement. C'est voulu (classes parallèles).

> **Vérif H** : *World Settings → GameMode Override* affiche `BP_CubeGameMode`.

---

## I. Tester en PIE (Play In Editor)

- [ ] Ouvre `L_Cube`. Clique **Play** (Alt+P).
- [ ] Regarde l'*Output Log* (*Window → Output Log*) : tu dois voir
      `[Cube] Niveau prêt : seed=demo N=… |chemin|=… raccourcis=… interdits=… Rho=0.00 strict=0`,
      puis `[Cube] Ancrages : entrée … face 2, sortie … face 1.` (West = 2, East = 1).
      *(`interdits=` = cellules bloquées en mode chemin unique strict ; `strict=1` si `Strict Unique Path` coché.)*
- [ ] Déplace-toi : **W/A/S/D** (horizontal), **Space/Ctrl** (haut/bas). Chaque appui = un pas
      d'une cellule.
- [ ] Suis les salles **turquoise** (chemin sûr). Les **vertes** sont des raccourcis sains.
- [ ] En **Facile** (`Rho=0`), aucune mort : entraîne-toi à naviguer.
- [ ] Atteins la salle **or** (sortie) → l'*Output Log* affiche
      `[Cube] Sortie atteinte — niveau résolu.` (phase `Solved`).

### Passer en mode létal
- [ ] Stoppe le PIE. Ouvre `BP_CubeGameMode` → `Starting Difficulty` = **Impossible**.
      *Compile/Save*. Rejoue : les cellules **rouges** renvoient au départ.

### Passer en mode « chemin unique strict »
- [ ] Ouvre `DA_Impossible` → coche `Strict Unique Path` (catégorie *Cube|Clue*). *Save*, rejoue.
      Les raccourcis verts disparaissent : toute zone saine reliant deux portions du chemin devient
      mortelle. Seuls restent sûrs le chemin du solveur et les culs-de-sac. Le log affiche `strict=1`
      et `interdits=` > 0.

---

## Dépannage rapide (si ça coince)

| Problème | Vérifie |
|---|---|
| Rien ne s'affiche (pas de salles) | `Room Class = BP_CubeRoom` dans `BP_CubeGameMode` (G) |
| Salles toutes de la même couleur | paramètre matériau nommé **`DebugColor`** exact (C) + `MI_Room` sur le mesh (D) |
| Le perso ne bouge pas | `IMC_Cube` + 6 `IA_Move*` assignés dans `BP_CubePlayerController` (F) **et** `Player Controller Class = BP_CubePlayerController` (G) |
| Salles qui se chevauchent / trop espacées | *Scale* du `ShellMesh` (D) vs `Room Size` 500 (G) |
| « GameMode Override » vide au lancement | tu as oublié H : le World Settings du niveau |
| Log « Génération échouée » | grille trop petite/serrée : élargis `Grid Size Min/Max` du Data Asset (B) |

---

## Ordre de dépendance (pourquoi cet ordre)

```
DA_Facile/DA_Impossible ─┐
M_Room → MI_Room → BP_CubeRoom ─┐
IA_Move* → IMC_Cube → BP_CubePlayerController ─┤
                                               ├─→ BP_CubeGameMode → L_Cube → PLAY
```

Chaque brique est référencée par la suivante : respecte l'ordre B→H et tout se branche.
Quand tu changes une difficulté, un mapping ou l'échelle d'une salle, tu n'as qu'à
*Compile/Save* l'asset concerné — pas besoin de recompiler le C++.
