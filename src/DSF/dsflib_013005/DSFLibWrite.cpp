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
#include "DSFLib.h"
#include "XChunkyFileUtils.h"
#include <stdio.h>
#if LIN
#define PROTOTYPES 1
#include <sys/types.h> /* to get caddr_t for md5 */
#include "md5global.h"
#endif
#include "md5.h"
#include "DSFDefs.h"
#include "DSFPointPool.h"

#if LIN
#include <set>
#include <algo.h>
#endif

// Define this to 1 to see statistics about the encoded DSF file.
#define ENCODING_STATS 1

#if APL
	#include <Endian.h>
	#define SWAP32(x) (Endian32_Swap(x))
	#define SWAP16(x) (Endian16_Swap(x))
#else
	#define SWAP32(x) (x)
	#define SWAP16(x) (x)
#endif

static	void	DSFSignMD5(const char * inPath)
{
	unsigned char buf[1024];
#if LIN
	unsigned char digest[16];
#endif
	FILE * fi = fopen(inPath, "rb");
	if (fi == NULL) return;
	MD5_CTX ctx;
	MD5Init(&ctx);

	while (1)
	{
		size_t c = fread(buf, 1, sizeof(buf), fi);
		if (c == 0) break;
		MD5Update(&ctx, buf, c);
	}
#if LIN
	MD5Final(digest,&ctx);
#else
	MD5Final(&ctx);
#endif
	fclose(fi);
	fi = fopen(inPath, "ab");
#if LIN
	fwrite(digest, 1, 16, fi);
#else
	fwrite(ctx.digest, 1, 16, fi);
#endif
	fclose(fi);
}

void	ErasePair(multimap<int, int>& ioMap, int key, int value)
{
	typedef multimap<int,int>::iterator iterator;
	pair<iterator, iterator> range = ioMap.equal_range(key);
	for (iterator i = range.first; i != range.second; ++i)
	{
		if (i->second == value)
		{
			ioMap.erase(i);
			return;
		}
	}
	printf("ERROR: BAD KEY VALUE PAIR\n");
}

void	WriteStringTable(FILE * fi, const vector<string>& v)
{
	for (int n = 0; n < v.size(); ++n)
	{
		fwrite(v[n].c_str(), 1, v[n].size() + 1, fi);
	}
}

void	UpdatePoolState(FILE * fi, int newType, int newPool, int& curType, int& curPool)
{
	if (newPool > 10000 || newPool < 0)
		printf("WARNING: set pool to %d\n", newPool);
	if (newType != curType)
	{
		curType = newType;
		if (curType > 65535)
		{
			WriteUInt8(fi, dsf_Cmd_SetDefinition32);
			WriteUInt32(fi, curType);
		} else if (curType > 255)
		{
			WriteUInt8(fi, dsf_Cmd_SetDefinition16);
			WriteUInt16(fi, (unsigned short) curType);
		} else {
			WriteUInt8(fi, dsf_Cmd_SetDefinition8);
			WriteUInt8(fi, (unsigned char) curType);
		}
	}
	if (curPool != newPool)
	{
		curPool = newPool;
		WriteUInt8(fi, dsf_Cmd_PoolSelect);
		WriteUInt16(fi, (unsigned short) curPool);
	}
}

#define REF(x) ((DSFFileWriterImp *) (x))

class	DSFFileWriterImp {
public:

	/********** DEF STORAGE **********/

	int		mDivisions;
	double	mNorth;
	double	mSouth;
	double	mEast;
	double	mWest;

	vector<string>		terrainDefs;
	vector<string>		objectDefs;
	vector<string>		polygonDefs;
	vector<string>		networkDefs;
	vector<string>		properties;

	/********** OBJECT STORAGE **********/
	DSFContiguousPointPool	objectPool;

	struct	ObjectSpec {
		int						type;
		int						pool;
		int						location;
		bool	operator<(const ObjectSpec& rhs) const {
			if (type < rhs.type) return true; 		if (type > rhs.type) return false;
			if (pool < rhs.pool) return true; 		if (pool > rhs.pool) return false;
			return location < rhs.location; }
	};
	typedef vector<ObjectSpec>	ObjectSpecVector;
	ObjectSpecVector			objects;

	/********** POLYGON STORAGE **********/
	DSFContiguousPointPool	polygonPool;

	struct PolygonSpec {
		int					type;
		int					pool;
		int					param;
		vector<int>			intervals;	// All but first are inclusive ends of ranges.
		bool	operator<(const PolygonSpec& rhs) const {
			if (type < rhs.type) return true;		if (type > rhs.type) return false;
			if (pool < rhs.pool) return true;		if (pool > rhs.pool) return false;
			return param < rhs.param; }
	};
	typedef	vector<PolygonSpec>		PolygonSpecVector;
	PolygonSpecVector				polygons;

	PolygonSpec *				accum_poly;
	DSFTupleVectorVector		accum_poly_winding;

	/********** TERRAIN STORAGE **********/
	typedef map<int, DSFSharedPointPool>	DSFSharedPointPoolMap;
	DSFSharedPointPoolMap					terrainPool;

	struct	TriPrimitive {
		int						type;
		bool					is_range;
		bool					is_cross_pool;
		DSFTupleVector			vertices;
		DSFPointPoolLocVector	indices;
	};
	typedef vector<TriPrimitive>	TriPrimitiveVector;

	struct PatchSpec {
		double					nearLOD;
		double					farLOD;
		int						type;
		unsigned char			flags;
		int						depth;
		TriPrimitiveVector		primitives;

		bool	operator<(const PatchSpec& rhs) const {
			if (nearLOD < rhs.nearLOD) return true;			if (nearLOD > rhs.nearLOD) return false;
			if (farLOD < rhs.farLOD) return true;			if (farLOD > rhs.farLOD) return false;
			if (type < rhs.type) return true;				if (type > rhs.type) return false;
			if (depth < rhs.depth) return true;				if (depth > rhs.depth) return false;
			if (flags < rhs.flags) return true;				if (flags > rhs.flags) return false;
			return primitives.size() < rhs.primitives.size(); }
	};
	typedef	vector<PatchSpec>		PatchSpecVector;
	PatchSpecVector					patches;

	PatchSpec *					accum_patch;
	DSFSharedPointPool *		accum_patch_pool;
	TriPrimitive *				accum_primitive;

	/********** VECTOR STORAGE **********/
	DSF32BitPointPool	vectorPool;
	DSF32BitPointPool	vectorPoolCurved;

	struct	ChainSpec {								// The chain spec contains a single complete
		int						startNode;			// chain as passed into us.  We use this  while
		int						endNode;			// accumulating data.
		int						type;
		int						subType;
		bool					curved;
		bool					contiguous;
		DSFTupleVector			path;
		DSFPointPoolLocVector	indices;
		int						lowest_index;
		int						highest_index;
		bool operator<(const ChainSpec& rhs) const {
			if (curved != rhs.curved)	return curved;
			if (type < rhs.type) return true;					if (type > rhs.type) return false;
			if (subType < rhs.subType) return true;				if (subType > rhs.subType) return false;
			if (lowest_index < rhs.lowest_index) return true;	if (lowest_index > rhs.lowest_index) return false;
			return highest_index < rhs.highest_index;
		}
	};
	typedef vector<ChainSpec> 	ChainSpecVector;
	typedef	multimap<int, int>	ChainSpecIndex;
	ChainSpecVector				chainSpecs;
	ChainSpecIndex				chainSpecsIndex;
	ChainSpec *					accum_chain;

