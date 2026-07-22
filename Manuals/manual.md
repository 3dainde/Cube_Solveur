# Manuel : Mise en Place et Utilisation du Système de Cubes et Loot

Ce manuel vous guidera pas à pas pour configurer le système de cubes générés de façon procédurale (HISM), avec l'intégration des raretés et des drops de butin (loot) optimisés par Object Pooling.

---

## 1. Mise en Place des Data Assets (Configuration de Base)

La force de ce système est qu'il repose entièrement sur des Data Assets. Voici comment les créer et les configurer dans l'Éditeur Unreal.

### A. Créer un Butin (Loot)
Pour chaque type d'objet qu'un joueur peut ramasser (Vie, Munitions, etc.), vous devez créer un Data Asset :
1. Dans votre **Content Browser**, faites un clic droit.
2. Allez dans **Miscellaneous > Data Asset**.
3. Choisissez la classe **DA_Loot** et nommez-le (ex: `DA_Loot_Health`).
4. Ouvrez-le et configurez ses propriétés :
   - **Loot Name** : "Soin"
   - **Loot Type** : Choisissez `Health` dans la liste déroulante.
   - **Value** : Le montant donné au joueur (ex: 50.0).
   - **Mesh** : Assignez un Static Mesh (ex: un cœur ou une trousse de soin).
   - **Size / Rotation** : Ajustez si le mesh est trop grand ou mal orienté.
   - **Pickup Sound / VFX** : Assignez un son (ex: un *bip* de soin) et un Niagara System pour l'effet visuel lors du ramassage.

> [!TIP]
> Répétez cette étape pour créer `DA_Loot_Ammo`, `DA_Loot_Armor`, `DA_Loot_XP`, etc.

### B. Créer une Table de Butin (Loot Table)
La Loot Table gère les probabilités d'apparition des butins lorsqu'un cube est détruit.
1. Clic droit > **Miscellaneous > Data Asset**.
2. Choisissez la classe **DA_LootTable** et nommez-la (ex: `DA_LootTable_Standard`).
3. Ouvrez le fichier :
   - Ajoutez des éléments dans **Loot Entries**.
   - Pour chaque entrée, sélectionnez un `DA_Loot` que vous avez créé précédemment, et définissez sa **Probability** (ex: 20%).
   - Ajustez la valeur de **No Loot Probability** (ex: 40% pour que le cube ne lâche rien presque 1 fois sur 2).

### C. Créer les Types de Cubes (Block Types)
C'est ici que vous définissez vos raretés.
1. Clic droit > **Miscellaneous > Data Asset**.
2. Choisissez la classe **DA_BlockType** et créez un fichier pour chaque rareté (ex: `DA_Cube_Standard`, `DA_Cube_Rare`, `DA_Cube_Legendary`).
3. Ouvrez un fichier et configurez-le :
   - **Block Name** : "Cube Légendaire"
   - **Mesh** : Le Static Mesh du cube de base.
   - **Material** : *Voir la section 2 pour la configuration du matériel*.
   - **Color** : Choisissez la couleur désirée (ex: Rouge pour légendaire).
   - **Health Points** : Combien de tirs pour le détruire (ex: 8).
   - **XP / Score** : Points accordés au joueur.
   - **Spawn Probability** : Ajustez cette valeur pour rendre le cube plus ou moins fréquent (ex: 1.0 pour très rare, 100.0 pour très commun).
   - **Loot Table** : Assignez la `DA_LootTable` correspondante (ex: une table plus généreuse pour le cube légendaire).

---

## 2. Configuration du Matériel (Per Instance Custom Data)

Pour que la couleur configurée dans le `DA_BlockType` s'applique aux cubes sur la grille :
1. Ouvrez le **Material** assigné à votre cube.
2. Faites un clic droit dans le graphe et ajoutez un nœud **PerInstanceCustomData**.
3. Laissez le *Data Index* à **0** (ceci correspond à la couche Rouge / R).
4. Ajoutez deux autres nœuds **PerInstanceCustomData** avec les *Data Index* **1** (Vert / G) et **2** (Bleu / B).
5. Assemblez ces 3 valeurs avec un nœud **MakeFloat3** (ou **AppendVector**).
6. Connectez le résultat dans le canal **Base Color** (ou **Emissive Color** si vous voulez qu'il brille).
7. Sauvegardez et compilez le matériel.

---

## 3. Mise en place de la Grille (BP_CUBE)

La classe C++ `ACubeGrid` s'occupe de tout générer.
1. Dans le Content Browser, créez un Blueprint héritant de la classe **CubeGrid** (ex: `BP_CubeGenerator`).
2. Placez ce Blueprint dans votre niveau.
3. Dans le panneau de détails (Details Panel) du Blueprint :
   - **Nbr H / Nbr V** : Ajustez la taille de la grille de cubes (ex: 10 par 10).
   - **Block Size** : L'espacement/taille entre les cubes (ex: 100.0).
   - **Block Types** : Ajoutez autant d'entrées que vous avez de raretés (`DA_Cube_Standard`, `DA_Cube_Rare`, etc.).

Dès que vous lancerez le jeu, la grille se générera aléatoirement en respectant les probabilités (`SpawnProbability`) de chaque type de cube !

---

## 4. Gérer les Dégâts (Tirer sur un cube)

Actuellement, l'architecture attend que vous lui disiez quel cube a été touché. 
Voici comment relier votre système de tir (Line Trace) au système de santé des cubes via un Blueprint ou en C++ :

### Exemple en Blueprint (dans le Player Character) :
1. À partir du `Hit Result` de votre Line Trace, récupérez le **Hit Component**.
2. Récupérez le **Hit Item** (qui correspond à l'Index de l'Instance du HISM).
3. Faites un **Cast to CubeGrid** sur le *Hit Actor*.
4. Si le cast réussit, appelez la fonction **Damage Cube** sur le `CubeGrid`.
5. Branchez le *Hit Component*, l'*Instance Index* (Hit Item), et la quantité de *Damage Amount* (ex: 1).

> [!WARNING]
> Assurez-vous que le composant de collision du Static Mesh de votre cube bloque bien les Line Traces dans les paramètres de collision (Block Visibility / Block Camera).

---

## 5. Fonctionnement du Butin (Ramassage)

Dès qu'un cube arrive à 0 Point de Vie, il disparaît, joue ses effets (VFX/Son), et fait apparaître un `LootActor` s'il y en a un.

- **Comportement visuel** : Le butin lévite sur place et tourne sur lui-même (géré automatiquement en C++ sans physique pour maximiser les performances).
- **Ramassage** : Il suffit au joueur de marcher dessus. Le `LootActor` détectera la collision (Overlap).
- **Mise à jour des stats** : Le `LootActor` récupérera le `PS_PlayerStateCustom` du joueur et appellera automatiquement la fonction d'ajout correspondante (`AddHealth`, `AddAmmo`, etc.).
- **Optimisation (Pool)** : L'objet disparaîtra (mais ne sera pas détruit), attendant silencieusement qu'un autre cube soit cassé pour être réutilisé.

> [!IMPORTANT]
> Pour que le ramassage fonctionne, votre personnage joueur doit bien hériter de `ACharacter` et utiliser `APS_PlayerStateCustom` comme Player State par défaut (à configurer dans votre **GameMode**).
