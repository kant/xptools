/*
 * Copyright (c) 2004, Laminar Research.
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
#ifndef NETTABLES_H
#define NETTABLES_H

struct	NetFeatureInfo {
	float		density_factor;
	int			one_way;
};
typedef hash_map<int, NetFeatureInfo>	NetFeatureInfoTable;

struct	NetEntityInfo {
	float		width;
	float		pad;
	float		building_percent;
	float		max_slope;
	int			use_mode;
	int			export_type_normal;
	int			export_type_overpass;
};
typedef hash_map<int, NetEntityInfo>			NetEntityInfoTable;

struct	Road2NetInfo {
	float		min_density;
	float		max_density;
	int			entity_type;
};
typedef hash_multimap<int, Road2NetInfo>		Road2NetInfoTable;

struct	BridgeInfo {
	int			entity_type;

	// Rulez
	float		min_length;
	float		max_length;
	float		min_seg_length;
	float		max_seg_length;
	float		min_seg_count;
	float		max_seg_count;
	float		curve_limit;	// Expressed as a DOT product (Cosine) - 0 means no limit, 1.0 means straight!

	// Splitting
	int			split_count;
	float		split_length;
	int			split_arch;

	// Geometry
	float		min_start_agl;
	float		max_start_agl;
	float		search_dist;
	float		pref_start_agl;

	float		min_center_agl;
	float		max_center_agl;
	float		height_ratio;
	float		road_slope;

	// Export to X-Plane
	int			export_type;
};
typedef	vector<BridgeInfo>				BridgeInfoTable;


extern	NetFeatureInfoTable				gNetFeatures;
extern 	NetEntityInfoTable				gNetEntities;
extern	Road2NetInfoTable				gRoad2Net;
extern	BridgeInfoTable					gBridgeInfo;
void	LoadNetFeatureTables(void);

bool	IsSeparatedHighway(int road_type);
int		SeparatedToOneway(int road_type);

int		FindBridgeRule(int entity_type, double len, double smallest_seg, double biggest_seg, int num_segments, double curve_dot, double agl1, double agl2);

#endif