	struct	SortChainByLength {
		bool operator()(const ChainSpec& lhs, const ChainSpec& rhs) const {
			return lhs.path.size() > rhs.path.size(); } };


	DSFFileWriterImp(double inWest, double inSouth, double inEast, double inNorth, int divisions);
	void WriteToFile(const char * inPath);

	// DATA ACCUMULATORS

	static void	AcceptTerrainDef(const char * inPartialPath, void * inRef);
	static void	AcceptObjectDef(const char * inPartialPath, void * inRef);
	static void	AcceptPolygonDef(const char * inPartialPath, void * inRef);
	static void	AcceptNetworkDef(const char * inPartialPath, void * inRef);
	static void AcceptProperty(const char * inProp, const char * inValue, void * inRef);

	static void BeginPatch(
					unsigned int	inTerrainType,
					double 			inNearLOD,
					double 			inFarLOD,
					unsigned char	inFlags,
					int				inCoordDepth,
					void *			inRef);
	static void BeginPrimitive(
					int				inType,
					void *			inRef);
	static void AddPatchVertex(
					double			inCoordinate[],
					void *			inRef);
	static void EndPrimitive(
					void *			inRef);
	static void EndPatch(
					void *			inRef);

	static void	AddObject(
					unsigned int	inObjectType,
					double			inCoordinates[2],
					double			inRotation,
					void *			inRef);

	static void BeginSegment(
					unsigned int	inNetworkType,
					unsigned int	inNetworkSubtype,
					unsigned int	inStartNodeID,
					double			inCoordinates[6],
					bool			inCurved,
					void *			inRef);
	static void	AddSegmentShapePoint(
					double			inCoordinates[6],
					bool			inCurved,
					void *			inRef);
	static void EndSegment(
					unsigned int	inEndNodeID,
					double			inCoordinates[6],
					bool			inCurved,
					void *			inRef);

	static void BeginPolygon(
					unsigned int	inPolygonType,
					unsigned short	inParam,
					void *			inRef);
	static void BeginPolygonWinding(
					void *			inRef);
	static void AddPolygonPoint(
					double			inCoordinates[2],
					void *			inRef);
	static void EndPolygonWinding(
					void *			inRef);
	static void EndPolygon(
					void *			inRef);

	TriPrimitiveVector::iterator DisassemblePrimitive(
									TriPrimitiveVector& vec,
									TriPrimitiveVector::iterator prim);

};





void *	DSFCreateWriter(double inWest, double inSouth, double inEast, double inNorth, int divisions)
{
	DSFFileWriterImp * imp = new DSFFileWriterImp(inWest, inSouth, inEast, inNorth, divisions);
	return imp;
}



void	DSFDestroyWriter(void * inRef)
{
	DSFFileWriterImp * imp = (DSFFileWriterImp *) inRef;
	delete imp;
}
void	DSFGetWriterCallbacks(DSFCallbacks_t * ioCallbacks)
{
	ioCallbacks->AcceptTerrainDef_f = DSFFileWriterImp::AcceptTerrainDef;
	ioCallbacks->AcceptObjectDef_f = DSFFileWriterImp::AcceptObjectDef;
	ioCallbacks->AcceptPolygonDef_f = DSFFileWriterImp::AcceptPolygonDef;
	ioCallbacks->AcceptNetworkDef_f = DSFFileWriterImp::AcceptNetworkDef;
	ioCallbacks->AcceptProperty_f = DSFFileWriterImp::AcceptProperty;
	ioCallbacks->BeginPatch_f = DSFFileWriterImp::BeginPatch;
	ioCallbacks->BeginPrimitive_f = DSFFileWriterImp::BeginPrimitive;
	ioCallbacks->AddPatchVertex_f = DSFFileWriterImp::AddPatchVertex;
	ioCallbacks->EndPrimitive_f = DSFFileWriterImp::EndPrimitive;
	ioCallbacks->EndPatch_f = DSFFileWriterImp::EndPatch;
	ioCallbacks->AddObject_f = DSFFileWriterImp::AddObject;
	ioCallbacks->BeginSegment_f = DSFFileWriterImp::BeginSegment;
	ioCallbacks->AddSegmentShapePoint_f = DSFFileWriterImp::AddSegmentShapePoint;
	ioCallbacks->EndSegment_f = DSFFileWriterImp::EndSegment;
	ioCallbacks->BeginPolygon_f = DSFFileWriterImp::BeginPolygon;
	ioCallbacks->BeginPolygonWinding_f = DSFFileWriterImp::BeginPolygonWinding;
	ioCallbacks->AddPolygonPoint_f = DSFFileWriterImp::AddPolygonPoint;
	ioCallbacks->EndPolygonWinding_f = DSFFileWriterImp::EndPolygonWinding;
	ioCallbacks->EndPolygon_f = DSFFileWriterImp::EndPolygon;
}

void	DSFWriteToFile(const char * inPath, void * inRef)
{
	((DSFFileWriterImp *)	inRef)->WriteToFile(inPath);
}

