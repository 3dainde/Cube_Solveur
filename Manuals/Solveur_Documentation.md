# Documentation Technique : Le Solveur (State-Space BFS)

Ce document décrit les fondations mathématiques et algorithmiques du Solveur utilisé pour le générateur du Cube.

## 1. Coordonnées et Identité Mathématique

Afin de garantir que l'identité d'une salle soit déterministe, indépendante d'Unreal Engine, et réversible, nous utilisons un encodage d'un vecteur spatial $(X, Y, Z)$ vers un scalaire $ID$.

### Formule d'Encodage
Pour une grille de dimensions $N_x, N_y, N_z$, où $0 \le X < N_x$, $0 \le Y < N_y$, et $0 \le Z < N_z$ :

$$ ID(X, Y, Z) = X + (Y \times N_x) + (Z \times N_x \times N_y) $$

*Propriété :* Cette bijection garantit une empreinte mémoire optimale et un adressage direct dans des structures `TMap` ou `TArray`.

### Formule de Décodage (Opération inverse)
Le retour vers une coordonnée spatiale depuis un $ID$ s'effectue via des modulos et divisions entières :

$$ Z = \lfloor \frac{ID}{N_x \times N_y} \rfloor $$
$$ \text{Reste} = ID \pmod{N_x \times N_y} $$
$$ Y = \lfloor \frac{\text{Reste}}{N_x} \rfloor $$
$$ X = \text{Reste} \pmod{N_x} $$

## 2. Topologie et Distances

Deux salles $A$ et $B$ sont physiquement adjacentes si et seulement si leur **Distance de Manhattan** est exactement égale à 1 :

$$ D_{Manhattan}(A, B) = |A_x - B_x| + |A_y - B_y| + |A_z - B_z| = 1 $$

Toute tentative de franchissement vers une salle dont la distance de Manhattan est différente de 1 est rejetée par le solveur (prévention des exploits de téléportation ou clipping).

## 3. Modèle d'État (State-Space)

Une simple recherche de chemin spatial (Pathfinding) n'est pas suffisante car une même salle peut être traversée dans des "états" différents (ex: inventaire, orientation du joueur, flags d'énigme activés).

Nous définissons un état du Solveur comme un tuple :
$$ S = \langle RoomID, Orientation, Inventory, Flags \rangle $$

Un état est considéré comme unique par son Hash. L'exploration (BFS) se fait sur l'espace de ces **états** et non seulement sur les coordonnées spatiales. Deux passages dans la même salle avec des conditions différentes seront traités comme deux nœuds algorithmiques distincts.

## 4. Algorithme de Résolution (BFS)

Le solveur utilise un algorithme **Breadth-First Search (BFS)** pour garantir la découverte du chemin le plus court (chemin optimal).

### Pseudocode Mathématique
1. Soit $Q$ une file (Queue) d'états à visiter.
2. Soit $V$ l'ensemble (Set) des états visités (pour éviter les boucles).
3. Insérer l'état initial $S_0$ dans $Q$ et $V$.
4. Tant que $Q \neq \emptyset$ :
   1. Extraire $S_{courant}$ de $Q$.
   2. Si $S_{courant}.RoomID == ExitID$, alors la solution optimale est trouvée. Fin.
   3. Pour chaque transition possible $T_{dir}$ depuis $S_{courant}$ :
      1. Vérifier la validité topologique et les contraintes de la porte.
      2. Calculer le nouvel état $S_{suivant} = Transition(S_{courant}, T_{dir})$.
      3. Si $S_{suivant} \notin V$ :
         - Ajouter $S_{suivant}$ à $V$ et à $Q$.
         - Mémoriser le parent pour la reconstruction du chemin.

### Complexité
- **Temps** : $\mathcal{O}(|V| + |E|)$ où $V$ est le nombre d'états uniques et $E$ le nombre de transitions.
- **Espace** : $\mathcal{O}(|V|)$ pour maintenir le Set `VISITED` et la Queue `OPEN`.

---
*Généré pour le projet Cube_Solveur (R&D).*
