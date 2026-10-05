#pragma once

#include "CoreMinimal.h"

/**
 * Générateur pseudo-aléatoire DÉTERMINISTE du Cube (parité bit à bit avec la R&D Python).
 *
 * La R&D tire ses niveaux avec numpy.random.Generator(numpy.random.PCG64(seed)). Pour qu'une seed
 * donne exactement le même niveau dans Blender et dans Unreal, cette classe reproduit :
 *   - numpy SeedSequence (pool de 4 mots, hashmix / mix),
 *   - PCG64 XSL-RR 128/64 (multiplicateur 128 bits standard),
 *   - Generator.random()   : (next64 >> 11) * 2^-53,
 *   - Generator.integers() : Lemire 32 bits sur next32 (tampon 32 bits de numpy),
 *   - Generator.choice(p=) : somme cumulée normalisée + recherche "right".
 * Aucun FRandomStream / FMath::Rand : leur séquence n'a aucun rapport avec numpy.
 * Le hachage (CRC32, Combine) est dans CubeHash.h.
 */
/** PCG64 + SeedSequence, identique à numpy.random.Generator(numpy.random.PCG64(Seed)). */
class CUBE_API FCubeRandom
{
public:
	explicit FCubeRandom(uint32 Seed);

	/** Sortie brute 64 bits (pcg64_next64). */
	uint64 NextUInt64();

	/** 32 bits (pcg64_next32) : une sortie 64 bits sert deux fois, poids faible puis poids fort. */
	uint32 NextUInt32();

	/** Réel uniforme dans [0, 1) sur 53 bits (Generator.random()). */
	double NextDouble();

	/** Entier uniforme dans [0, MaxInclusive] (Generator.integers(0, MaxInclusive + 1)), MaxInclusive < 2^32 - 1. */
	uint32 BoundedUInt32(uint32 MaxInclusive);

	/** Index tiré selon des poids positifs (Generator.choice(Num, p = Weights / somme)). */
	int32 WeightedIndex(const double* Weights, int32 Num);

private:
	void Step();

	// État et incrément 128 bits, en deux mots de 64 bits (MSVC n'a pas de __int128).
	uint64 StateHi = 0;
	uint64 StateLo = 0;
	uint64 IncHi = 0;
	uint64 IncLo = 0;

	bool bHasUInt32 = false;
	uint32 UInt32Buffer = 0;
};