DSFFileWriterImp::DSFFileWriterImp(double inWest, double inSouth, double inEast, double inNorth, int divisions)
{
	mDivisions = divisions;
	mNorth = inNorth;
	mSouth = inSouth;
	mEast = inEast;
	mWest = inWest;

	// BUILD VECTOR POOLS
	DSFTuple	vecRangeMin, vecRangeMax;
	vecRangeMin.push_back(inWest);
	vecRangeMin.push_back(inSouth);
	vecRangeMin.push_back(-32768.0);
	vecRangeMin.push_back(0.0);
	vecRangeMax.push_back(inEast);
	vecRangeMax.push_back(inNorth);
	vecRangeMax.push_back(32767.0);
	vecRangeMax.push_back(0.0);
	DSFTuple	vecRangeCurveMin(vecRangeMin), vecRangeCurveMax(vecRangeMax);
	vecRangeCurveMin.push_back(inWest);
	vecRangeCurveMin.push_back(inSouth);
	vecRangeCurveMin.push_back(-32768.0);
	vecRangeCurveMax.push_back(inEast);
	vecRangeCurveMax.push_back(inNorth);
	vecRangeCurveMax.push_back(32767.0);

	vectorPool.SetRange(vecRangeMin, vecRangeMax);
	vectorPoolCurved.SetRange(vecRangeCurveMin, vecRangeCurveMax);

	// BUILD OBJECT POINT POOLS

	DSFTuple	objRangeMin, objRangeMax;
	objRangeMin.push_back(inWest);
	objRangeMin.push_back(inSouth);
	objRangeMin.push_back(0.0);
	objRangeMax.push_back(inEast);
	objRangeMax.push_back(inNorth);
	objRangeMax.push_back(360.0);

	objectPool.SetRange(objRangeMin, objRangeMax);
	for (int i = 0; i < divisions; ++i)
	for (int j = 0; j < divisions; ++j)
	{
		DSFTuple	fracMin, fracMax;
		fracMin.push_back((double) i / double (divisions));
		fracMin.push_back((double) j / double (divisions));
		fracMin.push_back(0.0);
		fracMax.push_back((double) (i+1) / double (divisions));
		fracMax.push_back((double) (j+1) / double (divisions));
		fracMax.push_back(1.0);
		objectPool.AddPool(fracMin, fracMax);
		objectPool.AddPool(fracMin, fracMax);
	}

	// BUILD POLYGON POINT POOLS
	DSFTuple	polyRangeMin, polyRangeMax;
	polyRangeMin.push_back(inWest);
	polyRangeMin.push_back(inSouth);
	polyRangeMax.push_back(inEast);
	polyRangeMax.push_back(inNorth);

	polygonPool.SetRange(polyRangeMin, polyRangeMax);
	for (int i = 0; i < divisions; ++i)
	for (int j = 0; j < divisions; ++j)
	{
		DSFTuple	fracMin, fracMax;
		fracMin.push_back((double) i / double (divisions));
		fracMin.push_back((double) j / double (divisions));
		fracMax.push_back((double) (i+1) / double (divisions));
		fracMax.push_back((double) (j+1) / double (divisions));
		polygonPool.AddPool(fracMin, fracMax);
	}
	for (int i = 1; i < divisions; ++i)
	for (int j = 1; j < divisions; ++j)
	{
		DSFTuple	fracMin, fracMax;
		fracMin.push_back(((double) i - 0.5) / double (divisions));
		fracMin.push_back(((double) j - 0.5) / double (divisions));
		fracMax.push_back(((double) i+0.5) / double (divisions));
		fracMax.push_back(((double) j+0.5) / double (divisions));
		polygonPool.AddPool(fracMin, fracMax);
	}
	DSFTuple	fracMin, fracMax;
	fracMin.push_back(0.0);		fracMax.push_back(1.0);
	fracMin.push_back(0.0);		fracMax.push_back(1.0);
	polygonPool.AddPool(fracMin, fracMax);

	// POINT POOL TERRAINS ARE DRAWN ON THE FLY
}

// Given a triangle primitive, this routine breaks the primitive
// into individual triangles.
DSFFileWriterImp::TriPrimitiveVector::iterator DSFFileWriterImp::DisassemblePrimitive(
									TriPrimitiveVector& vec,
									TriPrimitiveVector::iterator prim)
{
	TriPrimitiveVector	new_prims;
	switch(prim->type) {
	case dsf_Tri:
		for (int n = 0; n < prim->vertices.size(); n += 3)
		{
			new_prims.push_back(DSFFileWriterImp::TriPrimitive());
			new_prims.back().type = dsf_Tri;
			new_prims.back().vertices.push_back(prim->vertices[n  ]);
			new_prims.back().vertices.push_back(prim->vertices[n+1]);
			new_prims.back().vertices.push_back(prim->vertices[n+2]);
		}
		break;
	case dsf_TriStrip:
		for (int n = 2; n < prim->vertices.size(); n += 1)
		{
			new_prims.push_back(DSFFileWriterImp::TriPrimitive());
			new_prims.back().type = dsf_Tri;
			if (n%2) {
				new_prims.back().vertices.push_back(prim->vertices[n-2]);
				new_prims.back().vertices.push_back(prim->vertices[n  ]);
				new_prims.back().vertices.push_back(prim->vertices[n-1]);
			} else {
				new_prims.back().vertices.push_back(prim->vertices[n-2]);
				new_prims.back().vertices.push_back(prim->vertices[n-1]);
				new_prims.back().vertices.push_back(prim->vertices[n  ]);
			}
		}
		break;
	case dsf_TriFan:
		for (int n = 2; n < prim->vertices.size(); n += 1)
		{
			new_prims.push_back(DSFFileWriterImp::TriPrimitive());
			new_prims.back().type = dsf_Tri;
			new_prims.back().vertices.push_back(prim->vertices[0  ]);
			new_prims.back().vertices.push_back(prim->vertices[n-1]);
			new_prims.back().vertices.push_back(prim->vertices[n  ]);
		}
		break;
	}

	std::size_t pp = distance(vec.begin(), prim);
	prim = vec.erase(prim);
	vec.insert(prim, new_prims.begin(), new_prims.end());
	prim = vec.begin();
	advance(prim, pp + new_prims.size() - 1);
	return prim;
}

