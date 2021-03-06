/*
 * Copyright (c) 2007, Laminar Research.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "WED_ObjPlacement.h"
#include "WED_EnumSystem.h"
#include "WED_ResourceMgr.h"
#include "WED_ToolUtils.h"
#include "XObjDefs.h"
#include "MathUtils.h"
#include "CompGeomDefs2.h"
#include "XESConstants.h"

DEFINE_PERSISTENT(WED_ObjPlacement)
TRIVIAL_COPY(WED_ObjPlacement,WED_GISPoint_Heading)

WED_ObjPlacement::WED_ObjPlacement(WED_Archive * a, int i) : 
	WED_GISPoint_Heading(a,i),
#if AIRPORT_ROUTING
	has_msl(this,PROP_Name("Set MSL", XML_Name("obj_placement","custom_msl")),0),
	msl    (this,PROP_Name("MSL",     XML_Name("obj_placement","msl")), 0, 5,3),
#endif
	resource  (this,PROP_Name("Resource",  XML_Name("obj_placement","resource")),""),
	show_level(this,PROP_Name("Show with", XML_Name("obj_placement","show_level")),ShowLevel, show_Level1),
	visibleWithinDeg(-1.0)
{
}

WED_ObjPlacement::~WED_ObjPlacement()
{
}

void		WED_ObjPlacement::SetShowLevel(int sl)
{
	show_level = ENUM_Import(ShowLevel,sl);
}

int			WED_ObjPlacement::GetShowLevel(void) const
{
	return ENUM_Export(show_level.value);
}

void		WED_ObjPlacement::GetResource(	  string& r) const
{
	r = resource.value;
}

void		WED_ObjPlacement::SetResource(const string& r)
{
	resource = r;
	visibleWithinDeg = -1.0; // force re-evaluation when object is changed
}

double 	WED_ObjPlacement::GetVisibleDeg(void) const
{
	return visibleWithinDeg; // one note - 
}

bool		WED_ObjPlacement::Cull(const Bbox2& b) const
{

	// caching the objects dimension here for off-display culling in the map view. Its disregarding object rotation
	// and any lattitude dependency in the conversion, so the value will be choosen sufficiently pessimistic.

	if(visibleWithinDeg < 0.0)                            // so we only do this once for each object, ever
	{
		float * f = (float *) &visibleWithinDeg;          // tricking the compiler, breaking all rules. But Cull() must be const ...
		*f = GLOBAL_WED_ART_ASSET_FUDGE_FACTOR;           // the old, brain-dead visibility rule of thumb
		IResolver * res = GetArchive()->GetResolver();
		if(res)
		{
			WED_ResourceMgr * rmgr = WED_GetResourceMgr(res);
			if(rmgr)
			{
				XObj8 * o;
				agp_t agp;
				Point2	my_loc;
				GetLocation(gis_Geo,my_loc);
				double mtr_to_lon = MTR_TO_DEG_LAT / cos(my_loc.y() * DEG_TO_RAD);

	//			int n = GetNumVariants(resource.value);   // no need to cycle through these - only the first variant is used for preview
				if(rmgr->GetObj(resource.value,o))
				{
					*f = pythag(o->xyz_max[0] - o->xyz_min[0], o->xyz_max[2] - o->xyz_min[2]) * mtr_to_lon;
				}
				else if(rmgr->GetAGP(resource.value,agp))
				{
					double min_xy[2] = { 0, 0 };
					double max_xy[2] = { 0, 0 };
					for(int n = 0; n < agp.tile.size(); n += 4)
					{
						min_xy[0] = min(min_xy[0],agp.tile[n]);
						max_xy[0] = max(max_xy[0],agp.tile[n]);
						min_xy[1] = min(min_xy[1],agp.tile[n+1]);
						max_xy[1] = max(max_xy[1],agp.tile[n+1]);
					}
					*f = pythag(max_xy[0] - min_xy[0],	max_xy[1] - min_xy[1]) * mtr_to_lon;
				}
			}
		}
	}

// This woould be a radical approach to culling - drop ALL visible items, like preview/structure/handles/highlight if its too small.
// Thois will make items completely invisible, but keeps thm selectable. Very similar as the "too small to go in" apprach where whole
// groups or airport drop out of view.
// Results in only a verw cases in users surprises - as large group of isolated items (like forests drawn as individual tree objects).
// This practive of drawing trees comoes a popular "bad paractive" for gateway airports now - which result in sceneries that do not scale 
// very well with rendering settings.
// This concept could be enhances by passing an additional parameter into the Cull() function to allow items to cull themselves differently
// based on structure/preview or even tool views.
//
//	if(visibleWithinDeg < b.yspan() * 0.002) return false;      // cull objects by size as well. Makes structure/preview and handles all disappear

	Point2	my_loc;
	GetLocation(gis_Geo,my_loc);

	Bbox2	my_bounds(my_loc - Vector2(visibleWithinDeg,visibleWithinDeg), 
					  my_loc + Vector2(visibleWithinDeg,visibleWithinDeg));

	return b.overlap(my_bounds);
}

#if AIRPORT_ROUTING
bool		WED_ObjPlacement::HasCustomMSL(void) const
{
	return has_msl.value;
}

double		WED_ObjPlacement::GetCustomMSL(void) const
{
	return msl.value;
}

void		WED_ObjPlacement::SetCustomMSL(double in_msl)
{
	has_msl = 1;
	msl = in_msl;
}

void		WED_ObjPlacement::SetDefaultMSL(void)
{
	has_msl = 0;
}

#endif
