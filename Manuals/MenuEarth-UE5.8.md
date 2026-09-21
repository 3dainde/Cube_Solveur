# Menu Earth UE 5.8

## Fichiers installes

- `Source/Cube/Public/MenuEarthActor.h`
- `Source/Cube/Private/MenuEarthActor.cpp`
- `Content/Art/MenuEarth/T_Earth_Day.jpg`
- `Content/Art/MenuEarth/T_Earth_Night.jpg`

Les JPG sont importes par Unreal a l'ouverture de l'editeur. Dans leurs Details, regler `Texture Group` sur `World`, `Compression Settings` sur `Default`, et laisser `sRGB` active.

## Materiau de la Terre

1. Creer `Content/Art/MenuEarth/M_MenuEarth` en `Default Lit`.
2. Ajouter deux `Texture Sample Parameter 2D`, nommes exactement `DayTexture` et `NightTexture`.
3. Relier `DayTexture RGB` a `Base Color`.
4. Relier `NightTexture RGB * (1 - Saturate(DotProduct(PixelNormalWS, LightVector))) * 2` a `Emissive Color`.
5. Brancher `M_MenuEarth` dans `Earth Material` de l'acteur `MenuEarthActor` et les deux textures dans `Day Texture` et `Night Texture`.

## Materiau de l'atmosphere

1. Creer `Content/Art/MenuEarth/M_MenuEarthAtmosphere` en `Unlit`, `Blend Mode: Additive`, `Two Sided`.
2. Utiliser `Fresnel` multiplie par une couleur bleu cyan faible, par exemple `(0.05, 0.32, 1.0)`.
3. Relier ce resultat a `Emissive Color` et `Opacity`.
4. Assigner ce materiau dans `Atmosphere Material` de l'acteur.

## Mise en scene

1. Creer une carte de menu et y placer `MenuEarthActor`.
2. Regler `Earth Scale` a `20` pour une Terre d'environ 20 m de diametre.
3. Utiliser une camera de menu a environ 35 a 50 m de la Terre, avec un FOV entre 30 et 40.
4. Conserver `Rotation Speed Degrees` entre `0.3` et `0.8` pour une rotation calme.

Les trois stations sont des meshes engine tres legers, sans simulation orbitale ni Niagara. La rotation, les materiaux et les textures sont tous accessibles dans Blueprint et dans le panneau Details.