void DSFFileWriterImp::WriteToFile(const char * inPath)
{
	int n, i, p;
	pair<int, int> loc;

	/******************** PREPROCESS PATCHES **************************/

	// For each given plane depth, work up all of our primitives.
	typedef vector<TriPrimitive *>						TPV;
	typedef map<int, TPV>								TPVM;	//
	typedef	map<int, int>								TPDOM;	// Terrain pool depth offset map

	PatchSpecVector::iterator			patchSpec;
	PolygonSpecVector::iterator			polySpec;
	TriPrimitiveVector::iterator		primIter;
	TPVM::iterator 						prims;
	TPV::iterator						prim;
	DSFPointPoolLocVector::iterator 	v;
	ObjectSpecVector::iterator			objSpec;
	ChainSpecIndex::iterator			csIndex;
#if 0
	// NOTE: WE NEED TO DEAL WITH BREAKING APART TRISTRIPS
	// AND TRIS AND TRIFANS!

	for (patchSpec = patches.begin(); patchSpec != patches.end(); ++patchSpec)
	{
		for (primIter = patch->primitives.begin(); primIter != patch->primitives.end(); ++primIter)
		{
			if ((*primIter).type == dsf_Tri &&
				!terrainPool[patch->depth].
							CanBeContiguous((*primIter).vertices))
			{
				// TODO HERE: BREAK UP TRIANGLES THAT SPAN!
				primIter = DisassemblePrimitive(patch->primitives, prim);
			}
		}
	}
#endif

	// Build up a list of all primitives, sorted by depth
	TPVM	all_primitives;
	for (patchSpec = patches.begin(); patchSpec != patches.end(); ++patchSpec)
	for (primIter = patchSpec->primitives.begin(); primIter != patchSpec->primitives.end(); ++primIter)
	{
		primIter->is_range = false;
		all_primitives[patchSpec->depth].push_back(&*primIter);
	}

#if ENCODING_STATS
	int total_prim_v_contig = 0;
	int	total_prim_v_shared = 0;
#endif

	// Sort these lists by size, and try to sink any non-shared primitive.
	for (prims = all_primitives.begin(); prims != all_primitives.end(); ++prims)
	{
		sort(prims->second.begin(), prims->second.end());

		for (prim = prims->second.begin(); prim != prims->second.end(); ++prim)
		{
			if (terrainPool[prims->first].CanBeContiguous((*prim)->vertices))
			{
				loc = terrainPool[prims->first].AcceptContiguous((*prim)->vertices);
				if (loc.first != -1 && loc.second != -1)
				{
#if ENCODING_STATS
					total_prim_v_contig += (*prim)->vertices.size();
#endif
					(*prim)->is_range = true;
					for (n = 0; n < (*prim)->vertices.size(); ++n)
						(*prim)->indices.push_back(DSFPointPoolLoc(loc.first, loc.second + n));
				}
			}
		}
	}

	// Now sink remaining vertices individually.
	for (prims = all_primitives.begin(); prims != all_primitives.end(); ++prims)
	for (prim = prims->second.begin(); prim != prims->second.end(); ++prim)
	if ((*prim)->indices.empty())
	for (n = 0; n < (*prim)->vertices.size(); ++n)
	{
		loc = terrainPool[prims->first].AcceptShared((*prim)->vertices[n]);
		if (loc.first == -1 || loc.second == -1)
		{
			printf("ERROR: could not sink vertex:\n");
			(*prim)->vertices[n].dump();
			printf("\n");
			(*prim)->vertices[n].dumphex();
			printf("\n");
		}
		(*prim)->indices.push_back(loc);
#if ENCODING_STATS
		++total_prim_v_shared;
#endif
	}

#if ENCODING_STATS
	printf("Contiguous vertices: %d.  Individual vertices: %d\n", total_prim_v_contig, total_prim_v_shared);
#endif

	// Compact final pool data.
	for(DSFSharedPointPoolMap::iterator pool = terrainPool.begin(); pool != terrainPool.end(); ++pool)
		pool->second.ProcessPoints();

	for (patchSpec = patches.begin(); patchSpec != patches.end(); ++patchSpec)
	for (primIter = patchSpec->primitives.begin(); primIter != patchSpec->primitives.end(); ++primIter)
	for (v = primIter->indices.begin(); v != primIter->indices.end(); ++v)
		v->first = terrainPool[patchSpec->depth].MapPoolNumber(v->first);

	/******************** PREPROCESS OBJECTS **************************/

	sort(objects.begin(),	objects.end());

	objectPool.ProcessPoints();
	for (objSpec = objects.begin(); objSpec != objects.end(); ++objSpec)
		objSpec->pool = objectPool.MapPoolNumber(objSpec->pool);

	/******************** PREPROCESS POLYGONS **************************/
	sort(polygons.begin(),	polygons.end());

	polygonPool.ProcessPoints();
	for (polySpec = polygons.begin(); polySpec != polygons.end(); ++polySpec)
		polySpec->pool = polygonPool.MapPoolNumber(polySpec->pool);

	/******************** PREPROCESS VECTORS **************************/

	// First we sort chains, biggest to smallest, and index them.
	sort(chainSpecs.begin(), chainSpecs.end(), SortChainByLength());
	for (n = 0; n < chainSpecs.size(); ++n)
	{
		chainSpecsIndex.insert(ChainSpecIndex::value_type(chainSpecs[n].startNode, n));
		chainSpecsIndex.insert(ChainSpecIndex::value_type(chainSpecs[n].endNode, n));
	}

	// Now try to build up the longest chains possible.
	//	Build up arrays that contain coordinates & junction IDs in runs.
	//		work through the length-sorted vector, building up the longest possible type-consistent chains,
	//		removing them from the index.

	bool	did_merge;
	do {
		did_merge = false;
		for (n = 0; n < chainSpecs.size(); ++n)
		{
			if (chainSpecs[n].path.empty()) continue;

			// TRY TO MATCH OUR FRONT
			int	best = -1;
			int	bestlen = 0;
			pair<ChainSpecIndex::iterator,ChainSpecIndex::iterator> range = chainSpecsIndex.equal_range(chainSpecs[n].startNode);
			for (csIndex = range.first; csIndex != range.second; ++csIndex)
			{
				if (n != csIndex->second &&
					chainSpecs[n].type == chainSpecs[csIndex->second].type &&
					chainSpecs[n].subType == chainSpecs[csIndex->second].subType &&
					chainSpecs[n].curved == chainSpecs[csIndex->second].curved &&
					(chainSpecs[n].path.size() + chainSpecs[csIndex->second].path.size()) < 255)
				{
					if (chainSpecs[csIndex->second].path.size() > bestlen)
					{
						best = csIndex->second;
						bestlen = chainSpecs[csIndex->second].path.size();
					}
				}
			}
			if (best != -1)
			{
				int shared_node = chainSpecs[n].startNode;
				if (chainSpecs[n].startNode == chainSpecs[best].startNode)
				{
					// Merge - reverse other guy, snip him onto us
					chainSpecs[n].startNode = chainSpecs[best].endNode;
					int l = chainSpecs[best].path.size();
					for (p = 0; p < ( l / 2); ++p)
						swap(chainSpecs[best].path[p], chainSpecs[best].path[l-p-1]);
					chainSpecs[n].path.erase(chainSpecs[n].path.begin());
					chainSpecs[n].path.insert(chainSpecs[n].path.begin(), chainSpecs[best].path.begin(), chainSpecs[best].path.end());
					chainSpecs[best].path.clear();
				} else {
					chainSpecs[n].startNode = chainSpecs[best].startNode;
					chainSpecs[n].path.erase(chainSpecs[n].path.begin());
					chainSpecs[n].path.insert(chainSpecs[n].path.begin(),
						chainSpecs[best].path.begin(), chainSpecs[best].path.end());
					chainSpecs[best].path.clear();
				}
				ErasePair(chainSpecsIndex, shared_node, n);
				ErasePair(chainSpecsIndex, chainSpecs[best].endNode, best);
				ErasePair(chainSpecsIndex, chainSpecs[best].startNode, best);
				chainSpecsIndex.insert(ChainSpecIndex::value_type(chainSpecs[n].startNode, n));
				did_merge = true;
			}


			// TRY TO MATCH OUR BACK

			best = -1;
			bestlen = 0;
			range = chainSpecsIndex.equal_range(chainSpecs[n].endNode);
			for (csIndex = range.first; csIndex != range.second; ++csIndex)
			{
				if (n != csIndex->second &&
					chainSpecs[n].type == chainSpecs[csIndex->second].type &&
					chainSpecs[n].subType == chainSpecs[csIndex->second].subType &&
					chainSpecs[n].curved == chainSpecs[csIndex->second].curved &&
					(chainSpecs[n].path.size() + chainSpecs[csIndex->second].path.size()) < 255)
				{
					if (chainSpecs[csIndex->second].path.size() > bestlen)
					{
						best = csIndex->second;
						bestlen = chainSpecs[csIndex->second].path.size();
					}
				}
			}
			if (best != -1)
			{
				int shared_node = chainSpecs[n].endNode;
				if (chainSpecs[n].endNode == chainSpecs[best].endNode)
				{
					// Merge - reverse other guy, snip him onto us
					chainSpecs[n].endNode = chainSpecs[best].startNode;
					int l = chainSpecs[best].path.size();
					for (p = 0; p < ( l / 2); ++p)
						swap(chainSpecs[best].path[p], chainSpecs[best].path[l-p-1]);
					chainSpecs[n].path.pop_back();
					chainSpecs[n].path.insert(chainSpecs[n].path.end(), chainSpecs[best].path.begin(), chainSpecs[best].path.end());
					chainSpecs[best].path.clear();
				} else {
					chainSpecs[n].endNode = chainSpecs[best].endNode;
					chainSpecs[n].path.pop_back();
					chainSpecs[n].path.insert(chainSpecs[n].path.end(), chainSpecs[best].path.begin(), chainSpecs[best].path.end());
					chainSpecs[best].path.clear();
				}
				ErasePair(chainSpecsIndex, shared_node, n);
				ErasePair(chainSpecsIndex, chainSpecs[best].endNode, best);
				ErasePair(chainSpecsIndex, chainSpecs[best].startNode, best);
				chainSpecsIndex.insert(ChainSpecIndex::value_type(chainSpecs[n].endNode, n));
				did_merge = true;
			}

		}
	} while (did_merge);

	// We now have each chain lengthened as far as we think it can go.  Now it's time to
	// assign points to the pools.  Delete unused chains and resort.

	// BAS NOTE: deleting chains is EXPENSIVE - we have to move the chains back,
	// and the points in the chains are COPIED not refernece counted, so this is
	// a case where the STL SCREWS US.
	// So just don't.  Sort, and then skip 0-lengt chains.
//	ChainSpecVector	temp;
//	for (ChainSpecVector::iterator i = chainSpecs.begin(); i != chainSpecs.end(); )
//		if (!i->path.empty())
//			temp.push_back(*i);
//	chainSpecs = temp;

	sort(chainSpecs.begin(), chainSpecs.end(), SortChainByLength());

	// Go through and add each chain to the right pool.
	for (n = 0; n < chainSpecs.size(); ++n)
	if(!chainSpecs[n].path.empty())
	{
		DSF32BitPointPool& targetPool = chainSpecs[n].curved ? vectorPoolCurved : vectorPool;
		int	sharedLen = targetPool.CountShared(chainSpecs[n].path);
		int planes = chainSpecs[n].curved ? 7 : 4;
		int chainLen = chainSpecs[n].path.size();
		if ((2 + chainLen * planes) > (chainLen + (chainLen - sharedLen) * planes))
		{
			// Sink into the most shared pool
			chainSpecs[n].contiguous = false;
			for (i = 0; i < chainSpecs[n].path.size(); ++i)
			{
				DSFPointPoolLoc	loc = 	targetPool.AcceptShared(chainSpecs[n].path[i]);
				if (loc.first == -1 || loc.second == -1)
					printf("ERROR: Could not sink chain.\n");
				chainSpecs[n].indices.push_back(loc);
				if (i == 0) {
					chainSpecs[n].lowest_index = loc.second;
					chainSpecs[n].highest_index = loc.second;
				} else {
					chainSpecs[n].lowest_index = min(loc.second, chainSpecs[n].lowest_index);
					chainSpecs[n].highest_index = max(loc.second, chainSpecs[n].highest_index);
				}
			}
		} else {
			// Sink into the least shared pool, but contiguous.
			DSFPointPoolLoc	loc = 	targetPool.AcceptContiguous(chainSpecs[n].path);
			if (loc.first == -1 || loc.second == -1)
				printf("ERROR: Could not sink chain.\n");
			chainSpecs[n].contiguous = true;
			chainSpecs[n].lowest_index = loc.second;
			chainSpecs[n].highest_index = loc.second + chainSpecs[n].path.size();
			for (i = 0; i < chainSpecs[n].path.size(); ++i)
				chainSpecs[n].indices.push_back(DSFPointPoolLoc(loc.first, loc.second + i));
		}
	}

	/******************** WRITE HEADER **************************/

	FILE * fi = fopen(inPath, "wb");
	DSFHeader_t header;
	memcpy(header.cookie, DSF_COOKIE, sizeof(header.cookie));
	header.version = SWAP32(DSF_MASTER_VERSION);
	fwrite(&header, 1, sizeof(header), fi);

	/******************** WRITE DEFINITION AND HEADER **************************/

	{
		StAtomWriter	writeHead(fi, dsf_MetaDataAtom);
		{
			StAtomWriter	writeProp(fi, dsf_PropertyAtom);
			WriteStringTable(fi, properties);
		}
	}

	{
		StAtomWriter	writeDefn(fi, dsf_DefinitionsAtom);
		{
			StAtomWriter	writeTert(fi, dsf_TerrainTypesAtom);
			WriteStringTable(fi, terrainDefs);
		}
		{
			StAtomWriter	writeObjt(fi, dsf_ObjectsAtom);
			WriteStringTable(fi, objectDefs);
		}
		{
			StAtomWriter	writePoly(fi, dsf_PolygonAtom);
			WriteStringTable(fi, polygonDefs);
		}
		{
			StAtomWriter	writeNetw(fi, dsf_NetworkAtom);
			WriteStringTable(fi, networkDefs);
		}
	}

	/******************** WRITE POOLS AND GEODATA **************************/


	int		offset_to_terrain_pools = 0;
	int		offset_to_poly_pools = 0;
	int		last_pool_offset = 0;
	TPDOM	offset_to_terrain_pool_of_depth;

	{
		StAtomWriter	writeGeod(fi, dsf_GeoDataAtom);

		offset_to_terrain_pools = objectPool.WritePoolAtoms (fi, def_PointPoolAtom);
								  objectPool.WriteScaleAtoms(fi, def_PointScaleAtom);

		offset_to_poly_pools = offset_to_terrain_pools;
		for (DSFSharedPointPoolMap::iterator sp = terrainPool.begin(); sp != terrainPool.end(); ++sp)
		{
			offset_to_terrain_pool_of_depth.insert(map<int,int>::value_type(sp->first, offset_to_poly_pools));
			offset_to_poly_pools += sp->second.WritePoolAtoms (fi, def_PointPoolAtom);
									sp->second.WriteScaleAtoms(fi, def_PointScaleAtom);
		}

		last_pool_offset = offset_to_poly_pools + polygonPool.WritePoolAtoms (fi, def_PointPoolAtom);
														polygonPool.WriteScaleAtoms(fi, def_PointScaleAtom);

		vectorPool.WritePoolAtoms (fi, def_PointPool32Atom);
		vectorPool.WriteScaleAtoms(fi, def_PointScale32Atom);
		vectorPoolCurved.WritePoolAtoms (fi, def_PointPool32Atom);
		vectorPoolCurved.WriteScaleAtoms(fi, def_PointScale32Atom);
	}

	for (TPDOM::iterator i = offset_to_terrain_pool_of_depth.begin(); i != offset_to_terrain_pool_of_depth.end(); ++i)
		printf("Terrain pool depth %d starts at %d\n", i->first, i->second);
	printf("Polygon pools start at %d\n", offset_to_poly_pools);
	printf("next pool would be at %d\n", last_pool_offset);


	/******************** WRITE OBJECTS **************************/

	int	curPool = -1;
	int	juncOff = 0;
	int curDef = -1;
	int	curSubDef = -1;
	double	lastLODNear = -1.0;
	double	lastLODFar = -1.0;
	unsigned char lastFlags = 0xFF;

	{
		StAtomWriter	writeCmds(fi, dsf_CommandsAtom);

		WriteUInt8(fi, dsf_Cmd_JunctionOffsetSelect);
		WriteUInt32(fi, 0);

		for (objSpec = objects.begin(); objSpec != objects.end(); ++objSpec)
		{
			int first_loc = objSpec->location, last_loc = objSpec->location;
			ObjectSpecVector::iterator objSpecNext = objSpec;
			while (objSpecNext != objects.end() && objSpec->pool == objSpecNext->pool && objSpec->type == objSpecNext->type)
				last_loc = objSpecNext->location, ++objSpecNext;

			UpdatePoolState(fi, objSpec->type, objSpec->pool, curDef, curPool);
			if (first_loc != last_loc)
			{
				WriteUInt8(fi, dsf_Cmd_Object);
				if (first_loc > 65535) 	fprintf(stderr, "Overflow writing objects (indexed object).\n");
				WriteUInt16(fi, first_loc);
			} else {
				WriteUInt8(fi, dsf_Cmd_ObjectRange);
				if (first_loc > 65535) 	fprintf(stderr, "Overflow writing objects (first loc of range).\n");
				if (last_loc > 65534) 	fprintf(stderr, "Overflow writing objects (last loc of range).\n");
				WriteUInt16(fi, first_loc);
				WriteUInt16(fi, last_loc+1);
			}
		}

	/******************** WRITE POLYGONS **************************/
		for (polySpec = polygons.begin(); polySpec != polygons.end(); ++polySpec)
		{
			UpdatePoolState(fi, polySpec->type, polySpec->pool + offset_to_poly_pools, curDef, curPool);
			if (polySpec->intervals.size() <= 2)
			{
				WriteUInt8(fi, dsf_Cmd_PolygonRange);
				WriteUInt16(fi, polySpec->param);
				if (polySpec->intervals[0] > 65535) fprintf(stderr, "ERROR: polygon range start too large.\n");
				if (polySpec->intervals[1] > 65535) fprintf(stderr, "ERROR: polygon range end too large.\n");
				WriteUInt16(fi, polySpec->intervals[0]);
				WriteUInt16(fi, polySpec->intervals[1]);
			} else {
				WriteUInt8(fi, dsf_Cmd_NestedPolygonRange);
				WriteUInt16(fi, polySpec->param);
				WriteUInt8(fi, polySpec->intervals.size() - 1);
				for (i = 0; i < polySpec->intervals.size(); ++i)
				{
					if (polySpec->intervals[i] > 65535) fprintf(stderr, "ERROR: polygon index out of range.\n");
					WriteUInt16(fi, polySpec->intervals[i]);
				}
			}
		}

	/******************** WRITE PATCHES **************************/

#if ENCODING_STATS
		int total_prim_p_crosspool = 0, total_prim_p_range = 0, total_prim_p_individual = 0;
#endif

		for (patchSpec = patches.begin(); patchSpec != patches.end(); ++patchSpec)
		{
			// Prep step - build up a list of pools referenced and also
			// mark pools as cross-pool or not.
			set<int>	pools;
			for (primIter = patchSpec->primitives.begin(); primIter != patchSpec->primitives.end(); ++primIter)
			{
				if (primIter->vertices.empty()) continue;
				if (primIter->is_range)
				{
					pools.insert(primIter->indices.begin()->first);
					primIter->is_cross_pool = false;
				} else {
					int first_val = primIter->indices.begin()->first;
					primIter->is_cross_pool = false;
					for (n = 0; n < primIter->indices.size(); ++n)
					{
						if (primIter->indices[n].first != first_val)
							primIter->is_cross_pool = true;
						pools.insert(primIter->indices[n].first);
					}
				}
			}

			// Here's the basic idea of the patch writer: there are 3 kinds of patche primitives
			// as well as 3 kinds of enocding (range, pool, and x-pool).  So we're basically going
			// to make nine passes through the data looking for primitives that do what we want.

			// Update the polygon type and start the patch.
			UpdatePoolState(fi, patchSpec->type, patchSpec->primitives.front().indices[0].first + offset_to_terrain_pool_of_depth[patchSpec->depth], curDef, curPool);

			if (lastLODNear != patchSpec->nearLOD || lastLODFar != patchSpec->farLOD)
			{
				WriteUInt8(fi, dsf_Cmd_TerrainPatchFlagsLOD);
				WriteUInt8(fi, patchSpec->flags);
				WriteFloat32(fi, patchSpec->nearLOD);
				WriteFloat32(fi, patchSpec->farLOD);
				lastLODNear = patchSpec->nearLOD;
				lastLODFar = patchSpec->farLOD;
				lastFlags = patchSpec->flags;
			} else if (lastFlags != patchSpec->flags) {
				WriteUInt8(fi, dsf_Cmd_TerrainPatchFlags);
				WriteUInt8(fi, patchSpec->flags);
				lastFlags = patchSpec->flags;
			} else {
				WriteUInt8(fi, dsf_Cmd_TerrainPatch);
			}

			// Now handle all primitives within the pool
			for (set<int>::iterator apool = pools.begin(); apool != pools.end(); ++apool)
			for (primIter = patchSpec->primitives.begin(); primIter != patchSpec->primitives.end(); ++primIter)
			if (!primIter->is_cross_pool &&
				primIter->indices[0].first == *apool)
			{
				UpdatePoolState(fi, patchSpec->type, (*apool) + offset_to_terrain_pool_of_depth[patchSpec->depth], curDef, curPool);
				if (primIter->is_range)
				{
#if ENCODING_STATS
					++total_prim_p_range;
#endif
					if (primIter->type == dsf_Tri)			WriteUInt8(fi, dsf_Cmd_TriangleRange);
					if (primIter->type == dsf_TriStrip)		WriteUInt8(fi, dsf_Cmd_TriangleStripRange);
					if (primIter->type == dsf_TriFan)		WriteUInt8(fi, dsf_Cmd_TriangleFanRange);
					if (primIter->indices[0].second > 65535) 							fprintf(stderr, "ERROR: array range primitive offsets out of bounds at beginning.\n");
					if (primIter->indices[0].second + primIter->indices.size() > 65535) fprintf(stderr, "ERROR: array range primitive offsets out of bounds at end.\n");
					WriteUInt16(fi,primIter->indices[0].second);
					WriteUInt16(fi,primIter->indices[0].second + primIter->indices.size());
				} else {
#if ENCODING_STATS
					++total_prim_p_individual;
#endif

					if (primIter->type == dsf_Tri)			WriteUInt8(fi, dsf_Cmd_Triangle);
					if (primIter->type == dsf_TriStrip)		WriteUInt8(fi, dsf_Cmd_TriangleStrip);
					if (primIter->type == dsf_TriFan)		WriteUInt8(fi, dsf_Cmd_TriangleFan);
					if (primIter->indices.size() > 255)
						fprintf(stderr,"WARNING: Overflow on standard tri command, type = %d, %d indices\n", primIter->type, primIter->indices.size());
					WriteUInt8(fi, primIter->indices.size());
					for (n = 0; n < primIter->indices.size(); ++n)
					{
						if (primIter->indices[n].second > 65535) fprintf(stderr, "ERROR: overflow on explicit index for primtive.\n");
						WriteUInt16(fi, primIter->indices[n].second);
					}
				}
			}

			// Now go back and write the cross-pool primitives.
			for (primIter = patchSpec->primitives.begin(); primIter != patchSpec->primitives.end(); ++primIter)
			if (primIter->is_cross_pool)
			{
#if ENCODING_STATS
					++total_prim_p_crosspool;
#endif
				if (primIter->type == dsf_Tri)			WriteUInt8(fi, dsf_Cmd_TriangleCrossPool);
				if (primIter->type == dsf_TriStrip)		WriteUInt8(fi, dsf_Cmd_TriangleStripCrossPool);
				if (primIter->type == dsf_TriFan)		WriteUInt8(fi, dsf_Cmd_TriangleFanCrossPool);
				WriteUInt8(fi, primIter->indices.size());
				for (n = 0; n < primIter->indices.size(); ++n)
				{
					if (primIter->indices[n].first + offset_to_terrain_pool_of_depth[patchSpec->depth] > 65535) fprintf(stderr,"ERROR: Overflow writing range primitive cross pool index, subpool =%d, offset to pool = %d.\n", primIter->indices[n].first, offset_to_terrain_pool_of_depth[patchSpec->depth]);
					if (primIter->indices[n].second > 65535) 													fprintf(stderr,"ERROR: Overflow writing range primitive cross pool offset.\n");
					WriteUInt16(fi, primIter->indices[n].first + offset_to_terrain_pool_of_depth[patchSpec->depth]);
					WriteUInt16(fi, primIter->indices[n].second);
				}
			}
		}

#if ENCODING_STATS
		printf("Total cross-pool primitives: %d.  Total range primitives: %d.  Total enumerated primitives: %d.\n",
			total_prim_p_crosspool,total_prim_p_range, total_prim_p_individual);
#endif


	/******************** WRITE VECTORS **************************/
		sort(chainSpecs.begin(), chainSpecs.end());

		for (ChainSpecVector::iterator chain = chainSpecs.begin(); chain != chainSpecs.end(); ++chain)
		if (!chain->path.empty())
		{
			UpdatePoolState(fi, chain->type, chain->curved ? 1 : 0, curDef, curPool);
			if (chain->subType != curSubDef)
			{
				curSubDef = chain->subType;
				WriteUInt8(fi, dsf_Cmd_SetRoadSubtype8);
				WriteUInt8(fi, (unsigned char) curSubDef);
			}
			// See if we are too wide to run in 16-bits.  If so
			// we need to write 32 bits.
			if ((chain->highest_index - chain->lowest_index) > 65535)
			{
				WriteUInt8(fi, dsf_Cmd_NetworkChain32);
				if (chain->indices.size() > 255)
					fprintf(stderr, "WARNING: overflow on network chain.\n");
				WriteUInt8(fi, chain->indices.size());
				for (n = 0; n < chain->indices.size(); ++n)
					WriteUInt32(fi, chain->indices[n].second);

			} else {
				// We can run in 16 bits.  Update the junction
				// pool as needed.
				if (((juncOff + 65535) < chain->highest_index) || (juncOff > chain->lowest_index))
				{
					juncOff = chain->lowest_index;
					WriteUInt8(fi, dsf_Cmd_JunctionOffsetSelect);
					WriteUInt32(fi, juncOff);
				}

				if (chain->contiguous)
				{
					WriteUInt8(fi, dsf_Cmd_NetworkChainRange);
					WriteUInt16(fi, chain->lowest_index - juncOff);
					WriteUInt16(fi, chain->highest_index - juncOff);
				} else {
					WriteUInt8(fi, dsf_Cmd_NetworkChain);
					if (chain->indices.size() > 255)
						fprintf(stderr, "ERROR: overflow on network chain.\n");

					WriteUInt8(fi, chain->indices.size());
					for (n = 0; n < chain->indices.size(); ++n)
					{
						int	delta = chain->indices[n].second;
						delta = delta - juncOff;
						if (delta < 0	)	fprintf(stderr,"ERROR: Range error writing chain - delta system logic errror.\n");
						if (delta > 65535)	fprintf(stderr,"Range error writing chain - delta system logic errror.\n");
						WriteUInt16(fi, delta);
					}
				}
			}
		}
	}

	/******************** WRITE FOOTER **************************/

//	DSFFooter_t	footer;
//	fwrite(&footer, 1, sizeof(footer), fi);
	fclose(fi);

	DSFSignMD5(inPath);
}


