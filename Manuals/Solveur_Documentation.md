# Cube Solveur — Documentation du solveur

Implémentation de référence : **C++ UE 5.8**, module `Cube`, dossier `Source/Cube/{Public,Private}/`.
La R&D Python (Blender) a servi de modèle ; le C++ en est un portage **bit à bit** : une seed donne exactement le même niveau, le même chemin, les mêmes clés et les mêmes ancrages dans Blender et dans Unreal (vérifié sur les seeds 1 à 50 et 90 configurations, voir §11). Les seeds sont des chaînes : la seed numérique `7` de Blender correspond à la seed `"7"` dans Unreal.

Notations : $N$ = côté de la grille ($N_x = N_y = N_z = N$, $8 \le N \le 64$), $s$ = taille d'une salle, $\oplus$ = XOR, toute l'arithmétique entière de hachage est **modulo $2^{32}$** (`uint32`).

| Fichier | Rôle |
|---|---|
| `CubeCore.h` | `ECubeDirection`, `ECubeCellType`, `FCubeCoordinate`, `FCubeState`, `FCubeMath` (encodage, `CellToWorld`, directions) |
| `CubeLogicTypes.h` | `FCubeManifest` (grille `Cells`, salles, `CriticalPath`, `SolutionPath`), `FCubeLogicalRoom` |
| `CubeHash.h` | CRC-32 (table `constexpr`), `Crc32Str` (UTF-8), `Combine` |
| `CubeRandom.h/.cpp` | `FCubeRandom` : PCG64 + SeedSequence, parité numpy |
| `CubeGenerator.h/.cpp` | `UCubeGenerator` : chemin auto-évitant, clés/portes, sous-seeds |
| `CubeSolver.h/.cpp` | `UCubeSolver` : BFS sur l'espace d'états |
| `CubeRebusLibrary.h/.cpp` | Pièges mortels, raccourcis sûrs, chemin unique strict, formules, indices |
| `CubePortalLibrary.h/.cpp` | `FCubePortal`, ancrages et transform de passerelle |
| `CubeViewLibrary.h/.cpp` | Salles à instancier (enveloppe visible, coupe Z, matériaux) |
| `CubeLevelActor.h/.cpp` | `ACubeLevelActor` : `Socket_Entree` / `Socket_Sortie` répliqués, aperçu instancié |
| `GM_CubeGameMode.*` | Génère le niveau, spawne l'acteur niveau, accroche les passerelles |
| `Tests/CubeSolverParityTest.cpp` | Test d'automatisation `Cube.Solver.Parity` |

---

## §1 Coordonnées et identité mathématique

Chaque salle a un identifiant unique :

$$\mathrm{ID}(x, y, z) = x + y\,N_x + z\,N_x N_y \qquad 0 \le \mathrm{ID} < N_x N_y N_z$$

Décodage (divisions entières) :

$$z = \left\lfloor \frac{\mathrm{ID}}{N_x N_y} \right\rfloor,\quad r = \mathrm{ID} \bmod N_x N_y,\quad y = \left\lfloor \frac{r}{N_x} \right\rfloor,\quad x = r \bmod N_x$$

En 64³, $\mathrm{ID} < 2^{18} = 262\,144$ : il tient sur 18 bits.

**Position d'une salle.** La grille est centrée en X/Y et posée sur le sol en Z. Dans le repère de la R&D (main droite, Blender) :

$$C(x, y, z) = \left(x s - o_x,\; y s - o_y,\; z s + \tfrac{s}{2}\right), \qquad o_x = \tfrac{(N_x - 1)s}{2},\; o_y = \tfrac{(N_y - 1)s}{2}$$

Le volume occupe $[-\tfrac{N s}{2}, \tfrac{N s}{2}]^2 \times [0, N s]$.

**Passage dans Unreal** (main gauche, cm) : on inverse Y. Avec $s$ = `RoomSize` en cm (500 en jeu ; 100 = 1 m Blender) :

$$C_{UE} = (C_x,\; -C_y,\; C_z) \qquad \texttt{FCubeMath::CellToWorld}$$

Les vecteurs (normales, axes) se convertissent de la même façon (`FCubeMath::DirectionToWorld`). Conséquence : la direction grille North (+Y) pointe vers **−Y monde** dans Unreal, South vers +Y monde. La grille est posée à l'origine du monde ; `ACubeLevelActor` est spawné à l'origine, sans rotation.

---

## §2 Topologie

