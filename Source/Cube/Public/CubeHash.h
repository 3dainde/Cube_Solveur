#pragma once

#include "CoreMinimal.h"

/**
 * Hachage déterministe canonique du Cube (seed -> chemin, champ mortel, formules, indices).
 * CRC32 (polynôme IEEE 802.3, identique à zlib.crc32) + mélangeur boost::hash_combine.
 * Toute l'arithmétique est en uint32 (repli 32 bits naturel) : résultat identique sur toutes
 * les plateformes et identique à la R&D Python.
 *
 * Seeds : on hache les octets UTF-8 de la chaîne. Une seed numérique « 7 » donne donc le même
 * hash que str(7) côté Python. Ne pas utiliser FCrc::StrCrc32 (il hache des TCHAR UTF-16).
 */
namespace CubeHash
{
	namespace Detail
	{
		/** Table CRC32 calculée à la compilation (pas d'initialisation paresseuse : thread-safe). */
		struct FCrc32Table
		{
			uint32 Entries[256];

			constexpr FCrc32Table() : Entries()
			{
				for (uint32 I = 0; I < 256; ++I)
				{
					uint32 C = I;
					for (int32 K = 0; K < 8; ++K)
					{
						C = (C & 1u) ? (0xEDB88320u ^ (C >> 1)) : (C >> 1);
					}
					Entries[I] = C;
				}
			}
		};

		inline constexpr FCrc32Table Crc32Table;
	}

	/** CRC32 (= zlib.crc32). Crc permet de chaîner plusieurs blocs. */
	inline uint32 Crc32(const uint8* Data, int32 Len, uint32 Crc = 0)
	{
		uint32 C = ~Crc;
		for (int32 I = 0; I < Len; ++I)
		{
			C = Detail::Crc32Table.Entries[(C ^ Data[I]) & 0xFFu] ^ (C >> 8);
		}
		return ~C;
	}

	/** CRC32 d'une chaîne ASCII terminée par 0 (constantes "PATH", "TRAP", "SIZE"...). */
	inline uint32 Crc32Ascii(const char* Text)
	{
		int32 Len = 0;
		while (Text[Len] != '\0')
		{
			++Len;
		}
		return Crc32(reinterpret_cast<const uint8*>(Text), Len);
	}

	/** CRC32 d'une chaîne encodée en UTF-8. */
	inline uint32 Crc32Str(const FString& S)
	{
		FTCHARToUTF8 Utf8(*S);
		return Crc32(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	}

	/** hash_combine(a, b) = a ^ (b + 0x9E3779B9 + (a << 6) + (a >> 2))  (mod 2^32). */
	inline uint32 Combine(uint32 A, uint32 B)
	{
		return A ^ (B + 0x9E3779B9u + (A << 6) + (A >> 2));
	}
}