/*
	TODO ON MESH OPTIMIZATION:
	1. DO CONSOLIDATE INDIVIDUAL TRIANGLES AFTER WE PLACE THEM!!

*/


#pragma mark -


void	DSFFileWriterImp::AcceptTerrainDef(const char * inPartialPath, void * inRef)
{
	REF(inRef)->terrainDefs.push_back(inPartialPath);
}
void	DSFFileWriterImp::AcceptObjectDef(const char * inPartialPath, void * inRef)
{
	REF(inRef)->objectDefs.push_back(inPartialPath);
}
void	DSFFileWriterImp::AcceptPolygonDef(const char * inPartialPath, void * inRef)
{
	REF(inRef)->polygonDefs.push_back(inPartialPath);
}
void	DSFFileWriterImp::AcceptNetworkDef(const char * inPartialPath, void * inRef)
{
	REF(inRef)->networkDefs.push_back(inPartialPath);
}
void	DSFFileWriterImp::AcceptProperty(const char * inProp, const char * inValue, void * inRef)
{
	REF(inRef)->properties.push_back(inProp);
	REF(inRef)->properties.push_back(inValue);
}

void 	DSFFileWriterImp::BeginPatch(
				unsigned int	inTerrainType,
				double 			inNearLOD,
				double 			inFarLOD,
				unsigned char	inFlags,
				int				inCoordDepth,
				void *			inRef)
{
	bool	new_pool = REF(inRef)->terrainPool.find(inCoordDepth) == REF(inRef)->terrainPool.end();
	REF(inRef)->accum_patch_pool = &(REF(inRef)->terrainPool[inCoordDepth]);
	if (new_pool)
	{
		DSFTuple	patchRangeMin, patchRangeMax;
		patchRangeMin.push_back(REF(inRef)->mWest);
		patchRangeMin.push_back(REF(inRef)->mSouth);
		patchRangeMin.push_back(-32768.0);
		patchRangeMin.push_back(-1.0);
		patchRangeMin.push_back(-1.0);
		for (int i = 0; i < (inCoordDepth-5); ++i)
			patchRangeMin.push_back(0.0);
		patchRangeMax.push_back(REF(inRef)->mEast);
		patchRangeMax.push_back(REF(inRef)->mNorth);
		patchRangeMax.push_back(32767.0);
		patchRangeMax.push_back(1.0);
		patchRangeMax.push_back(1.0);
		for (int i = 0; i < (inCoordDepth-5); ++i)
			patchRangeMax.push_back(1.0);

		REF(inRef)->accum_patch_pool->SetRange(patchRangeMin, patchRangeMax);
		for (int i = 0; i < REF(inRef)->mDivisions; ++i)
		for (int j = 0; j < REF(inRef)->mDivisions; ++j)
		{
			DSFTuple	fracMin, fracMax;
			fracMin.push_back((double) i / double (REF(inRef)->mDivisions));
			fracMin.push_back((double) j / double (REF(inRef)->mDivisions));
			for (int i = 0; i < (inCoordDepth-2); ++i)
				fracMin.push_back(0.0);
			fracMax.push_back((double) (i+1) / double (REF(inRef)->mDivisions));
			fracMax.push_back((double) (j+1) / double (REF(inRef)->mDivisions));
			for (int i = 0; i < (inCoordDepth-2); ++i)
				fracMax.push_back(1.0);
			REF(inRef)->accum_patch_pool->AddPool(fracMin, fracMax);
		}
	}

	REF(inRef)->patches.push_back(PatchSpec());
	REF(inRef)->accum_patch = &(REF(inRef)->patches.back());

	REF(inRef)->accum_patch->nearLOD = inNearLOD;
	REF(inRef)->accum_patch->farLOD = inFarLOD;
	REF(inRef)->accum_patch->type = inTerrainType;
	REF(inRef)->accum_patch->flags = inFlags;
	REF(inRef)->accum_patch->depth = inCoordDepth;
}

