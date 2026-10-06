"""
Harnais de parité bit-a-bit  Python (reference Blender)  <->  C++ (UCubeRebusLibrary).

Il extrait, SANS importer Blender, les fonctions pures de reference du solveur
(cube_solveur_blender_3d.py) puis les compare a un miroir scalaire exact du C++
ecrit dans Source/Cube/Private/CubeRebusLibrary.cpp.

Usage :  python Scripts/test_parity.py
Sortie :  code retour 0 si toutes les assertions passent, 1 sinon.
"""
import ast
import os
import sys
import zlib

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
REF_PATH = os.path.join(HERE, "cube_solveur_blender_3d.py")

# --- 1. Extraction AST des fonctions pures de reference (sans executer 'import bpy') ---
NEEDED = ["_hash_combine", "encode_coord", "decode_coord",
          "get_lethal_mask", "make_formula_py"]

def load_reference_functions():
    with open(REF_PATH, "r", encoding="utf-8") as f:
        src = f.read()
    tree = ast.parse(src)
    ns = {"np": np, "zlib": zlib}
    segments = []
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in NEEDED:
            segments.append(ast.get_source_segment(src, node))
    exec("\n\n".join(segments), ns)
    missing = [n for n in NEEDED if n not in ns]
    if missing:
        raise RuntimeError(f"Fonctions de reference introuvables : {missing}")
    return ns

# --- 2. Miroir scalaire EXACT du C++ (arithmetique masquee 32 bits) ---
MASK = 0xFFFFFFFF

def cpp_crc32_str(s: str) -> int:
    return zlib.crc32(s.encode("utf-8")) & MASK

def cpp_combine(a: int, b: int) -> int:
    # a ^ (b + 0x9E3779B9 + (a<<6) + (a>>2))   mod 2^32
    t = (b + 0x9E3779B9 + ((a << 6) & MASK) + (a >> 2)) & MASK
    return (a ^ t) & MASK

def cpp_is_lethal(seed, x, y, z, rho) -> bool:
    a = cpp_combine(cpp_crc32_str(str(seed)), cpp_crc32_str("TRAP"))
    b = (((x * 73856093) & MASK) ^ ((y * 19349663) & MASK) ^ ((z * 83492791) & MASK)) & MASK
    h_final = cpp_combine(a, b)
    limit = int(np.floor(rho * 10000.0))
    return (h_final % 10000) < limit

def cpp_make_formula(seed, path_index, x, y, z, safe_dir_index):
    hs = cpp_combine(cpp_crc32_str(str(seed)), path_index & MASK)
    a1 = 1 + (hs % 3)
    a2 = 1 + ((hs >> 3) % 3)
    a3 = 1 + ((hs >> 6) % 3)
    base = a1 * x + a2 * y + a3 * z
    a4 = ((safe_dir_index - base) % 6 + 6) % 6
    return (a1, a2, a3, a4)

# --- 3. Assertions ---
def main():
    ref = load_reference_functions()
    ref_lethal = ref["get_lethal_mask"]
    ref_formula = ref["make_formula_py"]

    failures = 0
    checked = 0

    # Test A : champ mortel sur grilles paires et impaires, plusieurs rho.
    for n in (10, 13, 27, 32, 49):
        rng = np.random.default_rng(12345 + n)
        for rho in (0.0, 0.15, 0.35, 0.7, 1.0):
            xs = rng.integers(0, n, size=800)
            ys = rng.integers(0, n, size=800)
            zs = rng.integers(0, n, size=800)
            ref_mask = ref_lethal(f"seed_{n}", xs, ys, zs, rho)
            for i in range(len(xs)):
                cpp = cpp_is_lethal(f"seed_{n}", int(xs[i]), int(ys[i]), int(zs[i]), rho)
                checked += 1
                if bool(ref_mask[i]) != cpp:
                    failures += 1
                    if failures <= 5:
                        print(f"  [LETHAL MISMATCH] n={n} rho={rho} "
                              f"({xs[i]},{ys[i]},{zs[i]}) ref={bool(ref_mask[i])} cpp={cpp}")

    # Test B : formules modulaires.
    rng = np.random.default_rng(999)
    for _ in range(3000):
        seed = f"S{rng.integers(1, 500)}"
        pi = int(rng.integers(0, 4000))
        x, y, z = (int(rng.integers(0, 50)) for _ in range(3))
        sdir = int(rng.integers(0, 6))
        r = ref_formula(seed, pi, x, y, z, sdir)
        c = cpp_make_formula(seed, pi, x, y, z, sdir)
        checked += 1
        if tuple(r) != tuple(c):
            failures += 1
            if failures <= 10:
                print(f"  [FORMULA MISMATCH] seed={seed} pi={pi} pos=({x},{y},{z}) "
                      f"sdir={sdir} ref={r} cpp={c}")
        # Coherence interne : la formule doit pointer sur la direction sure.
        a1, a2, a3, a4 = c
        if (a1 * x + a2 * y + a3 * z + a4) % 6 != sdir:
            failures += 1
            print(f"  [FORMULA EVAL WRONG] pos=({x},{y},{z}) -> attendu {sdir}")

    print(f"\nCellules/cas verifies : {checked}")
    if failures == 0:
        print("PARITE OK : Python <-> C++ identiques (delta = 0).")
        return 0
    print(f"ECHEC : {failures} divergences detectees.")
    return 1

if __name__ == "__main__":
    sys.exit(main())
