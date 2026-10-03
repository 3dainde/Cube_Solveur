#pragma once

#include "CoreMinimal.h"

/**
 * Hashage déterministe canonique du Cube (seed -> chemin, champ mortel, formules, indices).
 * On reproduit CRC32 (polynôme IEEE 802.3, identique à zlib.crc32) combiné au mélangeur
 * "boost::hash_combine". Toute l'arithmétique est en uint32 (repli 32 bits naturel),
 * ce qui rend le résultat identique sur toutes les plateformes.
 */
namespace CubeHash
{
    inline uint32 Crc32(const uint8* Data, int32 Len)
    {
        static uint32 Table[256];
        static bool bInit = false;
        if (!bInit)
        {
            for (uint32 i = 0; i < 256; ++i)
            {
                uint32 c = i;
                for (int32 k = 0; k < 8; ++k)
                {
                    c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
                }
                Table[i] = c;
            }
            bInit = true;
        }
        uint32 c = 0xFFFFFFFFu;
        for (int32 i = 0; i < Len; ++i)
        {
            c = Table[(c ^ Data[i]) & 0xFFu] ^ (c >> 8);
        }
        return c ^ 0xFFFFFFFFu;
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