void 	DSFFileWriterImp::BeginPrimitive(
				int				inType,
				void *			inRef)
{
	REF(inRef)->accum_patch->primitives.push_back(TriPrimitive());
	REF(inRef)->accum_patch->primitives.back().type = inType;
	REF(inRef)->accum_primitive = &REF(inRef)->accum_patch->primitives.back();
}

void 	DSFFileWriterImp::AddPatchVertex(
				double			inCoordinate[],
				void *			inRef)
{
	REF(inRef)->accum_primitive->vertices.push_back(
		DSFTuple(
			inCoordinate,
			REF(inRef)->accum_patch->depth));
}

void 	DSFFileWriterImp::EndPrimitive(
				void *			inRef)
{
	if (REF(inRef)->accum_primitive->vertices.empty())
	{
//		fprintf(stderr, "WARNING: Empty primitive.\n");
		REF(inRef)->accum_patch->primitives.pop_back();
	}
}

void	DSFFileWriterImp::EndPatch(
				void *			inRef)
{
	if (REF(inRef)->accum_patch->primitives.empty())
	{
//		fprintf(stderr, "WARNING: Empty patch.\n");
		REF(inRef)->patches.pop_back();
	}
}

void	DSFFileWriterImp::AddObject(
				unsigned int	inObjectType,
				double			inCoordinates[2],
				double			inRotation,
				void *			inRef)
{
	DSFTuple	p(inCoordinates,2);
	p.push_back(inRotation);
	DSFPointPoolLoc loc = REF(inRef)->objectPool.AccumulatePoint(p);
	if (loc.first == -1 || loc.second == -1)
		printf("ERROR: could not place object %lf, %lf\n", inCoordinates[0], inCoordinates[1]);
	else {
		ObjectSpec o;
		o.type = inObjectType;
		o.pool = loc.first;
		o.location = loc.second;
		REF(inRef)->objects.push_back(o);
	}
}

