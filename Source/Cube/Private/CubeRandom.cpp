#include "CubeRandom.h"

namespace
{
	// --- numpy SeedSequence -------------------------------------------------------------------
	constexpr uint32 SeqInitA = 0x43b0d7e5u;
	constexpr uint32 SeqMultA = 0x931e8875u;
	constexpr uint32 SeqInitB = 0x8b51f9ddu;
	constexpr uint32 SeqMultB = 0x58f38dedu;
	constexpr uint32 SeqMixL  = 0xca01f9ddu;
	constexpr uint32 SeqMixR  = 0x4973f715u;
	constexpr int32  SeqPoolSize = 4;

	FORCEINLINE uint32 SeqHashMix(uint32 Value, uint32& HashConst)
	{
		Value ^= HashConst;
		HashConst *= SeqMultA;
		Value *= HashConst;
		Value ^= Value >> 16;
		return Value;
	}

	FORCEINLINE uint32 SeqMix(uint32 X, uint32 Y)
	{
		uint32 R = SeqMixL * X - SeqMixR * Y;
		R ^= R >> 16;
		return R;
	}

	// --- PCG64 : multiplicateur 128 bits standard (0x2360ED051FC65DA4'4385DF649FCCF645) ---------
	constexpr uint64 PcgMultHi = 0x2360ED051FC65DA4ull;
	constexpr uint64 PcgMultLo = 0x4385DF649FCCF645ull;

	/** 64 x 64 -> 128 bits, portable (MSVC, Clang, GCC). */
	FORCEINLINE void Mul64To128(uint64 A, uint64 B, uint64& OutHi, uint64& OutLo)
	{
		const uint64 ALo = A & 0xFFFFFFFFull, AHi = A >> 32;
		const uint64 BLo = B & 0xFFFFFFFFull, BHi = B >> 32;
		const uint64 LL = ALo * BLo;
		const uint64 LH = ALo * BHi;
		const uint64 HL = AHi * BLo;
		const uint64 HH = AHi * BHi;
		const uint64 Mid = (LL >> 32) + (LH & 0xFFFFFFFFull) + (HL & 0xFFFFFFFFull);
		OutLo = (Mid << 32) | (LL & 0xFFFFFFFFull);
		OutHi = HH + (LH >> 32) + (HL >> 32) + (Mid >> 32);
	}

	FORCEINLINE void Add128(uint64& Hi, uint64& Lo, uint64 AddHi, uint64 AddLo)
	{
		const uint64 NewLo = Lo + AddLo;
		Hi = Hi + AddHi + (NewLo < Lo ? 1u : 0u);
		Lo = NewLo;
	}
}

// =============================================================================================
FCubeRandom::FCubeRandom(uint32 Seed)
{
	// SeedSequence(entropy = Seed) : un mot d'entropie, pool de 4 mots.
	uint32 Pool[SeqPoolSize];
	uint32 HashConst = SeqInitA;
	for (int32 I = 0; I < SeqPoolSize; ++I)
	{
		Pool[I] = SeqHashMix(I == 0 ? Seed : 0u, HashConst);
	}
	for (int32 Src = 0; Src < SeqPoolSize; ++Src)
	{
		for (int32 Dst = 0; Dst < SeqPoolSize; ++Dst)
		{
			if (Src != Dst)
			{
				const uint32 Hashed = SeqHashMix(Pool[Src], HashConst);
				Pool[Dst] = SeqMix(Pool[Dst], Hashed);
			}
		}
	}

	// generate_state(4, uint64) : 8 mots de 32 bits, assemblés en petit-boutiste.
	uint64 Words64[4];
	uint32 HashB = SeqInitB;
	for (int32 I = 0; I < 8; ++I)
	{
		uint32 V = Pool[I % SeqPoolSize];
		V ^= HashB;
		HashB *= SeqMultB;
		V *= HashB;
		V ^= V >> 16;
		if ((I & 1) == 0)
		{
			Words64[I / 2] = V;
		}
		else
		{
			Words64[I / 2] |= uint64(V) << 32;
		}
	}

	// pcg64_set_seed : initstate = W0:W1, initseq = W2:W3 (poids fort : poids faible).
	IncHi = (Words64[2] << 1) | (Words64[3] >> 63);
	IncLo = (Words64[3] << 1) | 1u;
	StateHi = 0;
	StateLo = 0;
	Step();
	Add128(StateHi, StateLo, Words64[0], Words64[1]);
	Step();
}

void FCubeRandom::Step()
{
	// State = State · Mult + Inc   (mod 2^128)
	uint64 Hi = 0;
	uint64 Lo = 0;
	Mul64To128(StateLo, PcgMultLo, Hi, Lo);
	Hi += StateLo * PcgMultHi + StateHi * PcgMultLo;
	Add128(Hi, Lo, IncHi, IncLo);
	StateHi = Hi;
	StateLo = Lo;
}

uint64 FCubeRandom::NextUInt64()
{
	Step();
	// XSL-RR : (hi ^ lo) tourné à droite des 6 bits de poids fort de l'état.
	const uint64 X = StateHi ^ StateLo;
	const uint32 Rot = static_cast<uint32>(StateHi >> 58);
	return (X >> Rot) | (X << ((64u - Rot) & 63u));
}

uint32 FCubeRandom::NextUInt32()
{
	if (bHasUInt32)
	{
		bHasUInt32 = false;
		return UInt32Buffer;
	}
	const uint64 Next = NextUInt64();
	bHasUInt32 = true;
	UInt32Buffer = static_cast<uint32>(Next >> 32);
	return static_cast<uint32>(Next & 0xFFFFFFFFull);
}

double FCubeRandom::NextDouble()
{
	return static_cast<double>(NextUInt64() >> 11) * (1.0 / 9007199254740992.0);
}

uint32 FCubeRandom::BoundedUInt32(uint32 MaxInclusive)
{
	if (MaxInclusive == 0)
	{
		return 0;                                           // numpy ne tire rien dans ce cas
	}
	check(MaxInclusive < 0xFFFFFFFFu);

	// Lemire (numpy buffered_bounded_lemire_uint32)
	const uint32 RangeExcl = MaxInclusive + 1u;
	uint64 M = uint64(NextUInt32()) * RangeExcl;
	uint32 Leftover = static_cast<uint32>(M & 0xFFFFFFFFull);
	if (Leftover < RangeExcl)
	{
		const uint32 Threshold = (0xFFFFFFFFu - MaxInclusive) % RangeExcl;
		while (Leftover < Threshold)
		{
			M = uint64(NextUInt32()) * RangeExcl;
			Leftover = static_cast<uint32>(M & 0xFFFFFFFFull);
		}
	}
	return static_cast<uint32>(M >> 32);
}

int32 FCubeRandom::WeightedIndex(const double* Weights, int32 Num)
{
	check(Num > 0 && Num <= 8);

	// Même arithmétique que numpy : p = w / somme, cdf = cumsum(p), cdf /= cdf[-1],
	// index = searchsorted(cdf, u, 'right'). L'ordre des opérations est contractuel.
	double Sum = 0.0;
	for (int32 I = 0; I < Num; ++I)
	{
		Sum += Weights[I];
	}
	double Cdf[8];
	double Acc = 0.0;
	for (int32 I = 0; I < Num; ++I)
	{
		Acc += Weights[I] / Sum;
		Cdf[I] = Acc;
	}
	const double Last = Cdf[Num - 1];
	const double U = NextDouble();
	for (int32 I = 0; I < Num; ++I)
	{
		if (U < Cdf[I] / Last)
		{
			return I;
		}
	}
	return Num - 1;
}
