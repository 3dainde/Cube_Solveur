# Architecture du Système de Cubes et de Loot

Ce document décrit l'architecture proposée pour implémenter les nouvelles fonctionnalités de cubes rares, la génération aléatoire, les Data Assets, et le système de butin (loot) via un Object Pool.

## User Review Required

> [!IMPORTANT]
> - **Per Instance Custom Data vs Multi-HISM** : Le document mentionne d'utiliser un HISM et "Per Instance Custom Data" pour la couleur. Cependant, les Data Assets de cubes incluent aussi potentiellement des Mesh et Matériaux différents. L'approche standard dans Unreal est de créer **un composant HISM par type de cube (DA_BlockType)** si le mesh ou le matériel de base diffèrent, et d'utiliser le Custom Data pour les variations de couleur si le matériel le gère. La solution proposée va créer dynamiquement un HISM par `DA_BlockType`.
> - **Object Pool** : Les loots seront gérés par un `ULootPoolSubsystem` (World Subsystem). Cela permet d'éviter les `SpawnActor` à la volée pendant la destruction, en réutilisant les acteurs de loot inactifs.
> - **Lévitation du Loot** : Comme spécifié, les loots n'auront pas de physique. Ils auront un mouvement de lévitation (sinusoïdal) mis à jour dans leur `Tick` et attendront qu'un joueur passe dessus (overlap) ou qu'un timer expire.

## Open Questions

> [!WARNING]
> 1. Est-ce que le joueur interagit avec les cubes en tirant dessus (projectile) ou via un LineTrace (Hitscan) ? Cela influence la manière dont on détecte quel instance HISM a été touchée.
> 2. Est-ce que les types de loot (Santé, Munitions, Armure, etc.) ont déjà des systèmes correspondants dans le jeu, ou devrais-je simplement ajouter des events/delegates génériques pour l'instant ?
> 3. Voulez-vous que le composant de loot soit un `UPrimitiveComponent` (comme une sphère de collision) pour détecter le joueur automatiquement (Overlap) ?

## Proposed Changes

Nous allons créer plusieurs classes C++ pour soutenir cette architecture évolutive, de sorte que les développeurs/designers n'aient qu'à configurer les Data Assets dans l'éditeur.

### Enums et Structures de base
Création d'un fichier contenant les enums :
- `ELootType` (Health, Ammo, Armor, XP, Currency, Custom, None)
- Structure `FLootTableEntry` (Probabilité + référence au DA_Loot)

### Data Assets (DA)
#### [NEW] `DA_Loot` (H/CPP)
- **Hérite de** : `UPrimaryDataAsset`
- **Propriétés** : Mesh, Rotation, Size, Name, Type (`ELootType`), Value, PickupSound, PickupVFX.

#### [NEW] `DA_LootTable` (H/CPP)
- **Hérite de** : `UPrimaryDataAsset`
- **Propriétés** : Un tableau de `FLootTableEntry`.
- **Fonctionnalité** : Une fonction `UDA_Loot* RollLoot()` qui sélectionne aléatoirement un loot en fonction des probabilités pondérées.

#### [NEW] `DA_BlockType` (H/CPP)
- **Hérite de** : `UPrimaryDataAsset`
- **Propriétés** : Name, Mesh, Material, Color, Health, XP, Score, VFX, Destruction Sound, Spawn Probability, LootTable (`UDA_LootTable*`).

### Gestion du Butin (Loot Object Pool)
#### [NEW] `LootActor` (H/CPP)
- **Hérite de** : `AActor`
- **Composants** : `UStaticMeshComponent`, `USphereComponent` (pour détecter le joueur).
- **Fonctionnalité** : Fonction `InitializeLoot(UDA_Loot* Data)` pour définir son mesh/couleur. Fonction de Tick pour l'animation de lévitation. Pas de physique/gravité. Fonction pour retourner dans le pool.

#### [NEW] `LootPoolSubsystem` (H/CPP)
- **Hérite de** : `UWorldSubsystem`
- **Fonctionnalité** : Maintient un `TArray<ALootActor*>`. Gère l'acquisition (`SpawnLoot`) et la restitution (`ReturnLoot`) des acteurs pour éviter les allocations en cours de partie.

### Grille de Cubes (Architecture BP_CUBE)
#### [NEW] `CubeGrid` (H/CPP)
- **Hérite de** : `AActor`
- **Propriétés** : NbrH, NbrV, BlockSize, `TArray<UDA_BlockType*> BlockTypes`.
- **Composants** : Tableau dynamique de `UHierarchicalInstancedStaticMeshComponent` (un par DA_BlockType).
- **Fonctionnalité** : 
  - `GenerateGrid()` : Remplit les HISM en choisissant aléatoirement un type de cube selon les probabilités configurées.
  - Sauvegarde de la santé (HP) de chaque instance dans un tableau ou map.
  - `DamageCube(HISM, InstanceIndex, DamageAmount)` : Réduit la santé. Si <= 0, détruit l'instance, joue son VFX/Son, demande un loot à sa LootTable et l'affiche via le `LootPoolSubsystem`.

## Verification Plan

### Manual Verification
- Compilation avec succès du code C++.
- Création de DataAssets dans l'éditeur (1 BlockType, 1 LootTable, 2 Loots) pour s'assurer que les propriétés s'affichent correctement.
- Configuration du blueprint héritant de `ACubeGrid` pour vérifier que la génération d'instances HISM s'effectue et que les probabilités sont respectées.
- Vérification du tir/destruction sur les cubes et de l'apparition correcte du butin (en lévitation) via l'Object Pool.
