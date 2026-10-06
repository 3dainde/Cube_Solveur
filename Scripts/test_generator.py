"""
Validation de l'invariant du generateur (Etape 2, spec Manuals/RnD/IA_Implementation_Rebus_UE.md §7.3) :
resolubilite SANS freeze pour des grilles PAIRES et IMPAIRES.

On execute le coeur pur du solveur de reference (generate_manifest + CubeSolver,
lignes 1..367, avant 'import bpy') que le C++ UCubeGenerator reproduit
structurellement (carve reseau pair, jonction de sortie N impair, boucle d'essais).

Usage :  python Scripts/test_generator.py
"""
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REF_PATH = os.path.join(HERE, "cube_solveur_blender_3d.py")


def load_pure_core():
    with open(REF_PATH, "r", encoding="utf-8") as f:
        lines = f.readlines()
    # Coupe juste avant la partie Blender ('import random' / 'import bpy').
    cut = next(i for i, ln in enumerate(lines)
               if ln.strip() in ("import random", "import bpy"))
    src = "".join(lines[:cut])
    ns = {}
    exec(compile(src, REF_PATH, "exec"), ns)
    return ns


def main():
    ns = load_pure_core()
    build_solved = ns["build_solved"]

    sizes = [11, 13, 15, 27, 33, 49, 10, 32, 50]   # impaires + paires
    seeds = [1, 7, 23, 42]
    failures = 0
    total = 0

    print(f"{'N':>4} {'seed':>5} {'valid':>6} {'solved':>7} {'|path|':>7} {'ms':>8}")
    for n in sizes:
        for seed in seeds:
            total += 1
            t0 = time.perf_counter()
            try:
                manifest, result = build_solved(seed, size=n)
            except Exception as exc:                 # freeze/crash = echec dur
                failures += 1
                print(f"{n:>4} {seed:>5}   EXCEPTION: {exc}")
                continue
            ms = (time.perf_counter() - t0) * 1000.0

            solved = result is not None and len(result.path) > 0
            valid = getattr(manifest, "valid", False)
            plen = len(result.path) if solved else 0
            print(f"{n:>4} {seed:>5} {str(valid):>6} {str(solved):>7} {plen:>7} {ms:>8.1f}")

            if not (valid and solved):
                failures += 1
            # Invariant : le chemin doit relier start->exit (ids coherents).
            if solved and result.path[0].room_id != manifest.start_id:
                failures += 1
                print(f"     [ERREUR] depart != start_id")
            if solved and result.path[-1].room_id != manifest.exit_id:
                failures += 1
                print(f"     [ERREUR] arrivee != exit_id")

    print(f"\nNiveaux generes : {total} | echecs : {failures}")
    if failures == 0:
        print("OK : toutes les grilles (paires ET impaires) sont solvables, sans freeze.")
        return 0
    print("ECHEC : au moins une grille non solvable ou incoherente.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