Six directions, dans un ordre **contractuel** : c'est l'ordre de parcours des voisins du BFS (il fixe les parents, donc le chemin retenu à égalité) et l'index sénaire des formules du rébus.

| Index sénaire | `ECubeDirection` (valeur) | Vecteur grille | Vecteur monde UE |
|---|---|---|---|
| 0 | East (1) | $(+1, 0, 0)$ | $(+1, 0, 0)$ |
| 1 | West (2) | $(-1, 0, 0)$ | $(-1, 0, 0)$ |
| 2 | North (3) | $(0, +1, 0)$ | $(0, -1, 0)$ |
| 3 | South (4) | $(0, -1, 0)$ | $(0, +1, 0)$ |
| 4 | Top (5) | $(0, 0, +1)$ | $(0, 0, +1)$ |
| 5 | Bottom (6) | $(0, 0, -1)$ | $(0, 0, -1)$ |

`None` = 0 ; index sénaire = valeur − 1 (`UCubeRebusLibrary::DirectionToIndex`).

Le voisin de $(x, y, z)$ dans la direction $d$ est $(x, y, z) + \vec{d}$, **valide si et seulement si** il reste dans $[0, N)^3$ et n'est pas un mur.

> Le test « distance de Manhattan = 1 » de la version précédente est vrai par construction dès que le voisin est calculé sur les coordonnées décodées. Le seul piège réel est de calculer le voisin directement sur l'ID ($\mathrm{ID} \pm 1$, $\pm N_x$, $\pm N_x N_y$) : on passe alors d'une ligne à l'autre sans le voir. Le test qui compte est le **test de bord** sur $(x, y, z)$.

---

## §3 Modèle d'état

$$S = \langle \mathrm{Room},\ \mathrm{Orientation},\ \mathrm{Inventory},\ \mathrm{Flags} \rangle$$

- $\mathrm{Inventory}$ : bit $j$ = clé $j$ détenue (3 bits, au plus 3 clés).
- $\mathrm{Flags}$ : bit $j$ = porte $j$ franchie.
- $\mathrm{Orientation}$ : 3 bits, réservée (constante dans le modèle actuel).

Empaquetage sur 27 bits (`FCubeState::Pack`) :

$$P = \mathrm{Room} \;|\; (\mathrm{Inv} \ll 18) \;|\; (\mathrm{Flags} \ll 21) \;|\; (\mathrm{Ori} \ll 24)$$

Borne de l'espace : $|S| \le N^3 \cdot 2^9 = 2^{27}$ en 64³. L'espace **atteignable** est minuscule (quelques centaines d'états, §4).

**Transitions.** Depuis $S$ vers la salle voisine $v$ ouverte :

- $v$ porte de bit $b$ : autorisé seulement si $\mathrm{Inv} \,\&\, b \neq 0$ ; alors $\mathrm{Flags} \mathrel{|}= b$.
- $v$ clé de bit $b$ : $\mathrm{Inv} \mathrel{|}= b$.
- sinon : état inchangé hors salle.

---

## §4 Solveur BFS

Parcours en largeur sur le graphe des états : chaque transition coûte 1, donc **le premier état atteint dont la salle est la sortie donne un chemin de longueur minimale** (propriété du BFS sur graphe à poids unitaires).

Implémentation (`UCubeSolver::Solve`) :

1. **Index des salles ouvertes.** Le BFS ne visite jamais un mur. On numérote les salles ouvertes ($k \approx$ 300 à 1 200 en 64³) et on précalcule leurs 6 voisins ouverts. Indexer les 262 144 salles coûtait 95 % du temps.
2. **Ensemble visité** : tableau de $k \times 512$ octets indexé par (salle ouverte, $\mathrm{Inv} \,|\, \mathrm{Flags} \ll 3 \,|\, \mathrm{Ori} \ll 6$), au lieu d'un `TSet<FCubeState>` haché.
3. **File = table des parents** : chaque nœud de la file garde l'index de son parent ; le chemin se reconstruit en remontant (l'ancienne version faisait `Queue.RemoveAt(0)`, en $O(n)$ par pop).
4. **Sortie** : `SolutionPath` (états), `CriticalPath` (salles), `SolutionLength`, `NumStates`.

Complexité : $O(|V| + |E|)$ avec $|E| \le 6|V|$. Mesuré : **< 1 ms** par seed en 64³.

`NumStates` = états **découverts** (et non seulement dépilés) au moment où la sortie est dépilée, comme la R&D.

---

## §5 Générateur

### 5.1 Flux aléatoire d'une tentative

