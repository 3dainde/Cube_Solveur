bl_info = {
    "name": "Cube Solveur 3D (State-Space BFS)",
    "author": "Cube_Solveur R&D",
    "version": (2, 3, 0),
    "blender": (4, 0, 2),
    "location": "Vue 3D > Barre latérale (N) > Cube Solveur",
    "description": "Grille 64x64x64, solveur BFS, spline du chemin optimal, ancrages d'entrée/sortie et distance à parcourir selon la seed",
    "category": "Object",
}

# =============================================================================
#  COEUR LOGIQUE 3D — indépendant de Blender (testable hors moteur, comme le
#  README du projet le demande). Suit Manuals/Solveur_Documentation.md.
# =============================================================================
import time
import zlib
from collections import deque, namedtuple

import numpy as np

GRID_SIZE = 64          # 64 x 64 x 64 salles
SEED_MIN, SEED_MAX = 1, 50
MAX_ATTEMPTS = 64       # sous-seeds déterministes si une seed est insoluble

# Types de cellules
FLOOR, WALL, KEY, GATE = 0, 1, 2, 3

# Même ordre que ECubeDirection (CubeCore.h)
DIRECTIONS = (
    ("EAST",   (1, 0, 0)),
    ("WEST",   (-1, 0, 0)),
    ("NORTH",  (0, 1, 0)),
    ("SOUTH",  (0, -1, 0)),
    ("TOP",    (0, 0, 1)),
    ("BOTTOM", (0, 0, -1)),
)


# --- §1 Coordonnées et identité mathématique ---------------------------------
# (fonctionnent aussi bien sur des int Python que sur des tableaux numpy)
def encode_coord(x, y, z, nx, ny):
    """ID(X, Y, Z) = X + Y*Nx + Z*Nx*Ny"""
    return x + y * nx + z * nx * ny


def decode_coord(room_id, nx, ny):
    """Z = ID // (Nx*Ny) ; Reste = ID % (Nx*Ny) ; Y = Reste // Nx ; X = Reste % Nx"""
    z = room_id // (nx * ny)
    reste = room_id % (nx * ny)
    y = reste // nx
    x = reste % nx
    return x, y, z


# --- §2 Topologie --------------------------------------------------------------
def manhattan(a, b):
    return abs(a[0] - b[0]) + abs(a[1] - b[1]) + abs(a[2] - b[2])


# --- §3 Modèle d'état S = <RoomID, Orientation, Inventory, Flags> -------------
State = namedtuple("State", "room_id orientation inventory flags")

# Hash parfait d'un état : on empaquette le tuple dans un seul entier
# (RoomID sur 18 bits = 64^3, puis Inventory, Flags, Orientation sur 3 bits).
_ROOM_BITS, _INV_SHIFT, _FLAG_SHIFT, _ORI_SHIFT = 18, 18, 21, 24
_ROOM_MASK = (1 << _ROOM_BITS) - 1


def pack_state(s):
    return s.room_id | (s.inventory << _INV_SHIFT) | (s.flags << _FLAG_SHIFT) | (s.orientation << _ORI_SHIFT)


def unpack_state(p):
    return State(p & _ROOM_MASK, (p >> _ORI_SHIFT) & 7, (p >> _INV_SHIFT) & 7, (p >> _FLAG_SHIFT) & 7)


class Manifest:
    """Équivalent Python de FCubeManifest."""

    def __init__(self, seed, nx, ny, nz):
        self.seed = seed
        self.nx, self.ny, self.nz = nx, ny, nz
        self.cells = np.zeros(nx * ny * nz, dtype=np.uint8)  # indexé par RoomID
        self.key_bit = {}    # room_id -> bit d'inventaire donné par la clé
        self.gate_bit = {}   # room_id -> bit d'inventaire exigé par la porte
        self.start_id = -1
        self.exit_id = -1
        self.attempt = 0
        self.valid = True

    def grid(self):
        """Vue 3D [z, y, x] : l'ordre C de numpy correspond exactement à la formule d'ID."""
        return self.cells.reshape(self.nz, self.ny, self.nx)

    def in_bounds(self, x, y, z):
        return 0 <= x < self.nx and 0 <= y < self.ny and 0 <= z < self.nz


# --- Générateur déterministe ---------------------------------------------------
def _hash_combine(a, b):
    return (a ^ (b + 0x9E3779B9 + ((a << 6) & 0xFFFFFFFF) + (a >> 2))) & 0xFFFFFFFF


def _weighted_index(rng, wts):
    """Strictement équivalent à rng.choice(len(wts), p=wts/sum(wts)) — même tirage
    rng.random(), même arithmétique (somme, cumsum, normalisation, searchsorted
    'right') — mais ~9x plus rapide pour 1 à 6 poids. Vérifié sur 200 000 tirages.
    C++ : boucle identique sur un tableau fixe de 6 doubles."""
    s = 0.0
    for w in wts:
        s += w
    acc = 0.0
    cdf = []
    for w in wts:
        acc += w / s
        cdf.append(acc)
    last = cdf[-1]
    u = rng.random()
    for i, c in enumerate(cdf):
        if u < c / last:
            return i
    return len(cdf) - 1


