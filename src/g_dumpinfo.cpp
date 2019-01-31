/*
 ** g_dumpinfo.cpp
 ** diagnostic CCMDs that output info about the current game
 **
 **---------------------------------------------------------------------------
 ** Copyright 1998-2016 Randy Heit
 ** Copyright 2003-2018 Christoph Oelckers
 ** All rights reserved.
 **
 ** Redistribution and use in source and binary forms, with or without
 ** modification, are permitted provided that the following conditions
 ** are met:
 **
 ** 1. Redistributions of source code must retain the above copyright
 **    notice, this list of conditions and the following disclaimer.
 ** 2. Redistributions in binary form must reproduce the above copyright
 **    notice, this list of conditions and the following disclaimer in the
 **    documentation and/or other materials provided with the distribution.
 ** 3. The name of the author may not be used to endorse or promote products
 **    derived from this software without specific prior written permission.
 **
 ** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 ** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 ** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 ** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 ** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 ** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 ** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 ** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 ** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 ** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **---------------------------------------------------------------------------
 **
 */

#include "c_dispatch.h"
#include "g_levellocals.h"
#include "a_dynlight.h"
#include "a_sharedglobal.h"
#include "d_net.h"

//==========================================================================
//
// CCMDs
//
//==========================================================================

CCMD(listlights)
{
	int walls, sectors;
	int allwalls=0, allsectors=0, allsubsecs = 0;
	int i=0, shadowcount = 0;
	FDynamicLight * dl;
	
	for (auto Level : AllLevels())
	{
		Printf("Lights for %s\n", Level->MapName.GetChars());
		for (dl = Level->lights; dl; dl = dl->next)
		{
			walls=0;
			sectors=0;
			Printf("%s at (%f, %f, %f), color = 0x%02x%02x%02x, radius = %f %s %s",
				   dl->target->GetClass()->TypeName.GetChars(),
				   dl->X(), dl->Y(), dl->Z(), dl->GetRed(), dl->GetGreen(), dl->GetBlue(),
				   dl->radius, dl->IsAttenuated()? "attenuated" : "", dl->shadowmapped? "shadowmapped" : "");
			i++;
			shadowcount += dl->shadowmapped;
			
			if (dl->target)
			{
				FTextureID spr = sprites[dl->target->sprite].GetSpriteFrame(dl->target->frame, 0, 0., nullptr);
				Printf(", frame = %s ", TexMan.GetTexture(spr)->GetName().GetChars());
			}
			
			
			FLightNode * node;
			
			node=dl->touching_sides;
			
			while (node)
			{
				walls++;
				allwalls++;
				node = node->nextTarget;
			}
			
			
			node = dl->touching_sector;
			
			while (node)
			{
				allsectors++;
				sectors++;
				node = node->nextTarget;
			}
			Printf("- %d walls, %d sectors\n", walls, sectors);
			
		}
		Printf("%i dynamic lights, %d shadowmapped, %d walls, %d sectors\n\n\n", i, shadowcount, allwalls, allsectors);
	}
}

CCMD (countdecals)
{
	for (auto Level : AllLevels())
	{
		auto iterator = Level->GetThinkerIterator<DImpactDecal>(NAME_None, STAT_AUTODECAL);
		int count = 0;
		
		while (iterator.Next())
			count++;
		
		Printf("%s: Counted %d impact decals\n", Level->MapName.GetChars(), count);
	}
}

CCMD (spray)
{
	if (who == NULL || argv.argc() < 2)
	{
		Printf ("Usage: spray <decal>\n");
		return;
	}
	
	Net_WriteByte (DEM_SPRAY);
	Net_WriteString (argv[1]);
}

//==========================================================================
//
// CCMD mapchecksum
//
//==========================================================================

CCMD (mapchecksum)
{
	MapData *map;
	uint8_t cksum[16];
	
	if (argv.argc() < 2)
	{
		Printf("Usage: mapchecksum <map> ...\n");
	}
	for (int i = 1; i < argv.argc(); ++i)
	{
		map = P_OpenMapData(argv[i], true);
		if (map == NULL)
		{
			Printf("Cannot load %s as a map\n", argv[i]);
		}
		else
		{
			map->GetChecksum(cksum);
			const char *wadname = Wads.GetWadName(Wads.GetLumpFile(map->lumpnum));
			delete map;
			for (size_t j = 0; j < sizeof(cksum); ++j)
			{
				Printf("%02X", cksum[j]);
			}
			Printf(" // %s %s\n", wadname, argv[i]);
		}
	}
}

//==========================================================================
//
// CCMD hiddencompatflags
//
//==========================================================================

CCMD (hiddencompatflags)
{
	for(auto Level : AllLevels())
	{
		Printf("%s: %08x %08x %08x\n", Level->MapName.GetChars(), Level->ii_compatflags, Level->ii_compatflags2, Level->ib_compatflags);
	}
}

CCMD(dumpportals)
{
	for (auto Level : AllLevels())
	{
		Printf("Portal groups for %s\n", Level->MapName.GetChars());
		for (unsigned i = 0; i < Level->portalGroups.Size(); i++)
		{
			auto p = Level->portalGroups[i];
			double xdisp = p->mDisplacement.X;
			double ydisp = p->mDisplacement.Y;
			Printf(PRINT_LOG, "Portal #%d, %s, displacement = (%f,%f)\n", i, p->plane == 0 ? "floor" : "ceiling",
				   xdisp, ydisp);
			Printf(PRINT_LOG, "Coverage:\n");
			for (auto &sub : Level->subsectors)
			{
				auto port = sub.render_sector->GetPortalGroup(p->plane);
				if (port == p)
				{
					Printf(PRINT_LOG, "\tSubsector %d (%d):\n\t\t", sub.Index(), sub.render_sector->sectornum);
					for (unsigned k = 0; k < sub.numlines; k++)
					{
						Printf(PRINT_LOG, "(%.3f,%.3f), ", sub.firstline[k].v1->fX() + xdisp, sub.firstline[k].v1->fY() + ydisp);
					}
					Printf(PRINT_LOG, "\n\t\tCovered by subsectors:\n");
					FPortalCoverage *cov = &sub.portalcoverage[p->plane];
					for (int l = 0; l < cov->sscount; l++)
					{
						subsector_t *csub = &Level->subsectors[cov->subsectors[l]];
						Printf(PRINT_LOG, "\t\t\t%5d (%4d): ", cov->subsectors[l], csub->render_sector->sectornum);
						for (unsigned m = 0; m < csub->numlines; m++)
						{
							Printf(PRINT_LOG, "(%.3f,%.3f), ", csub->firstline[m].v1->fX(), csub->firstline[m].v1->fY());
						}
						Printf(PRINT_LOG, "\n");
					}
				}
			}
		}
	}
}

ADD_STAT (interpolations)
{
	FString out;
	for (auto Level : AllLevels())
	{
		if (out.Len() > 0) out << '\n';
		out.AppendFormat("%s: %d interpolations", Level->MapName.GetChars(), Level->interpolator.CountInterpolations ());
		
	}
	return out;
}

CCMD(printsections)
{
	void PrintSections(FLevelLocals *Level);
	for (auto Level : AllLevels())
	{
		PrintSections(Level);
	}
}