void 	DSFFileWriterImp::BeginSegment(
				unsigned int	inNetworkType,
				unsigned int	inNetworkSubtype,
				unsigned int	inStartNodeID,
				double			inCoordinates[6],
				bool			inCurved,
				void *			inRef)
{
	REF(inRef)->chainSpecs.push_back(ChainSpec());
	REF(inRef)->chainSpecs.back().type = inNetworkType;
	REF(inRef)->chainSpecs.back().subType = inNetworkSubtype;
	REF(inRef)->chainSpecs.back().curved = inCurved;
	REF(inRef)->chainSpecs.back().contiguous = false;
	REF(inRef)->chainSpecs.back().startNode = inStartNodeID;
	DSFTuple	tuple(inCoordinates, inCurved ? 6 : 3);
	tuple.insert(tuple.begin()+3,inStartNodeID);
	REF(inRef)->chainSpecs.back().path.push_back(tuple);
	REF(inRef)->accum_chain = &REF(inRef)->chainSpecs.back();
}

void	DSFFileWriterImp::AddSegmentShapePoint(
				double			inCoordinates[6],
				bool			inCurved,
				void *			inRef)
{
	DSFTuple	tuple(inCoordinates, inCurved ? 6 : 3);
	tuple.insert(tuple.begin()+3,0);
	REF(inRef)->accum_chain->path.push_back(tuple);
}