$$H_{seed} = \mathrm{CRC32}(\mathrm{UTF8}(seed)),\quad H_{path} = \mathrm{Combine}(H_{seed}, \mathrm{CRC32}(\texttt{"PATH"})),\quad R = \mathrm{Combine}(H_{path}, \mathrm{attempt})$$

$$\mathrm{Combine}(a, b) = a \oplus \left(b + \texttt{0x9E3779B9} + (a \ll 6) + (a \gg 2)\right) \pmod{2^{32}}$$

Pour une seed numérique, $\mathrm{UTF8}(seed)$ = son écriture décimale (« 1 », « 50 »), comme `str(seed)` en Python. Le générateur de la tentative est `FCubeRandom(R)` (§9) ; l'ancien `FRandomStream` est abandonné.

### 5.2 Chemin auto-évitant

Le chemin est creusé sur le **réseau des nœuds pairs** $\{2, 4, \dots, h\}^3$ avec $h = (N - 2) \,\&\, \lnot 1$ ; deux nœuds voisins sont séparés de 2 salles, la salle intermédiaire est le « mur franchi ».

- Départ $A = (2, c, c)$ avec $c = \lfloor N/2 \rfloor \,\&\, \lnot 1$ ; cible $T = (h, e_y, e_z)$, $e_y$ puis $e_z$ tirés uniformément parmi les valeurs paires de $[2, h]$.
- DFS avec retour arrière. Depuis le sommet de pile $p$, chaque voisin non visité $q$ (pas de ±2 sur un axe) reçoit le poids

$$w(q) = w_{axe} \times \begin{cases} \tau & \text{si } \lVert q - T \rVert_1 < \lVert p - T \rVert_1 \\ 1 & \text{sinon} \end{cases}, \qquad w_{axe} = \begin{cases} 1 & \text{pas en X ou Y} \\ \nu & \text{pas en Z} \end{cases}$$

  avec $\nu$ = `Verticality` et $\tau = \mathrm{clamp}(12^{\,1-\ell},\ 2,\ 14)$, $\ell$ = `PathLength`.
- Le voisin est tiré avec la probabilité $w(q) / \sum w$ ; sans voisin libre, on dépile.

> **Correction.** $12^{1-\ell} \in [1, 12]$ pour $\ell \in [0, 1]$ : la borne 14 n'est jamais atteinte. Le biais effectif est $\tau \in [2, 12]$, et il vaut 2 pour tout $\ell \ge 1 - \log_{12} 2 \approx 0{,}721$ (au-delà, la longueur ne change plus rien).

Le chemin complet : entrée $(0, c, c)$, connecteur $(1, c, c)$, puis chaque nœud de la pile suivi du milieu vers le nœud suivant, raccord en X jusqu'à $N - 2$ (si $N$ est impair), sortie $(N - 1, e_y, e_z)$. Comme seuls les murs franchis sont ouverts, deux salles ouvertes ne sont voisines que si elles se suivent : **le niveau est un couloir unique**, sans raccourci ni cul-de-sac.

### 5.3 Clés et portes

Soit $L$ le nombre de nœuds de la pile et $K$ le nombre de clés. Si $L < 4K + 2$, la tentative est rejetée. Sinon, pour $j = 0 \dots K-1$ :

$$k_j = \mathrm{clamp}\!\left(\mathrm{round}_{\frac12 \to \text{pair}}\!\left(\frac{j+1}{K+1}\,(L-1)\right),\ 1,\ L-4\right)$$

La clé $j$ est sur le nœud $k_j$, la porte $j$ sur le nœud $k_j + 2$, soit **4 salles plus loin** sur le chemin. Si deux indices se chevauchent, la tentative est rejetée.

> **Correction.** L'arrondi est l'arrondi bancaire (au pair, celui de Python). `FMath::RoundToInt` arrondit $x{,}5$ vers le haut et décalerait une clé quand $\frac{j+1}{K+1}(L-1)$ tombe sur un demi-entier (cas fréquent avec $K = 1$ ou $K = 3$). Voir `CubeMath::RoundHalfToEven`.

### 5.4 Sous-seeds

`UCubeGenerator::Generate` essaie `attempt = 0, 1, 2…` jusqu'au premier niveau valide **et** résolu par le BFS (`MaxAttempts = 0`). Sur les seeds 1 à 50 avec les paramètres par défaut, la tentative 0 suffit toujours. Une borne de 100 000 tentatives empêche de geler le game thread sur des paramètres impossibles (`AreParamsFeasible` les écarte avant).