def _carve_path(n, rng, verticality, toward):
    """Chemin AUTO-ÉVITANT unique, creusé sur le réseau des noeuds pairs.

    On avance de 2 en 2 par un DFS avec retour arrière, en biaisant vers la
    sortie (poids 'toward' sur les pas qui s'en rapprochent) et faiblement vers
    le haut (poids 'verticality' sur les pas en Z). Comme on n'ouvre QUE les
    murs réellement franchis, deux cases ouvertes ne sont voisines que si elles
    se suivent dans le chemin : aucun raccourci, donc le solveur ne peut que
    suivre le chemin sans jamais revenir sur ses pas. Le chemin reste à 1 case
    des bords (marge), avec une seule entrée en X=0 et une seule sortie en X=n-1.

    Renvoie (cases_ordonnées, case_entrée, case_sortie, noeuds_intérieurs) ou
    (None, None, None, None) si la cible n'a pas été atteinte (réseau non connexe).
    """
    lo = 2
    hi = (n - 2) & ~1                                   # bornes paires intérieures (force pair)
    ev = list(range(lo, hi + 1, 2))
    sc = (n // 2) & ~1
    A = (2, sc, sc)                                     # 1er noeud, derrière l'entrée
    ey, ez = int(rng.choice(ev)), int(rng.choice(ev))
    T = (hi, ey, ez)                                    # dernier noeud, devant la sortie

    base = [(2, 0, 0, 1.0), (-2, 0, 0, 1.0), (0, 2, 0, 1.0), (0, -2, 0, 1.0),
            (0, 0, 2, verticality), (0, 0, -2, verticality)]

    def dist(p):
        return abs(p[0] - T[0]) + abs(p[1] - T[1]) + abs(p[2] - T[2])

    visited = {A}
    stack = [A]
    found = (A == T)
    while stack and not found:
        x, y, z = stack[-1]
        d0 = dist((x, y, z))
        cand, wts = [], []
        for dx, dy, dz, w in base:
            X, Y, Z = x + dx, y + dy, z + dz
            if lo <= X <= hi and lo <= Y <= hi and lo <= Z <= hi and (X, Y, Z) not in visited:
                cand.append((X, Y, Z))
                wts.append(w * (toward if dist((X, Y, Z)) < d0 else 1.0))
        if cand:
            nxt = cand[_weighted_index(rng, wts)]
            visited.add(nxt); stack.append(nxt)
            if nxt == T:
                found = True
        else:
            stack.pop()
    if not found:
        return None, None, None, None

    interior = stack                                    # pile DFS = chemin simple A..T
    full = [(0, sc, sc), (1, sc, sc)]                   # entrée sur la face + connecteur
    for i, node in enumerate(interior):
        full.append(node)
        if i + 1 < len(interior):
            q = interior[i + 1]
            full.append(((node[0] + q[0]) // 2, (node[1] + q[1]) // 2, (node[2] + q[2]) // 2))
            
    # Connecter la sortie même si n est impair
    cx, cy, cz = interior[-1]
    while cx < n - 2:
        cx += 1
        full.append((cx, cy, cz))
    full.append((n - 1, ey, ez))                        # sortie sur la face opposée
    return full, (0, sc, sc), (n - 1, ey, ez), interior


def generate_manifest(seed, attempt=0, size=GRID_SIZE, path_len=0.45, verticality=0.18, n_keys=2):
    """Bloc plein creusé d'UN chemin auto-évitant (étages en X/Y, puits en Z),
    jalonné de clés et de portes verrouillées.

    Même logique de sous-seed que CubeGenerator.cpp : CRC32(seed) combiné avec
    CRC32("PATH"), puis avec l'indice de tentative.
    """
    seed_hash = zlib.crc32(str(seed).encode())
    path_hash = _hash_combine(seed_hash, zlib.crc32(b"PATH"))
    rng = np.random.Generator(np.random.PCG64(_hash_combine(path_hash, attempt)))

    n = size
    m = Manifest(seed, n, n, n)
    m.attempt = attempt

    toward = min(14.0, max(2.0, 12.0 ** (1.0 - path_len)))   # court/direct <-> long/sinueux
    full, start_cell, exit_cell, interior = _carve_path(n, rng, verticality, toward)
    if full is None:
        m.valid = False
        return m

    open_ = np.zeros((n, n, n), dtype=bool)              # [z, y, x]
    for cx, cy, cz in full:
        open_[cz, cy, cx] = True

    # Clés et portes le long du chemin : clé j strictement AVANT porte j, donc
    # le solveur doit détenir la bonne clé en atteignant chaque porte.
    gates, keys, ok = [], [], True
    if n_keys > 0:
        if len(interior) < 4 * n_keys + 2:
            ok = False
        else:
            used = set()
            for j in range(n_keys):
                ki = int(round((j + 1) / (n_keys + 1) * (len(interior) - 1)))
                ki = min(max(ki, 1), len(interior) - 4)
                if ki in used or (ki + 2) in used:
                    ok = False; break
                used.update((ki, ki + 2))
                keys.append((interior[ki], j))
                gates.append((interior[ki + 2], j))

    g = m.grid()
    g[~open_] = WALL
    if ok:
        for (kx, ky, kz), j in keys:
            rid = encode_coord(kx, ky, kz, n, n); m.cells[rid] = KEY;  m.key_bit[rid] = 1 << j
        for (gx, gy, gz), j in gates:
            rid = encode_coord(gx, gy, gz, n, n); m.cells[rid] = GATE; m.gate_bit[rid] = 1 << j

    m.start_id = encode_coord(*start_cell, n, n)
    m.exit_id = encode_coord(*exit_cell, n, n)
    m.valid = ok
    return m


# --- §4 Solveur BFS sur l'espace d'états -------------------------------------
SolveResult = namedtuple("SolveResult", "path n_states n_rooms time_ms")


class CubeSolver:
    """Équivalent Python de UCubeSolver, étendu à l'inventaire et aux flags."""

    def __init__(self, manifest):
        self.m = manifest
        self._table = self._build_topology()

    # -- Topologie statique, limitée aux salles OUVERTES ----------------------
    # Le BFS ne visite jamais un mur : indexer les 262 144 salles coûtait ~100 ms
    # pour ~600 salles utiles. On ne construit la table que pour les salles
    # ouvertes (quelques centaines), dans l'ordre de DIRECTIONS (= ECubeDirection).
    # C++ : même principe, ou voisins calculés à la volée (ID ± 1, ± Nx, ± Nx*Ny
    # avec test de bord), sans aucune table.
    def _build_topology(self):
        m = self.m
        nx, ny, nz = m.nx, m.ny, m.nz
        cells = m.cells
        table = {}
        for rid in np.flatnonzero(cells != WALL).tolist():
            x, y, z = decode_coord(rid, nx, ny)
            out = []
            for _, (dx, dy, dz) in DIRECTIONS:
                X, Y, Z = x + dx, y + dy, z + dz            # §2 : Manhattan == 1 par construction
                if 0 <= X < nx and 0 <= Y < ny and 0 <= Z < nz:
                    nid = encode_coord(X, Y, Z, nx, ny)
                    if cells[nid] != WALL:
                        out.append(nid)
            table[rid] = out
        return table

    def get_adjacent_room_id(self, room_id, delta):
        m = self.m
        x, y, z = decode_coord(room_id, m.nx, m.ny)
        x, y, z = x + delta[0], y + delta[1], z + delta[2]
        if not m.in_bounds(x, y, z):
            return -1
        return encode_coord(x, y, z, m.nx, m.ny)

    def get_valid_transitions(self, state):
        """Version lisible (référence) — même règles que la boucle rapide de solve()."""
        m = self.m
        here = decode_coord(state.room_id, m.nx, m.ny)
        out = []
        for name, delta in DIRECTIONS:
            nid = self.get_adjacent_room_id(state.room_id, delta)
            if nid == -1 or manhattan(here, decode_coord(nid, m.nx, m.ny)) != 1:
                continue
            cell = m.cells[nid]
            if cell == WALL:
                continue
            inv, flags = state.inventory, state.flags
            if cell == GATE:
                bit = m.gate_bit[nid]
                if not (inv & bit):
                    continue
                flags |= bit
            elif cell == KEY:
                inv |= m.key_bit[nid]
            out.append((name, State(nid, state.orientation, inv, flags)))
        return out

    def solve(self):
        t0 = time.perf_counter()
        m = self.m
        if m.start_id == -1 or m.exit_id == -1:
            return SolveResult(None, 0, 0, 0.0)

        table = self._table
        cells = m.cells.tobytes()
        gate_bit, key_bit = m.gate_bit, m.key_bit
        exit_id = m.exit_id

        s0 = pack_state(State(m.start_id, 0, 0, 0))
        queue = deque([s0])      # Q (OPEN)
        parent = {s0: -1}        # clés = V (VISITED), valeurs = parent
        final = -1

        while queue:
            s = queue.popleft()
            room = s & _ROOM_MASK
            if room == exit_id:
                final = s        # solution optimale trouvée
                break
            inv = (s >> _INV_SHIFT) & 7
            flags = (s >> _FLAG_SHIFT) & 7
            high = s >> _ORI_SHIFT << _ORI_SHIFT      # orientation inchangée
            for nid in table[room]:
                c = cells[nid]
                ni, nf = inv, flags
                if c == GATE:
                    bit = gate_bit[nid]
                    if not (inv & bit):
                        continue
                    nf |= bit
                elif c == KEY:
                    ni |= key_bit[nid]
                ns = nid | (ni << _INV_SHIFT) | (nf << _FLAG_SHIFT) | high
                if ns not in parent:
                    parent[ns] = s
                    queue.append(ns)

        path = None
        if final != -1:
            path = []
            s = final
            while s != -1:
                path.append(unpack_state(s))
                s = parent[s]
            path.reverse()

        rooms = np.unique(np.fromiter(parent.keys(), dtype=np.int64, count=len(parent)) & _ROOM_MASK)
        return SolveResult(path, len(parent), int(rooms.size), (time.perf_counter() - t0) * 1000.0)


def build_solved(seed, size=GRID_SIZE, path_len=0.45, verticality=0.18, n_keys=2):
    """Génère puis valide ; en cas d'échec (chemin trop court pour les clés, ou
    cible non atteinte), retente indéfiniment avec un sous-seed déterministe
    pour garantir l'absence de seed incorrecte.
    
    // NOTE POUR UE5/C++ : Ce même principe de boucle `while (true)` avec incrémentation 
    // de `attempt` (sous-seed) devra être utilisé dans UCubeGenerator::GenerateCube 
    // pour s'assurer que le joueur n'a jamais un niveau injouable.
    """
    manifest = None
    result = SolveResult(None, 0, 0, 0.0)
    attempt = 0
    while True:
        manifest = generate_manifest(seed, attempt, size, path_len, verticality, n_keys)
        if manifest.valid:
            result = CubeSolver(manifest).solve()
            if result.path:
                break
        attempt += 1
    return manifest, result


# --- §5 Ancrages d'entrée / sortie (passerelles) -------------------------------
# Un ancrage = centre de la face EXTÉRIEURE du cube d'entrée (ou de sortie).
# Le socket au bout de la passerelle vient s'y accrocher. Rien n'est stocké :
# tout se déduit en O(1) des deux premiers / deux derniers états du chemin.
#   P      = centre_cube + (s/2) * n̂
#   X local = n̂ (normale sortante, vers la passerelle)
#   Z local = Z monde (faces latérales) ou X monde (faces TOP / BOTTOM)
#   Y local = Z × X (repère direct)
Portal = namedtuple("Portal", "face room_id cell normal up position")


def _step(a_id, b_id, nx, ny):
    ax, ay, az = decode_coord(a_id, nx, ny)
    bx, by, bz = decode_coord(b_id, nx, ny)
    return (bx - ax, by - ay, bz - az)


def _face_index(normal):
    """Index dans DIRECTIONS (= ECubeDirection)."""
    return next(i for i, (_, d) in enumerate(DIRECTIONS) if d == normal)


def compute_portal(manifest, path, entry, cell=1.0):
    """Ancrage au centre de la face extérieure du cube d'entrée (entry=True) ou de sortie.

    `path` : liste d'États (result.path) ou d'IDs de salles. `cell` : taille de salle.
    """
    if not path or len(path) < 2:
        return None
    nx, ny = manifest.nx, manifest.ny
    ids = [s.room_id if hasattr(s, "room_id") else int(s) for s in path]
    if entry:
        rid = ids[0]
        normal = tuple(-c for c in _step(ids[0], ids[1], nx, ny))
    else:
        rid = ids[-1]
        normal = _step(ids[-2], ids[-1], nx, ny)
    x, y, z = decode_coord(rid, nx, ny)
    ox, oy = (nx - 1) * cell / 2.0, (ny - 1) * cell / 2.0
    h = 0.5 * cell
    pos = (x * cell - ox + h * normal[0],
           y * cell - oy + h * normal[1],
           z * cell + h + h * normal[2])
    up = (1, 0, 0) if normal[2] else (0, 0, 1)        # TOP / BOTTOM : Z local = X monde
    return Portal(_face_index(normal), rid, (x, y, z), normal, up, pos)


def to_unreal(portal, cell_cm=100.0, cell=1.0):
    """Blender (main droite, unités Blender) -> UE (main gauche, cm) : Y inversé.

    Renvoie (position_cm, normale, up) à passer à FRotationMatrix::MakeFromXZ.
    """
    k = cell_cm / cell
    px, py, pz = portal.position
    nx_, ny_, nz_ = portal.normal
    ux, uy, uz = portal.up
    return (px * k, -py * k, pz * k), (nx_, -ny_, nz_), (ux, -uy, uz)


# --- §6 Distance à parcourir de l'entrée à la sortie ---------------------------
# Le chemin ne va que d'un cube à un cube voisin (pas d'axe) : chaque pas mesure
# exactement une taille de cube s. On mesure d'ancrage à ancrage (là où les
# passerelles s'accrochent), donc on ajoute un demi-cube à chaque bout :
#   D = (pas + 1) * s        pas = nombre de pas du chemin optimal
# Taille par défaut : s = 1 (m).
PathDistance = namedtuple("PathDistance", "steps cell total horizontal vertical direct")


def compute_path_distance(manifest, path, cell=1.0):
    """Distance à parcourir de l'ancrage d'entrée à l'ancrage de sortie.

    `path` : liste d'États (result.path) ou d'IDs. `cell` : taille d'un cube (1 m si non précisée).
    Renvoie total = horizontal + vertical (distance réellement parcourue dans les couloirs
    et les puits) et direct = distance en ligne droite entre les deux ancrages.
    """
    if not path or len(path) < 2:
        return None
    cell = float(cell) if cell else 1.0
    nx, ny = manifest.nx, manifest.ny
    ids = [s.room_id if hasattr(s, "room_id") else int(s) for s in path]
    steps = len(ids) - 1
    v_steps = sum(1 for a, b in zip(ids, ids[1:]) if _step(a, b, nx, ny)[2] != 0)

    entry = compute_portal(manifest, ids, True, cell)
    exit_ = compute_portal(manifest, ids, False, cell)
    # Demi-cubes d'entrée et de sortie : verticaux si l'ancrage est sur une face TOP / BOTTOM
    v_halves = (entry.normal[2] != 0) + (exit_.normal[2] != 0)
    vertical = (v_steps + 0.5 * v_halves) * cell
    total = (steps + 1) * cell
    direct = sum((a - b) ** 2 for a, b in zip(entry.position, exit_.position)) ** 0.5
    return PathDistance(steps, cell, total, total - vertical, vertical, direct)


# =============================================================================
#  COUCHE PRÉSENTATION — Blender 4.0.2
# =============================================================================
# === BLENDER ===
import random

import bpy
from bpy.props import BoolProperty, EnumProperty, FloatProperty, IntProperty, PointerProperty
from mathutils import Matrix, Vector

COLL_NAME = "Cube_Solveur"
GRID_OBJ = "CS_Grille"
PATH_OBJ = "CS_Chemin"
BOX_OBJ = "CS_Cadre"
ENTRY_OBJ = "CS_Entree"
EXIT_OBJ = "CS_Sortie"

# Couleur de la spline / des cubes de chemin selon la dernière clé possédée.
# Niveau 0 (avant toute clé) = vert ; puis une couleur par clé.
SPLINE_PALETTE = [
    ("Spline0", (0.00, 0.85, 0.85, 1.0), 3.0),  # Turquoise (Chemin du solveur - SafePath)
    ("Spline1", (1.00, 0.65, 0.00, 1.0), 3.0),
    ("Spline2", (0.90, 0.10, 0.85, 1.0), 3.0),
    ("Spline3", (0.00, 0.80, 1.00, 1.0), 3.0),
]

PALETTE = [
    ("Mur", (0.05, 0.05, 0.06, 1.0), 0.0),
    ("Sol", (0.16, 0.16, 0.19, 1.0), 0.0),
    ("Mortel", (0.85, 0.10, 0.10, 1.0), 0.4),      # Rouge vif (Mortel)
    ("Raccourci", (0.10, 0.85, 0.10, 1.0), 0.6),   # Vert (SafeShortcut reliant le chemin)
] + [("Voie%d" % i, rgba, 1.5) for i, (_, rgba, _) in enumerate(SPLINE_PALETTE)]
MAT_INDEX = {name: i for i, (name, _, _) in enumerate(PALETTE)}


# Cube unitaire centré, normales sortantes
_BASE_V = np.array([(-.5, -.5, -.5), (.5, -.5, -.5), (.5, .5, -.5), (-.5, .5, -.5),
                    (-.5, -.5, .5), (.5, -.5, .5), (.5, .5, .5), (-.5, .5, .5)], dtype=np.float32)
_BASE_F = np.array([(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4),
                    (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)], dtype=np.int32)

# Cache : changer l'affichage ne relance pas le solveur
_CACHE = {"key": None, "manifest": None, "result": None, "sc_key": None, "shortcuts": set(), "static": None}


def _cached_shortcuts(manifest, result, rho, reach):
    """Raccourcis recalculés seulement si la seed/génération ou rho change,
    pas à chaque réglage d'affichage (coupe Z, cubes du chemin, etc.)."""
    key = (_CACHE["key"], round(rho, 4), reach)
    if _CACHE["sc_key"] != key:
        _CACHE["shortcuts"] = compute_shortcuts(manifest, result, rho, reach)
        _CACHE["sc_key"] = key
    return _CACHE["shortcuts"]


# --- Outils Blender ---------------------------------------------------------------
def _make_material(name, rgba, emission):
    mat = bpy.data.materials.get(name)
    if mat is None:
        mat = bpy.data.materials.new(name)
        mat.use_nodes = True
    mat.diffuse_color = rgba                     # couleur en mode Solid
    bsdf = mat.node_tree.nodes.get("Principled BSDF") if mat.node_tree else None
    if bsdf:
        bsdf.inputs["Base Color"].default_value = rgba
        bsdf.inputs["Roughness"].default_value = 0.6
        for sock in ("Emission Color", "Emission"):   # 4.0 : "Emission Color"
            if sock in bsdf.inputs:
                bsdf.inputs[sock].default_value = rgba
                break
        if "Emission Strength" in bsdf.inputs:
            bsdf.inputs["Emission Strength"].default_value = emission
    return mat


def _get_collection(scene):
    coll = bpy.data.collections.get(COLL_NAME)
    if coll is None:
        coll = bpy.data.collections.new(COLL_NAME)
    if COLL_NAME not in scene.collection.children:
        scene.collection.children.link(coll)
    return coll


def _get_object(name, data_factory, coll):
    obj = bpy.data.objects.get(name)
    if obj is None:
        obj = bpy.data.objects.new(name, data_factory())
        coll.objects.link(obj)
    return obj


def _world_pos(x, y, z, n, pitch):
    """Grille centrée en X/Y, posée sur le sol en Z (fonctionne sur tableaux numpy)."""
    o = (n - 1) * pitch / 2.0
    return x * pitch - o, y * pitch - o, z * pitch + 0.5 * pitch


def _cube_edges():
    """12 arêtes du cube unitaire + arête de chaque coin (corner_edge) pour les 6 faces."""
    edges, index = [], {}
    corner_edge = []
    for face in _BASE_F.tolist():
        for i in range(4):
            a, b = face[i], face[(i + 1) % 4]          # coin i -> coin suivant
            key = (min(a, b), max(a, b))
            if key not in index:
                index[key] = len(edges)
                edges.append(key)
            corner_edge.append(index[key])
    return np.array(edges, np.int32), np.array(corner_edge, np.int32)


_BASE_E, _BASE_CE = _cube_edges()                    # (12, 2), (24,)
_HAS_LOOP_TOTAL = bpy.app.version < (4, 0, 0)        # lecture seule depuis 4.0


def _set_materials(mesh):
    """Matériaux affectés une seule fois (pas de clear/append à chaque refresh)."""
    names = ["CS_" + name for name, _, _ in PALETTE]
    if [m.name if m else None for m in mesh.materials] != names:
        mesh.materials.clear()
        for name, rgba, emi in PALETTE:
            mesh.materials.append(_make_material("CS_" + name, rgba, emi))


def _write_cubes(mesh, centers, size, mats):
    """Écrit k cubes dans un mesh.

    Chemin rapide (Blender 3.6+ / 4.x) : écriture directe des attributs génériques
    (position, .edge_verts, .corner_vert, .corner_edge, material_index), arêtes
    fournies -> pas de calc_edges. ~10x plus rapide que vertices.co /
    loops.vertex_index / polygons.material_index, qui passent par RNA élément
    par élément. Repli automatique sur l'ancienne API si un attribut manque.
    """
    mesh.clear_geometry()
    _set_materials(mesh)
    k = len(centers)
    if k == 0:
        mesh.update()
        return
    off8 = (np.arange(k, dtype=np.int32) * 8)
    verts = (_BASE_V[None, :, :] * np.float32(size) + centers[:, None, :]).astype(np.float32, copy=False)
    corner_vert = (_BASE_F.reshape(1, 24) + off8[:, None]).astype(np.int32, copy=False)
    edge_verts = (_BASE_E.reshape(1, 24) + off8[:, None]).astype(np.int32, copy=False)
    corner_edge = (_BASE_CE[None, :] + (np.arange(k, dtype=np.int32) * 12)[:, None]).astype(np.int32, copy=False)

    mesh.vertices.add(k * 8)
    mesh.edges.add(k * 12)
    mesh.loops.add(k * 24)
    mesh.polygons.add(k * 6)
    mesh.polygons.foreach_set("loop_start", np.arange(0, k * 24, 4, dtype=np.int32))
    if _HAS_LOOP_TOTAL:
        mesh.polygons.foreach_set("loop_total", np.full(k * 6, 4, dtype=np.int32))

    A = mesh.attributes
    pos, ev, cv, ce = (A.get(n) for n in ("position", ".edge_verts", ".corner_vert", ".corner_edge"))
    fast = all(a is not None for a in (pos, ev, cv, ce))
    if fast:
        pos.data.foreach_set("vector", verts.ravel())
        ev.data.foreach_set("value", edge_verts.ravel())
        cv.data.foreach_set("value", corner_vert.ravel())
        ce.data.foreach_set("value", corner_edge.ravel())
    else:                                            # repli : API historique
        mesh.vertices.foreach_set("co", verts.ravel())
        mesh.edges.foreach_set("vertices", edge_verts.ravel())
        mesh.loops.foreach_set("vertex_index", corner_vert.ravel())
        mesh.loops.foreach_set("edge_index", corner_edge.ravel())

    face_mats = np.repeat(mats.astype(np.int32, copy=False), 6)
    mi = A.get("material_index") or A.new("material_index", 'INT', 'FACE')
    mi.data.foreach_set("value", face_mats)
    mesh.update()


def _write_points(mesh, centers):
    """Nuage de points (un sommet par salle) pour l'instanciation « Salle Custom »."""
    mesh.clear_geometry()
    mesh.vertices.add(len(centers))
    pos = mesh.attributes.get("position")
    if pos is not None:
        pos.data.foreach_set("vector", centers.ravel())
    else:
        mesh.vertices.foreach_set("co", centers.ravel())
    mesh.update()


def _visible_shell(solid, n):
    """Ne garde que les cubes qui touchent du vide (ou le bord de la grille).

    Un cube entouré de 6 voisins pleins n'est jamais visible : le retirer donne
    exactement la même image avec bien moins de géométrie. N'enlève donc jamais
    un cube qui borde le chemin. Entrée/sortie : masque 1D indexé par RoomID.
    """
    s = solid.reshape(n, n, n)                          # [z, y, x]
    e = ~s
    exposed = np.zeros_like(s)
    # Bords de la grille : hors grille = vide
    exposed[0], exposed[-1] = True, True
    exposed[:, 0], exposed[:, -1] = True, True
    exposed[:, :, 0], exposed[:, :, -1] = True, True
    # 6 voisins par tranches (sans copie de np.roll)
    exposed[1:] |= e[:-1];       exposed[:-1] |= e[1:]
    exposed[:, 1:] |= e[:, :-1]; exposed[:, :-1] |= e[:, 1:]
    exposed[:, :, 1:] |= e[:, :, :-1]; exposed[:, :, :-1] |= e[:, :, 1:]
    return (s & exposed).ravel()


def _path_rooms(result):
    """Salles du chemin -> niveau de couleur (bit_length de l'inventaire).

    Une salle peut être visitée deux fois (aller/retour avec un inventaire
    différent) : on retient le niveau le plus élevé, comme le fait la spline.
    """
    if not result.path:
        return np.empty(0, np.int64), np.empty(0, np.int64)
    lvl_by_id = {}
    for s in result.path:
        lv = s.inventory.bit_length()
        if s.room_id not in lvl_by_id or lv > lvl_by_id[s.room_id]:
            lvl_by_id[s.room_id] = lv
    ids = np.fromiter(lvl_by_id.keys(), dtype=np.int64, count=len(lvl_by_id))
    lvl = np.fromiter(lvl_by_id.values(), dtype=np.int64, count=len(lvl_by_id))
    return ids, lvl


def get_lethal_mask(seed, x, y, z, rho):
    """Vectorized version of UCubeRebusLibrary::IsLethal"""
    H = zlib.crc32(str(seed).encode('utf-8')) & 0xFFFFFFFF
    H_TRAP = zlib.crc32(b"TRAP") & 0xFFFFFFFF
    A = np.uint32(_hash_combine(H, H_TRAP))
    
    x_part = (x.astype(np.uint32) * np.uint32(73856093))
    y_part = (y.astype(np.uint32) * np.uint32(19349663))
    z_part = (z.astype(np.uint32) * np.uint32(83492791))
    B = x_part ^ y_part ^ z_part
    
    term = B + np.uint32(0x9E3779B9) + (A << 6) + (A >> 2)
    H_final = A ^ term
    
    limit = int(rho * 10000.0)
    return (H_final % np.uint32(10000)) < np.uint32(limit)


def make_formula_py(seed, path_index, x, y, z, safe_dir_index):
    """Reference implementation of UCubeRebusLibrary::MakeFormula"""
    H = zlib.crc32(str(seed).encode('utf-8')) & 0xFFFFFFFF
    Hs = _hash_combine(H, path_index)
    A1 = 1 + (Hs % 3)
    A2 = 1 + ((Hs >> 3) % 3)
    A3 = 1 + ((Hs >> 6) % 3)
    base = A1*x + A2*y + A3*z
    A4 = ((safe_dir_index - base) % 6 + 6) % 6
    return (A1, A2, A3, A4)


def _neighbor_table(n):
    """Voisins 6-connexes de chaque salle (n³ x 6), -1 hors grille. Vectorisé."""
    ids = np.arange(n * n * n, dtype=np.int64)
    x, y, z = decode_coord(ids, n, n)
    table = np.full((ids.size, 6), -1, dtype=np.int64)
    for d, (_, (dx, dy, dz)) in enumerate(DIRECTIONS):
        X, Y, Z = x + dx, y + dy, z + dz
        ok = (X >= 0) & (X < n) & (Y >= 0) & (Y < n) & (Z >= 0) & (Z < n)
        table[ok, d] = encode_coord(X, Y, Z, n, n)[ok]
    return table


def compute_shortcuts(manifest, result, rho, reach=2):
    """Cellules SafeShortcut : petits détours NON mortels, collés au chemin, qui
    relient deux étapes du chemin en FAISANT GAGNER des pas.

    - Proximité : une cellule n'est candidate que si elle est à <= `reach`
      salles du chemin (BFS multi-source borné, lancé depuis toutes les étapes).
    - Gain : un détour de L salles relie l'étape i à l'étape j en L + 1 pas ;
      il n'est retenu que si L + 1 < |j - i| (vrai raccourci, pas un parallèle).
    - Seules les cellules du plus court détour sont colorées (couloir de 1 case).

    Coût linéaire : BFS borné + remontée des parents. Quelques dizaines de ms.
    """
    if not result.path or rho <= 0.0 or reach <= 0:
        return set()

    n = manifest.nx
    n3 = n * n * n
    path_ids = np.fromiter((s.room_id for s in result.path), dtype=np.int64, count=len(result.path))
    uniq, first = np.unique(path_ids, return_index=True)
    fi = [-1] * n3
    for rid, f in zip(uniq.tolist(), first.tolist()):
        fi[rid] = f                                     # 1re étape où la salle est atteinte

    def nbs(c):
        x, r = c % n, c // n
        y, z = r % n, r // n
        if x > 0: yield c - 1
        if x < n - 1: yield c + 1
        if y > 0: yield c - n
        if y < n - 1: yield c + n
        if z > 0: yield c - n * n
        if z < n - 1: yield c + n * n

    lethal_cache = {}

    def safe(c):
        v = lethal_cache.get(c)
        if v is None:
            x, r = c % n, c // n
            v = not bool(get_lethal_mask(manifest.seed, np.array([x]), np.array([r % n]),
                                         np.array([r // n]), rho)[0])
            lethal_cache[c] = v
        return v

    # Pré-calcul vectorisé de la létalité sur la bande autour du chemin
    band = set()
    frontier = set(uniq.tolist())
    for _ in range(reach):
        nxt = set()
        for c in frontier:
            for q in nbs(c):
                if fi[q] < 0 and q not in band:
                    nxt.add(q)
        band |= nxt
        frontier = nxt
    if not band:
        return set()
    band_ids = np.fromiter(band, dtype=np.int64, count=len(band))
    bx, by, bz = decode_coord(band_ids, n, n)
    ok = ~get_lethal_mask(manifest.seed, bx, by, bz, rho)
    for c, v in zip(band_ids.tolist(), ok.tolist()):
        lethal_cache[c] = v

    dist, src, par = {}, {}, {}
    out = set()

    def mark(c):
        while c is not None and c not in out:
            out.add(c)
            c = par.get(c)

    # Graines : cellules sûres voisines du chemin (distance 1)
    q = deque()
    for p in uniq.tolist():
        for c in nbs(p):
            if fi[c] >= 0 or not safe(c):
                continue
            f = fi[p]
            if c not in dist:
                dist[c], src[c], par[c] = 1, f, None
                q.append(c)
            else:
                if abs(f - src[c]) > 2:                 # 1 cellule relie deux étapes éloignées
                    out.add(c)
                if f < src[c]:
                    src[c] = f

    # Fronts : quand deux fronts d'étapes éloignées se touchent, détour retenu si gain
    while q:
        c = q.popleft()
        dc, sc = dist[c], src[c]
        for d in nbs(c):
            f = fi[d]
            if f >= 0:                                  # retour direct sur le chemin
                if dc + 1 < abs(f - sc):
                    mark(c)
                continue
            if d in dist:
                if dc + dist[d] + 1 < abs(src[d] - sc):
                    mark(c); mark(d)
            elif dc < reach and safe(d):
                dist[d], src[d], par[d] = dc + 1, sc, c
                q.append(d)
    return out


def _static_layers(manifest, result, p):
    """Couches qui ne dépendent que de la génération, de rho et des raccourcis :
    calculées une fois, puis réutilisées à chaque réglage d'affichage (coupe Z,
    mode roche, cubes du chemin, taille...). Un refresh ne fait plus que des
    masques numpy et l'écriture du maillage."""
    want_sc = p.rho > 0.0 and p.show_shortcuts
    key = (_CACHE["key"], round(p.rho, 4), p.sc_reach if want_sc else 0)
    st = _CACHE.get("static")
    if st is not None and st["key"] == key:
        return st

    n = manifest.nx
    n3 = n * n * n
    path_ids, path_lvl = _path_rooms(result)
    is_path = np.zeros(n3, dtype=bool)
    is_path[path_ids] = True

    # Matériau de roche par salle : Mur, Mortel (rouge), Raccourci (vert)
    cell_mat = np.full(n3, MAT_INDEX["Mur"], dtype=np.int32)
    sc_ids = np.empty(0, np.int64)
    if p.rho > 0.0:
        ax, ay, az = decode_coord(np.arange(n3, dtype=np.int64), n, n)
        cell_mat[get_lethal_mask(manifest.seed, ax, ay, az, p.rho)] = MAT_INDEX["Mortel"]
        if want_sc:
            sc = _cached_shortcuts(manifest, result, p.rho, p.sc_reach)
            sc_ids = np.fromiter(sc, dtype=np.int64, count=len(sc))
            cell_mat[sc_ids] = MAT_INDEX["Raccourci"]

    st = dict(key=key, path_ids=path_ids, path_lvl=path_lvl, is_path=is_path,
              cell_mat=cell_mat, sc_ids=sc_ids)
    _CACHE["static"] = st
    return st


def build_grid_mesh(mesh, manifest, result, p):
    """Grille PLEINE : un cube 1x1x1 par salle.

    Le chemin (largeur 1) est SOIT creusé (le cube est retiré, une seule salle
    par pas), SOIT rempli d'un cube coloré selon le segment de spline. Aucune
    autre salle n'est jamais retirée : rien n'est soustrait autour de la spline.
    """
    n = manifest.nx
    n2, n3 = n * n, n * n * n
    st = _static_layers(manifest, result, p)
    path_ids, path_lvl, is_path = st["path_ids"], st["path_lvl"], st["is_path"]
    top = min(p.cut_z, n - 1)

    # --- Roche : toute la grille, moins le chemin --------------------------------
    if p.wall_mode == 'NONE':
        rock_ids = np.empty(0, np.int64)
    else:
        rock = ~is_path                                 # nouveau tableau : le chemin n'est jamais de la roche
        if p.wall_mode == 'CUT':
            rock.reshape(n, n, n)[top + 1:] = False     # coupe : couches Z <= cut_z
        if p.optimize:
            rock = _visible_shell(rock, n)
        rock_ids = np.flatnonzero(rock)
    rock_mats = st["cell_mat"][rock_ids]

    # --- Cubes de chemin (si "avec cube") : couleur = segment de spline ----------
    if p.show_path_cubes and path_ids.size:
        vis = (path_ids // n2) <= top if p.wall_mode == 'CUT' else np.ones(path_ids.size, dtype=bool)
        show_ids = path_ids[vis]
        show_mats = MAT_INDEX["Voie0"] + np.clip(path_lvl[vis], 0, len(SPLINE_PALETTE) - 1)
    else:
        show_ids = np.empty(0, np.int64)
        show_mats = np.empty(0, np.int64)

    # --- Raccourcis isolés en mode "Chemin seul" --------------------------------
    sc_ids = st["sc_ids"] if p.wall_mode == 'NONE' else np.empty(0, np.int64)
    sc_mats = np.full(sc_ids.size, MAT_INDEX["Raccourci"], dtype=np.int64)

    ids = np.concatenate([rock_ids, show_ids, sc_ids])
    mats = np.concatenate([rock_mats, show_mats, sc_mats])

    x, y, z = decode_coord(ids, n, n)
    wx, wy, wz = _world_pos(x, y, z, n, p.room_size)
    centers = np.empty((ids.size, 3), dtype=np.float32)
    centers[:, 0], centers[:, 1], centers[:, 2] = wx, wy, wz

    if p.custom_room:
        _write_points(mesh, centers)                    # instanciation (≈ HISM UE)
    else:
        _write_cubes(mesh, centers, 0.96 * p.room_size, mats)
    return int(ids.size)


def build_path_curve(curve, manifest, result, smooth, pitch):
    curve.splines.clear()
    curve.dimensions = '3D'
    curve.bevel_depth = 0.15 * pitch
    curve.bevel_resolution = 3
    curve.use_fill_caps = True
    curve.materials.clear()
    for name, rgba, emi in SPLINE_PALETTE:
        curve.materials.append(_make_material("CS_" + name, rgba, emi))
    if not result.path:
        return

    n = manifest.nx
    # Découpe du chemin en segments : la couleur change à chaque clé ramassée
    segments = []
    prev_pt = None
    for s in result.path:
        x, y, z = decode_coord(s.room_id, manifest.nx, manifest.ny)
        pt = _world_pos(x, y, z, n, pitch)
        level = s.inventory.bit_length()
        if not segments or segments[-1][0] != level:
            segments.append((level, [prev_pt] if prev_pt else []))
        segments[-1][1].append(pt)
        prev_pt = pt

    for level, pts in segments:
        if len(pts) < 2:
            continue
        spline = curve.splines.new('NURBS' if smooth else 'POLY')
        spline.points.add(len(pts) - 1)
        spline.points.foreach_set("co", [c for p_ in pts for c in (*p_, 1.0)])
        spline.material_index = min(level, len(SPLINE_PALETTE) - 1)
        if smooth:
            spline.order_u = min(3, len(pts))
            spline.use_endpoint_u = True


def build_box(obj_mesh, n, pitch):
    lo = -n * pitch / 2.0
    hi = n * pitch / 2.0
    v = [(x, y, z) for z in (0.0, n * pitch) for y in (lo, hi) for x in (lo, hi)]
    e = [(0, 1), (2, 3), (4, 5), (6, 7), (0, 2), (1, 3), (4, 6), (5, 7), (0, 4), (1, 5), (2, 6), (3, 7)]
    obj_mesh.clear_geometry()
    obj_mesh.from_pydata(v, e, [])
    obj_mesh.update()


def build_portal_empty(name, portal, coll, seed, pitch):
    """Empty d'ancrage : flèche rouge (X local) = normale sortante, vers la passerelle.

    Volontairement NON parenté à CS_Grille : en mode « Salle Custom » la grille
    instancie ses enfants sur chaque sommet (instance_type = 'VERTS').
    """
    obj = bpy.data.objects.get(name)
    if obj is None:
        obj = bpy.data.objects.new(name, None)            # data None = Empty
    if obj.name not in coll.objects:
        coll.objects.link(obj)
    obj.empty_display_type = 'ARROWS'
    obj.empty_display_size = 1.5 * pitch
    obj.show_name = True
    obj.show_in_front = True
    obj.parent = None
    X, Z = Vector(portal.normal), Vector(portal.up)
    Y = Z.cross(X)                                       # repère direct
    rot = Matrix((X, Y, Z)).transposed().to_4x4()       # colonnes = axes locaux
    obj.matrix_world = Matrix.Translation(portal.position) @ rot
    obj["cs_face"] = DIRECTIONS[portal.face][0]
    obj["cs_face_index"] = portal.face
    obj["cs_room_id"] = portal.room_id
    obj["cs_cell"] = list(portal.cell)
    obj["cs_seed"] = seed
    return obj


def _remove_object(name):
    obj = bpy.data.objects.get(name)
    if obj:
        data = obj.data
        bpy.data.objects.remove(obj, do_unlink=True)
        if data and data.users == 0:
            (bpy.data.meshes if isinstance(data, bpy.types.Mesh) else bpy.data.curves).remove(data)


# --- Pipeline -----------------------------------------------------------------------
def _params_key(p):
    return (p.seed, p.size, round(p.path_len, 3), round(p.verticality, 3), p.n_keys)


def refresh_display(context):
    scene = context.scene
    p = scene.cube_solveur
    manifest, result = _CACHE["manifest"], _CACHE["result"]
    if manifest is None:
        return

    coll = _get_collection(scene)
    grid = _get_object(GRID_OBJ, lambda: bpy.data.meshes.new(GRID_OBJ), coll)
    path = _get_object(PATH_OBJ, lambda: bpy.data.curves.new(PATH_OBJ, 'CURVE'), coll)
    box = _get_object(BOX_OBJ, lambda: bpy.data.meshes.new(BOX_OBJ), coll)
    box.display_type = 'WIRE'
    box.hide_render = True
    box.hide_select = True

    p.stat_walls = build_grid_mesh(grid.data, manifest, result, p)
    build_path_curve(path.data, manifest, result, p.smooth, p.room_size)
    build_box(box.data, manifest.nx, p.room_size)

    # Ancrages d'entrée / sortie : calculés depuis le chemin, jamais stockés
    entry = compute_portal(manifest, result.path, True, p.room_size)
    exit_ = compute_portal(manifest, result.path, False, p.room_size)
    if entry and exit_:
        build_portal_empty(ENTRY_OBJ, entry, coll, manifest.seed, p.room_size)
        build_portal_empty(EXIT_OBJ, exit_, coll, manifest.seed, p.room_size)
    else:
        _remove_object(ENTRY_OBJ)
        _remove_object(EXIT_OBJ)

    # Distance d'ancrage à ancrage, stockée sur la spline (lisible par un export vers UE)
    dist = compute_path_distance(manifest, result.path, p.room_size)
    if dist:
        path["cs_distance_m"] = dist.total
        path["cs_distance_horizontale_m"] = dist.horizontal
        path["cs_distance_verticale_m"] = dist.vertical
        path["cs_distance_directe_m"] = dist.direct

    # Gestion de l'instanciation de la salle custom
    # // NOTE POUR UE5/C++ : Dans Unreal, cela correspond à l'utilisation de 
    # // Hierarchical Instanced Static Mesh (HISM). Les instances sont ajoutées via
    # // AddInstance() au lieu de spawner des milliers d'AActors.
    if getattr(p, "custom_room", None):
        grid.instance_type = 'VERTS'
        grid.show_instancer_for_viewport = False
        grid.show_instancer_for_render = False
        p.custom_room.parent = grid
    else:
        grid.instance_type = 'NONE'
        grid.show_instancer_for_viewport = True
        grid.show_instancer_for_render = True


def run_pipeline(context):
    p = context.scene.cube_solveur
    key = _params_key(p)
    if _CACHE["key"] != key:
        manifest, result = build_solved(p.seed, p.size, p.path_len, p.verticality, p.n_keys)
        _CACHE.update(key=key, manifest=manifest, result=result)
    manifest, result = _CACHE["manifest"], _CACHE["result"]

    refresh_display(context)

    p.stat_solved = bool(result.path)
    p.stat_length = (len(result.path) - 1) if result.path else -1
    p.stat_states = result.n_states
    p.stat_rooms = result.n_rooms
    p.stat_total = manifest.nx * manifest.ny * manifest.nz
    p.stat_ms = result.time_ms
    p.stat_attempt = manifest.attempt
    return result


def _on_display_change(self, context):
    # Réglages d'affichage : mise à jour instantanée sans relancer le solveur
    if _CACHE["key"] is not None and _CACHE["key"] == _params_key(self):
        refresh_display(context)


# --- Propriétés -----------------------------------------------------------------
class CubeSolveurProps(bpy.types.PropertyGroup):
    seed: IntProperty(name="Seed", default=1, min=SEED_MIN, max=SEED_MAX)
    size: IntProperty(name="Taille N (N³)", default=GRID_SIZE, min=8, max=GRID_SIZE,
                      description="Côté de la grille. 64 = 262 144 salles")
    room_size: FloatProperty(name="Taille (m)", default=1.0, min=0.1, max=1000.0, update=_on_display_change,
                             description="Taille physique d'une salle et espacement entre les instances")
    n_keys: IntProperty(name="Clés / portes", default=2, min=0, max=3,
                        description="Nombre de plans-barrières à porte verrouillée")
    path_len: FloatProperty(name="Longueur", default=0.45, min=0.0, max=1.0, precision=2,
                            description="0 = chemin court et direct ; 1 = chemin long et sinueux qui remplit le cube")
    verticality: FloatProperty(name="Verticalité", default=0.18, min=0.02, max=1.0, precision=2,
                               description="Fréquence des puits reliant les étages en Z (bas = étages nets)")
    rho: FloatProperty(name="Densité Mortelle", default=0.25, min=0.0, max=1.0, update=_on_display_change,
                       description="Proportion de fausses routes qui sont des pièges mortels (0 = Facile, >0 = Impossible)")
    wall_mode: EnumProperty(
        name="Roche",
        items=[
            ('FULL', "Grille pleine", "Toute la grille en cubes (le chemin est creusé ou coloré)"),
            ('CUT', "Coupe Z", "Grille tronquée à la couche Z choisie, pour voir l'intérieur"),
            ('NONE', "Chemin seul", "Aucune roche : uniquement le chemin et la spline"),
        ],
        default='CUT', update=_on_display_change)
    show_path_cubes: BoolProperty(
        name="Cubes sur le chemin", default=False, update=_on_display_change,
        description="Décoché : le chemin est creusé (tunnel 1x1x1 par salle). "
                    "Coché : chaque salle du chemin devient un cube coloré selon la spline")
    show_shortcuts: BoolProperty(
        name="Raccourcis sûrs (verts)", default=True, update=_on_display_change,
        description="Affiche les cubes de raccourcis alternatifs sûrs reliant deux étapes du chemin (vert)")
    sc_reach: IntProperty(
        name="Portée raccourcis", default=2, min=1, max=8, update=_on_display_change,
        description="Distance maximale (en salles) entre un raccourci et le chemin")
    cut_z: IntProperty(name="Couche Z", default=GRID_SIZE // 2, min=0, max=GRID_SIZE - 1,
                       update=_on_display_change)
    optimize: BoolProperty(
        name="Masquer les cubes invisibles", default=True, update=_on_display_change,
        description="N'affiche que les cubes qui touchent du vide : image identique, bien plus léger")
    smooth: BoolProperty(name="Spline lissée", default=True, update=_on_display_change)
    custom_room: PointerProperty(
        type=bpy.types.Object, name="Salle Custom", update=_on_display_change, 
        description="Objet à instancier au lieu des cubes générés (Similaire aux HISM d'Unreal)")

    stat_solved: BoolProperty(default=False)
    stat_length: IntProperty(default=-1)
    stat_states: IntProperty(default=0)
    stat_rooms: IntProperty(default=0)
    stat_total: IntProperty(default=0)
    stat_walls: IntProperty(default=0)
    stat_ms: FloatProperty(default=0.0)
    stat_attempt: IntProperty(default=0)


def _fmt_m(meters):
    """Distance lisible : « 306 m », « 1,53 km », « 12,5 m »."""
    if meters >= 1000.0:
        return ("%.2f km" % (meters / 1000.0)).replace(".", ",")
    if abs(meters - round(meters)) < 1e-9:
        return "%d m" % round(meters)
    return ("%.2f m" % meters).replace(".", ",")


# --- Opérateurs -----------------------------------------------------------------
def _report(op, p, res):
    if res.path:
        dist = compute_path_distance(_CACHE["manifest"], res.path, p.room_size)
        op.report({'INFO'}, "Seed %d : chemin de %d pas, %s à parcourir (%d états)"
                  % (p.seed, p.stat_length, _fmt_m(dist.total), p.stat_states))
    else:
        op.report({'WARNING'}, "Seed %d : aucune solution trouvée" % p.seed)


class CUBE_SOLVEUR_OT_generate(bpy.types.Operator):
    bl_idname = "cube_solveur.generate"
    bl_label = "Générer"
    bl_description = "Génère la grille N³ pour la seed courante et calcule le passage"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        context.window.cursor_set('WAIT')
        res = run_pipeline(context)
        context.window.cursor_set('DEFAULT')
        _report(self, context.scene.cube_solveur, res)
        return {'FINISHED'}


class CUBE_SOLVEUR_OT_reload(bpy.types.Operator):
    bl_idname = "cube_solveur.reload"
    bl_label = "Reload"
    bl_description = "Tire une nouvelle seed (1 à 50) et recalcule le passage"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        p = context.scene.cube_solveur
        p.seed = random.SystemRandom().choice([s for s in range(SEED_MIN, SEED_MAX + 1) if s != p.seed])
        context.window.cursor_set('WAIT')
        res = run_pipeline(context)
        context.window.cursor_set('DEFAULT')
        _report(self, p, res)
        return {'FINISHED'}


class CUBE_SOLVEUR_OT_clear(bpy.types.Operator):
    bl_idname = "cube_solveur.clear"
    bl_label = "Supprimer"
    bl_description = "Supprime la grille, la spline, le cadre et les ancrages"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        for name in (GRID_OBJ, PATH_OBJ, BOX_OBJ, ENTRY_OBJ, EXIT_OBJ):
            _remove_object(name)
        _CACHE.update(key=None, manifest=None, result=None, sc_key=None, shortcuts=set(), static=None)
        p = context.scene.cube_solveur
        p.stat_length = -1
        p.stat_solved = False
        return {'FINISHED'}


# --- Panneau (barre latérale N) ------------------------------------------------
class VIEW3D_PT_cube_solveur(bpy.types.Panel):
    bl_label = "Cube Solveur 3D"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Cube Solveur"

    def draw(self, context):
        p = context.scene.cube_solveur
        layout = self.layout

        col = layout.column(align=True)
        col.prop(p, "seed")
        row = col.row(align=True)
        row.scale_y = 1.4
        row.operator("cube_solveur.generate", icon='MESH_CUBE')
        row.operator("cube_solveur.reload", icon='FILE_REFRESH')

        box = layout.box()
        box.label(text="Génération", icon='PREFERENCES')
        box.prop(p, "size")
        box.prop(p, "n_keys")
        box.prop(p, "path_len", slider=True)
        box.prop(p, "verticality", slider=True)
        box.prop(p, "rho", slider=True)

        box = layout.box()
        box.label(text="Affichage", icon='HIDE_OFF')
        box.prop(p, "custom_room")
        box.prop(p, "room_size")
        box.prop(p, "wall_mode", text="")
        if p.wall_mode == 'CUT':
            box.prop(p, "cut_z", slider=True)
        box.prop(p, "show_path_cubes")
        box.prop(p, "show_shortcuts")
        if p.show_shortcuts:
            box.prop(p, "sc_reach")
        if p.wall_mode != 'NONE':
            box.prop(p, "optimize")
            if not p.optimize:
                box.label(text="Lourd : jusqu'à 262 144 cubes", icon='ERROR')
        box.prop(p, "smooth")

        box = layout.box()
        box.label(text="Résultat BFS", icon='INFO')
        if p.stat_length < 0 and not p.stat_solved:
            box.label(text="Pas encore généré")
        elif p.stat_solved:
            box.label(text="Chemin optimal : %d pas" % p.stat_length, icon='CHECKMARK')
            box.label(text="États uniques : %d" % p.stat_states)
            box.label(text="Salles explorées : %d / %d" % (p.stat_rooms, p.stat_total))
            box.label(text="Temps BFS : %.0f ms" % p.stat_ms)
            box.label(text="Cubes affichés : %d" % p.stat_walls)
            if p.stat_attempt:
                box.label(text="Sous-seed n°%d (seed initiale insoluble)" % p.stat_attempt)
        else:
            box.label(text="Aucune solution", icon='ERROR')

        manifest, result = _CACHE["manifest"], _CACHE["result"]
        if manifest is not None and result is not None and result.path:
            dist = compute_path_distance(manifest, result.path, p.room_size)
            box = layout.box()
            box.label(text="Distance entrée → sortie", icon='DRIVER_DISTANCE')
            box.label(text="À parcourir : %s" % _fmt_m(dist.total))
            box.label(text="    horizontale %s, verticale %s" % (_fmt_m(dist.horizontal), _fmt_m(dist.vertical)))
            box.label(text="En ligne droite : %s" % _fmt_m(dist.direct))
            box.label(text="(%d pas + 1) × cube de %s" % (dist.steps, _fmt_m(dist.cell)))

            box = layout.box()
            box.label(text="Ancrages passerelle", icon='EMPTY_ARROWS')
            for label, is_entry in (("Entrée", True), ("Sortie", False)):
                pt = compute_portal(manifest, result.path, is_entry, p.room_size)
                if pt is None:
                    continue
                box.label(text="%s : face %s, cube %s" % (label, DIRECTIONS[pt.face][0], pt.cell))
                box.label(text="    (%.2f, %.2f, %.2f)" % pt.position)

        layout.operator("cube_solveur.clear", icon='TRASH')


CLASSES = (
    CubeSolveurProps,
    CUBE_SOLVEUR_OT_generate,
    CUBE_SOLVEUR_OT_reload,
    CUBE_SOLVEUR_OT_clear,
    VIEW3D_PT_cube_solveur,
)


def register():
    if hasattr(bpy.types.Scene, "cube_solveur"):
        del bpy.types.Scene.cube_solveur
    # Permet de relancer le script depuis l'éditeur de texte sans erreur
    for cls in reversed(CLASSES):
        old = getattr(bpy.types, cls.__name__, None)
        if old is not None:
            try:
                bpy.utils.unregister_class(old)
            except RuntimeError:
                pass
    for cls in CLASSES:
        bpy.utils.register_class(cls)
    bpy.types.Scene.cube_solveur = PointerProperty(type=CubeSolveurProps)


def unregister():
    if hasattr(bpy.types.Scene, "cube_solveur"):
        del bpy.types.Scene.cube_solveur
    for cls in reversed(CLASSES):
        try:
            bpy.utils.unregister_class(cls)
        except RuntimeError:
            pass


if __name__ == "__main__":
    register()
