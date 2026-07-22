# Walkthrough : Système de Cubes et Loot

L'intégration du nouveau système de cubes avec raretés, butin aléatoire, et Object Pooling est terminée.

## Changements Effectués

### 1. Data Assets
Nous avons créé trois classes `UPrimaryDataAsset` qui permettent de configurer le jeu directement depuis l'éditeur (sans coder) :
- `UDA_BlockType` : Définit la rareté d'un cube (Couleur, Mesh, Material, HP, XP, Score, VFX de destruction et la table de loot associée).
- `UDA_LootTable` : Permet de configurer des probabilités d'apparition pour différents objets, avec un pourcentage configurable pour "Aucun loot".
- `UDA_Loot` : Définit les caractéristiques d'un butin (Type : Vie, Munition, etc., Valeur, Mesh, Taille, Son, VFX).

### 2. Architecture des Cubes (HISM)
- La classe `ACubeGrid` gère l'apparition des cubes sous forme de grilles.
- Elle lit un tableau de `UDA_BlockType` et génère un `UHierarchicalInstancedStaticMeshComponent` (HISM) pour chaque type de cube.
- **Custom Data** : La couleur (RGB) est injectée dans le `PerInstanceCustomData` du HISM, ce qui permet à un Material Unreal de récupérer cette couleur (via le nœud `PerInstanceCustomData` dans le Material Editor).
- **Santé Individuelle** : Chaque instance de cube a ses propres Points de Vie suivis via un `TMap`. Lors d'un tir, vous pouvez appeler `DamageCube()` pour baisser sa santé, le détruire, jouer les VFX et appeler le système de Loot.

### 3. Object Pool pour le Loot
- Le butin généré (`ALootActor`) ne possède **pas de physique ni de gravité**. Il s'anime en "flottant" continuellement grâce à son événement `Tick()`.
- Il utilise un `USphereComponent` (UPrimitiveComponent) pour la détection de chevauchement ("Overlap") avec le joueur.
- **Performance** : Les objets de loot proviennent de `ULootPoolSubsystem`. Quand un loot est ramassé, il est désactivé et retourné dans le pool au lieu d'être détruit (`Destroy`), empêchant le lag d'instanciation.

### 4. Statistiques Joueur
- Nous avons ajouté les propriétés `Health`, `Ammo`, `Armor`, `XP` et `Currency` à votre classe existante `APS_PlayerStateCustom` pour qu'elles se synchronisent en réseau (Replicated).
- Quand le joueur ramasse un butin en marchant dessus, le `LootActor` applique directement le bon montant à la bonne statistique selon le type du butin.

> [!TIP]
> **Configuration dans l'Éditeur** : Vous devez maintenant créer des Blueprint de type `Data Asset` dans l'éditeur (clic droit -> Miscellaneous -> Data Asset -> sélectionner les classes DA_Loot, DA_LootTable, etc.).

> [!NOTE]
> Pour que les couleurs Custom s'affichent, dans votre Material de base du cube, assurez-vous d'utiliser un noeud `PerInstanceCustomData` (Index 0 pour le Rouge, 1 pour le Vert, 2 pour le Bleu) et de les connecter dans le `BaseColor` (ou l'`Emissive`).