**Paramètres en `float`.** Le Data Asset et Blender stockent `PathLen` / `Verticality` en 32 bits : 0,45 y vaut 0,449999988… Le générateur convertit en `double` sans arrondir, exactement comme l'add-on Blender : les niveaux sont identiques (vérifié : aucune différence entre 0,45 et 0,45f sur les seeds 1 à 50).

---

## §6 Pièges, rébus, raccourcis

### 6.1 Pièges mortels

$$A = \mathrm{Combine}(H_{seed},\ \mathrm{CRC32}(\texttt{"TRAP"})),\quad B = (73\,856\,093\,x) \oplus (19\,349\,663\,y) \oplus (83\,492\,791\,z)$$

$$\mathrm{Lethal}(x, y, z) \iff \mathrm{Combine}(A, B) \bmod 10\,000 < \lfloor \rho \cdot 10\,000 \rfloor$$

Proportion de pièges ≈ $\rho$ (à $10^{-4}$ près par la troncature ; le biais du modulo, $2^{32} \bmod 10^4 = 7\,296$, est de l'ordre de $10^{-6}$). Une salle du chemin n'est jamais un piège.

> **Corrections.** Les produits se font en `uint32` : en `int32`, $63 \times 83\,492\,791$ déborde (comportement indéfini en C++). Le seuil se calcule en `double` : en `float`, `0.35f * 10000` donne 3500 au lieu de 3499 et décalait le champ mortel d'une cellule sur 10 000 par rapport à la R&D.

### 6.2 Rébus

Pour l'étape $i$ du chemin en $(x, y, z)$ et la direction sûre $d \in [0, 5]$ :

$$H_s = \mathrm{Combine}(H_{seed}, i),\quad A_1 = 1 + H_s \bmod 3,\; A_2 = 1 + (H_s \gg 3) \bmod 3,\; A_3 = 1 + (H_s \gg 6) \bmod 3$$

$$A_4 = (d - A_1 x - A_2 y - A_3 z) \bmod 6 \quad \text{(modulo positif)} \qquad \Rightarrow \qquad (A_1 x + A_2 y + A_3 z + A_4) \bmod 6 = d$$

> **Correction.** En C++, `%` garde le signe du dividende : il faut $((v \bmod 6) + 6) \bmod 6$.

### 6.3 Raccourcis sûrs

Une salle est un **raccourci** si elle est non mortelle, hors chemin, à au plus $r$ salles du chemin (`ShortcutReach`, 2 par défaut), et sur le plus court détour qui relie deux étapes $i < j$ du chemin **en gagnant des pas** :

$$\text{détour de } L \text{ salles retenu} \iff L + 1 < j - i$$

`UCubeRebusLibrary::ComputeSafeShortcuts(Seed, Path, N, Rho, Reach)`, `Reach` = `ShortcutReach` du Data Asset. Calcul linéaire : BFS multi-source borné à $r$, lancé depuis toutes les salles voisines du chemin. Quand deux fronts issus d'étapes $i$ et $j$ se touchent (ou quand un front revient sur le chemin), on teste le gain et l'on marque les salles en remontant les parents. Coût mesuré : ~2 ms en 64³ ; pour $r = 2$ et $\rho = 0{,}25$, de 77 à 953 salles selon la seed (médiane 305).

> **Correction.** L'ancienne définition (toute composante non mortelle touchant deux étapes) marquait jusqu'à 75 % de la roche et coûtait plusieurs minutes (un BFS par paire de contacts). Un détour **égal** au chemin ($L + 1 = j - i$) n'est pas un raccourci.

---

## §7 Ancrages de passerelle

Seuls le cube d'entrée et le cube de sortie portent un ancrage, au **centre de leur face extérieure**. Le socket au bout de la passerelle (`Socket_Cube`) s'y accroche.

**Normale sortante** (depuis le chemin $p_0 \dots p_k$) :

$$\hat n_{in} = -(p_1 - p_0) \qquad \hat n_{out} = p_k - p_{k-1}$$

**Position** et **repère** (repère grille) :

$$P = C(p) + \tfrac{s}{2}\,\hat n, \qquad \vec X = \hat n,\quad \vec Z = \begin{cases} (0, 0, 1) & \text{face latérale} \\ (1, 0, 0) & \text{face TOP / BOTTOM} \end{cases},\quad \vec Y = \vec Z \times \vec X$$

L'axe X pointe vers la passerelle, à l'entrée comme à la sortie. Dans UE : $P_{UE} = (P_x, -P_y, P_z)$, rotation `FRotationMatrix::MakeFromXZ`$(\vec X_{UE}, \vec Z_{UE})$.

Valeurs pour $N = 64$, $s = 100$ cm : entrée toujours en $(-3200, -50, 3250)$ cm, face West, lacet 180° ; sortie en $x = 3200$, face East. Avec `RoomSize` = 500, multiplier par 5.

**Dans Unreal** : `ACubeLevelActor` porte deux composants `Socket_Entree` et `Socket_Sortie` placés sur ces ancrages. Le serveur les calcule (`InitFromManifest`) et réplique deux `FCubePortal` : le chemin ne quitte jamais le serveur. Le GameMode spawne l'acteur, puis les passerelles `EntryBridgeClass` / `ExitBridgeClass` s'il y en a.

**Accroche** (`ACubeLevelActor::AttachBridge`) : si le socket de la passerelle vaut $T_s$ dans le repère de la passerelle et l'ancrage $T_a$ dans le monde, la passerelle doit être placée en

$$T_{passerelle} = T_s^{-1} \cdot T_a$$

(convention UE : `Monde = Local * Parent`). Le `PlayerStart` appartient à la passerelle, hors du cube. Le pawn grille actuel (`ACubeCharacter`) reste placé sur la cellule de départ par le GameMode.

---

## §8 Affichage

`UCubeViewLibrary::BuildView` choisit les salles à instancier :

- **Roche** = toutes les salles hors chemin ; en mode Coupe, seulement $z \le z_{cut}$.
- **Enveloppe visible** (`bOptimize`) : une salle de roche est gardée si un de ses 6 voisins est hors grille ou vide (chemin, ou au-dessus de la coupe). L'image est identique, avec ~14 fois moins d'instances (≈ 19 000 au lieu de 262 144).
- **Chemin** (`bShowPathCubes`) : une instance par salle, couleur = niveau de clé $\lfloor \log_2 \mathrm{Inv} \rfloor + 1$ (0 sans clé).
- Matériau par instance : `ECubeViewMaterial` (Wall, Floor, Lethal, Shortcut, Path0…3) dans `PerInstanceCustomData[0]`.

`ACubeLevelActor` (aperçu éditeur, ou en jeu avec `bShowPreviewInGame`) utilise un `UInstancedStaticMeshComponent` : un seul ajout groupé (`AddInstances`) par rafraîchissement, et le matériau par instance via `SetCustomData`. Les matériaux par salle (pièges, raccourcis) sont mis en cache par seed : changer la coupe ne relance ni le générateur ni les raccourcis. En jeu, les salles jouables restent celles de `UCubeRoomStreamer`.

---

## §9 Parité R&D (générateur pseudo-aléatoire)

`FCubeRandom` reproduit **bit à bit** `numpy.random.Generator(numpy.random.PCG64(R))`. Aucun `FRandomStream` / `FMath::Rand` : leur séquence n'a aucun rapport avec numpy.

1. **SeedSequence** (pool de 4 mots, 32 bits) : $hashmix(v) = ((v \oplus h) \cdot h') \oplus (\,\cdot \gg 16)$ avec $h' = h \cdot \texttt{0x931E8875}$, $h_0 = \texttt{0x43B0D7E5}$ ; $mix(x, y) = (\texttt{0xCA01F9DD}\,x - \texttt{0x4973F715}\,y) \oplus (\,\cdot \gg 16)$ ; sortie de 8 mots avec $h_0 = \texttt{0x8B51F9DD}$, multiplicateur $\texttt{0x58F38DED}$.
2. **PCG64** (XSL-RR 128/64) : $S \leftarrow S \cdot M + I \pmod{2^{128}}$, $M = \texttt{0x2360ED051FC65DA4\,4385DF649FCCF645}$ ; sortie $\mathrm{rotr}_{64}(S_{hi} \oplus S_{lo},\ S \gg 122)$. Initialisation : $I = (seq \ll 1) | 1$, $S = 0$, pas, $S \mathrel{+}= init$, pas.
3. **random()** $= (x \gg 11) \cdot 2^{-53}$.
4. **integers(0, n)** : méthode de Lemire sur 32 bits, avec le tampon de numpy (une sortie 64 bits sert deux fois : poids faible, puis poids fort).
5. **choice(p)** : $p = w / \sum w$, cdf = somme cumulée, normalisée par son dernier terme, index = premier $i$ avec $u < cdf_i$.

**CRC-32** : IEEE 802.3 (= `zlib.crc32`) sur les octets UTF-8, `CubeHash::Crc32` / `Crc32Str`. La table est désormais calculée à la compilation (`constexpr`) : l'ancienne initialisation paresseuse (`static bool bInit`) n'était pas thread-safe.

> **Correction.** `FCrc::StrCrc32` hache des `TCHAR` (UTF-16) : il ne donne pas `zlib.crc32` d'une chaîne ASCII. Ne pas l'utiliser pour les seeds.

> **Flottants.** Le tirage pondéré compare $u$ à des sommes de doubles : l'ordre des opérations est celui de numpy et ne doit pas être réécrit (pas de « simplification » de la normalisation). Une différence d'1 ulp ne change un choix que si $u$ tombe à $10^{-16}$ d'une borne.

---

## §10 Performances (64³)

| Étape | R&D Python | C++ |
|---|---|---|
| Génération + BFS d'une seed | 2 à 5 ms (100 à 125 ms avant optimisation) | < 1 ms (dont BFS < 0,1 ms) |
| Raccourcis ($r = 2$) | 2 à 25 ms | ~2 ms |
| Salles instanciées (mode Coupe) | ≈ 19 000 | ≈ 19 000 |

Mémoire : grille 256 Ko (un octet par salle) ; BFS ≈ $512\,k$ octets ; raccourcis 4 tableaux de $N^3$ entiers, libérés après calcul.

---

## §11 Tests

- **`Cube.Solver.Parity`** : pour les seeds `"1"` à `"50"`, compare au bit près graine RNG, premier tirage, CRC des cellules, départ/sortie, longueur, états, CRC du chemin, ancrages et raccourcis (703 vérifications).
- Les tests existants (`Cube.Rebus.*`, `Cube.Generator.OddGridSolvable`, `Cube.Clue.*`) passent inchangés.
- Lancer : *Session Frontend → Automation → filtrer « Cube »*, ou `UnrealEditor-Cmd Cube.uproject -ExecCmds="Automation RunTests Cube;Quit" -unattended -nop4 -nullrhi`.

Vérifié hors moteur avant livraison (cœur logique compilé avec GCC/Clang, sanitizers ASan/UBSan) : 90 configurations de génération (N de 8 à 64, 0 à 3 clés, réglages extrêmes) et 289 configurations d'affichage identiques à la R&D. Le build Unreal lui-même reste à lancer côté projet.

## §12 Corrections mathématiques de cette version

| § | Avant | Maintenant |
|---|---|---|
| 1 | Grille → monde UE sans inversion : niveau en miroir de Blender | Y inversé (`CellToWorld`, `WorldToCell`, `DirectionToWorld`) : même niveau dans les deux outils |
| 2 | Validité d'un voisin par « Manhattan = 1 » | Test de bord sur les coordonnées décodées (le test Manhattan est vrai par construction) |
| 4 | BFS sur `TSet`/`TMap` de `FCubeState`, file en `RemoveAt(0)` ; portes et clés ignorées | Index des salles ouvertes, tableau visité, file = parents ; portes/clés appliquées |
| 5.1 | `FRandomStream` : niveaux différents de la R&D | `FCubeRandom` (PCG64 + SeedSequence) : parité bit à bit |
| 5.2 | $\tau = \mathrm{clamp}(12^{1-\ell}, 2, 14)$ présenté comme allant jusqu'à 14 | Plage effective $[2, 12]$ ; saturation à 2 dès $\ell \ge 0{,}721$ |
| 5.3 | `FMath::RoundToInt` ; tentative gardée valide si les clés ne tiennent pas | Arrondi au pair ; tentative rejetée (sous-seed suivante), comme la R&D |
| 6.1 | Produits `int32` (débordement indéfini), seuil `float` | Produits `uint32`, seuil `double` |
| 6.2 | $A_4 = (d - base) \bmod 6$ | Modulo **positif** $((v \bmod 6) + 6) \bmod 6$ (inchangé dans le code, précisé) |
| 6.3 | Composante non mortelle touchant 2 étapes (jusqu'à 75 % de la roche, plusieurs minutes) | Détour de longueur $L$ avec $L + 1 < j - i$, à $\le r$ salles du chemin, linéaire |
| Rébus Euler | Yaw North = $\pi/2$, South = $3\pi/2$ | Avec Y inversé : North = $3\pi/2$ (−Y monde), South = $\pi/2$ |
| 9 | CRC32 à table paresseuse non thread-safe | Table `constexpr` |
