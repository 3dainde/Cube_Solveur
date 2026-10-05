#include "CubeViewLibrary.h"
#include "CubeRebusLibrary.h"

void UCubeViewLibrary::BuildCellMaterials(const FCubeManifest& M, const FCubeViewParams& Params,
	const TSet<int32>& Shortcuts, TArray<uint8>& OutCellMaterials)
{
	const int32 N = M.GridSize;
	const int32 N3 = N * N * N;
	OutCellMaterials.Init(static_cast<uint8>(ECubeViewMaterial::Wall), N3);
	if (Params.Rho <= 0.0f)
	{
		return;
	}

	const uint32 TrapHash = UCubeRebusLibrary::GetTrapSeedHash(M.Seed);
	const int32 RhoLimit = UCubeRebusLibrary::GetRhoLimit(static_cast<double>(Params.Rho));
	for (int32 ID = 0; ID < N3; ++ID)
	{
		const FCubeCoordinate P = FCubeMath::DecodeCoord(ID, N, N);
		if (UCubeRebusLibrary::IsLethalFast(TrapHash, P.X, P.Y, P.Z, RhoLimit))
		{
			OutCellMaterials[ID] = static_cast<uint8>(ECubeViewMaterial::Lethal);
		}
	}
	if (Params.bShowShortcuts)
	{
		for (const int32 ID : Shortcuts)
		{
			OutCellMaterials[ID] = static_cast<uint8>(ECubeViewMaterial::Shortcut);
		}
	}
}

FCubeViewInstances UCubeViewLibrary::BuildView(const FCubeManifest& M, const FCubeViewParams& Params, const TSet<int32>& Shortcuts)
{
	TArray<uint8> CellMaterials;
	BuildCellMaterials(M, Params, Shortcuts, CellMaterials);
	FCubeViewInstances Out;
	BuildViewCached(M, Params, Shortcuts, CellMaterials, Out);
	return Out;
}

void UCubeViewLibrary::BuildViewCached(const FCubeManifest& M, const FCubeViewParams& Params,
	const TSet<int32>& Shortcuts, const TArray<uint8>& CellMaterials, FCubeViewInstances& Out)
{
	Out.RoomIDs.Reset();
	Out.Materials.Reset();
	const int32 N = M.GridSize;
	const int32 Layer = N * N;
	const int32 N3 = Layer * N;
	if (N <= 0 || CellMaterials.Num() != N3)
	{
		return;
	}
	const int32 Top = FMath::Min(Params.CutZ, N - 1);
	const bool bCut = Params.WallMode == ECubeWallMode::Cut;

	// Salles du chemin, dans l'ordre de première visite, avec le niveau de clé le plus élevé atteint.
	TArray<int32> PathLevel;
	PathLevel.Init(INDEX_NONE, N3);
	TArray<int32> PathRooms;
	for (const FCubeState& S : M.SolutionPath)
	{
		int32& Level = PathLevel[S.RoomID];
		if (Level == INDEX_NONE)
		{
			PathRooms.Add(S.RoomID);
		}
		Level = FMath::Max(Level, S.KeyLevel());
	}

	// --- Roche : toute la grille hors chemin, tronquée à la coupe, réduite à l'enveloppe visible ----
	if (Params.WallMode != ECubeWallMode::None)
	{
		const int32 ZEnd = bCut ? Top + 1 : N;                  // couches 0..ZEnd-1
		auto IsRock = [&](int32 X, int32 Y, int32 Z)
		{
			return Z < ZEnd && PathLevel[X + Y * N + Z * Layer] == INDEX_NONE;
		};
		Out.RoomIDs.Reserve(Params.bOptimize ? 6 * Layer : ZEnd * Layer);
		Out.Materials.Reserve(Params.bOptimize ? 6 * Layer : ZEnd * Layer);
		for (int32 Z = 0; Z < ZEnd; ++Z)
		{
			for (int32 Y = 0; Y < N; ++Y)
			{
				for (int32 X = 0; X < N; ++X)
				{
					if (!IsRock(X, Y, Z))
					{
						continue;
					}
					if (Params.bOptimize)
					{
						// Visible si un voisin est hors grille ou vide (chemin, ou au-dessus de la coupe).
						const bool bExposed =
							X == 0 || X == N - 1 || Y == 0 || Y == N - 1 || Z == 0 || Z == N - 1
							|| !IsRock(X - 1, Y, Z) || !IsRock(X + 1, Y, Z)
							|| !IsRock(X, Y - 1, Z) || !IsRock(X, Y + 1, Z)
							|| !IsRock(X, Y, Z - 1) || !IsRock(X, Y, Z + 1);
						if (!bExposed)
						{
							continue;
						}
					}
					const int32 ID = X + Y * N + Z * Layer;
					Out.RoomIDs.Add(ID);
					Out.Materials.Add(CellMaterials[ID]);
				}
			}
		}
	}

	// --- Chemin coloré selon la dernière clé détenue -------------------------------------------------
	if (Params.bShowPathCubes)
	{
		for (const int32 ID : PathRooms)
		{
			if (bCut && ID / Layer > Top)
			{
				continue;
			}
			const int32 Level = FMath::Clamp(PathLevel[ID], 0, 3);
			Out.RoomIDs.Add(ID);
			Out.Materials.Add(static_cast<uint8>(static_cast<int32>(ECubeViewMaterial::Path0) + Level));
		}
	}

	// --- Raccourcis isolés en mode « Chemin seul » ----------------------------------------------------
	if (Params.WallMode == ECubeWallMode::None && Params.bShowShortcuts && Params.Rho > 0.0f)
	{
		TArray<int32> Sorted = Shortcuts.Array();
		Sorted.Sort();
		for (const int32 ID : Sorted)
		{
			Out.RoomIDs.Add(ID);
			Out.Materials.Add(static_cast<uint8>(ECubeViewMaterial::Shortcut));
		}
	}
}