void 	DSFFileWriterImp::EndSegment(
				unsigned int	inEndNodeID,
				double			inCoordinates[6],
				bool			inCurved,
				void *			inRef)
{
	DSFTuple	tuple(inCoordinates, inCurved ? 6 : 3);
	tuple.insert(tuple.begin()+3,inEndNodeID);
	REF(inRef)->accum_chain->path.push_back(tuple);
	REF(inRef)->accum_chain->endNode = inEndNodeID;
}

void 	DSFFileWriterImp::BeginPolygon(
				unsigned int	inPolygonType,
				unsigned short	inParam,
				void *			inRef)
{
	REF(inRef)->polygons.push_back(PolygonSpec());
	REF(inRef)->accum_poly = &(REF(inRef)->polygons.back());
	REF(inRef)->accum_poly_winding.clear();
	REF(inRef)->accum_poly->type = inPolygonType;
	REF(inRef)->accum_poly->param = inParam;
}

void 	DSFFileWriterImp::BeginPolygonWinding(
				void *			inRef)
{
	REF(inRef)->accum_poly_winding.push_back(DSFTupleVector());
}

void 	DSFFileWriterImp::AddPolygonPoint(
				double			inCoordinates[2],
				void *			inRef)
{
	REF(inRef)->accum_poly_winding.back().push_back(DSFTuple(inCoordinates, 2));
}

void	DSFFileWriterImp::EndPolygonWinding(
				void *			inRef)
{
}

void	DSFFileWriterImp::EndPolygon(
				void *			inRef)
{
	DSFTupleVector	pts;
	for (DSFTupleVectorVector::iterator i = REF(inRef)->accum_poly_winding.begin(); i != REF(inRef)->accum_poly_winding.end(); ++i)
		pts.insert(pts.end(), i->begin(), i->end());

	DSFPointPoolLoc	loc = REF(inRef)->polygonPool.AccumulatePoints(pts);
	if (loc.first == -1 || loc.second == -1)
	{
		printf("ERROR: Could not sink polygon point.\n");
		for (int n = 0; n < pts.size(); ++n)
		{
			for (int m = 0; m < pts[n].size(); ++m)
				printf("%lf ", pts[n][m]);
			printf("\n");
		}
	}
	REF(inRef)->accum_poly->pool = loc.first;
	REF(inRef)->accum_poly->intervals.push_back(loc.second);
	for (DSFTupleVectorVector::iterator i = REF(inRef)->accum_poly_winding.begin(); i != REF(inRef)->accum_poly_winding.end(); ++i)
	{
		REF(inRef)->accum_poly->intervals.push_back(REF(inRef)->accum_poly->intervals.back() + i->size());
	}
}
