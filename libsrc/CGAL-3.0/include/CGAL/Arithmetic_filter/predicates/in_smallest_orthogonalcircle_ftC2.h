// Copyright (c) 1997-2003  Utrecht University (The Netherlands),
// ETH Zurich (Switzerland), Freie Universitaet Berlin (Germany),
// INRIA Sophia-Antipolis (France), Martin-Luther-University Halle-Wittenberg
// (Germany), Max-Planck-Institute Saarbruecken (Germany), RISC Linz (Austria),
// and Tel-Aviv University (Israel).  All rights reserved.
//
// This file is part of CGAL (www.cgal.org); you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; version 2.1 of the License.
// See the file LICENSE.LGPL distributed with CGAL.
//
// Licensees holding a valid commercial license may use this file in
// accordance with the commercial license agreement provided with the software.
//
// This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
// WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
//
// $Source: /CVSROOT/CGAL/Packages/Interval_arithmetic/include/CGAL/Arithmetic_filter/predicates/in_smallest_orthogonalcircle_ftC2.h,v $
// $Revision: 1.40 $ $Date: 2003/10/21 12:17:30 $
// $Name: current_submission $
//
// Author(s)     : Sylvain Pion

// This file is automatically generated by
// scripts/filtered_predicates_generator.pl

#ifndef CGAL_ARITHMETIC_FILTER_IN_SMALLEST_ORTHOGONALCIRCLE_FTC2_H
#define CGAL_ARITHMETIC_FILTER_IN_SMALLEST_ORTHOGONALCIRCLE_FTC2_H

#include <CGAL/Profile_counter.h>

CGAL_BEGIN_NAMESPACE
template <class ET> class Lazy_exact_nt;
CGAL_END_NAMESPACE

CGAL_BEGIN_NAMESPACE

#ifndef CGAL_CFG_MATCHING_BUG_2
template < class CGAL_IA_CT, class CGAL_IA_ET, bool CGAL_IA_PROTECTED,
           class CGAL_IA_CACHE >
#else
static
#endif
/* CGAL_MEDIUM_INLINE */
Bounded_side
in_smallest_orthogonalcircleC2(
    const Filtered_exact <CGAL_IA_CT, CGAL_IA_ET, Dynamic, CGAL_IA_PROTECTED, CGAL_IA_CACHE> &px,
    const Filtered_exact <CGAL_IA_CT, CGAL_IA_ET, Dynamic, CGAL_IA_PROTECTED, CGAL_IA_CACHE> &py,
    const Filtered_exact <CGAL_IA_CT, CGAL_IA_ET, Dynamic, CGAL_IA_PROTECTED, CGAL_IA_CACHE> &pw,
    const Filtered_exact <CGAL_IA_CT, CGAL_IA_ET, Dynamic, CGAL_IA_PROTECTED, CGAL_IA_CACHE> &qx,
    const Filtered_exact <CGAL_IA_CT, CGAL_IA_ET, Dynamic, CGAL_IA_PROTECTED, CGAL_IA_CACHE> &qy,
    const Filtered_exact <CGAL_IA_CT, CGAL_IA_ET, Dynamic, CGAL_IA_PROTECTED, CGAL_IA_CACHE> &qw,
    const Filtered_exact <CGAL_IA_CT, CGAL_IA_ET, Dynamic, CGAL_IA_PROTECTED, CGAL_IA_CACHE> &tx,
    const Filtered_exact <CGAL_IA_CT, CGAL_IA_ET, Dynamic, CGAL_IA_PROTECTED, CGAL_IA_CACHE> &ty,
    const Filtered_exact <CGAL_IA_CT, CGAL_IA_ET, Dynamic, CGAL_IA_PROTECTED, CGAL_IA_CACHE> &tw)
{
  try
  {
    CGAL_PROFILER("IA in_smallest_orthogonalcircleC2 calls");
    Protect_FPU_rounding<CGAL_IA_PROTECTED> Protection;
    return in_smallest_orthogonalcircleC2(
		px.interval(),
		py.interval(),
		pw.interval(),
		qx.interval(),
		qy.interval(),
		qw.interval(),
		tx.interval(),
		ty.interval(),
		tw.interval());
  }
  catch (Interval_nt_advanced::unsafe_comparison)
  {
    CGAL_PROFILER("IA in_smallest_orthogonalcircleC2 failures");
    Protect_FPU_rounding<!CGAL_IA_PROTECTED> Protection(CGAL_FE_TONEAREST);
    return in_smallest_orthogonalcircleC2(
		px.exact(),
		py.exact(),
		pw.exact(),
		qx.exact(),
		qy.exact(),
		qw.exact(),
		tx.exact(),
		ty.exact(),
		tw.exact());
  }
}

#ifndef CGAL_CFG_MATCHING_BUG_2
template < class ET >
/* CGAL_MEDIUM_INLINE */
Bounded_side
in_smallest_orthogonalcircleC2(
    const Lazy_exact_nt<ET> &px,
    const Lazy_exact_nt<ET> &py,
    const Lazy_exact_nt<ET> &pw,
    const Lazy_exact_nt<ET> &qx,
    const Lazy_exact_nt<ET> &qy,
    const Lazy_exact_nt<ET> &qw,
    const Lazy_exact_nt<ET> &tx,
    const Lazy_exact_nt<ET> &ty,
    const Lazy_exact_nt<ET> &tw)
{
  try
  {
    CGAL_PROFILER("Lazy IA in_smallest_orthogonalcircleC2 calls");
    Protect_FPU_rounding<true> Protection;
    return in_smallest_orthogonalcircleC2(
		px.interval(),
		py.interval(),
		pw.interval(),
		qx.interval(),
		qy.interval(),
		qw.interval(),
		tx.interval(),
		ty.interval(),
		tw.interval());
  }
  catch (Interval_nt_advanced::unsafe_comparison)
  {
    CGAL_PROFILER("Lazy IA in_smallest_orthogonalcircleC2 failures");
    Protect_FPU_rounding<false> Protection(CGAL_FE_TONEAREST);
    return in_smallest_orthogonalcircleC2(
		px.exact(),
		py.exact(),
		pw.exact(),
		qx.exact(),
		qy.exact(),
		qw.exact(),
		tx.exact(),
		ty.exact(),
		tw.exact());
  }
}
#endif

CGAL_END_NAMESPACE

#endif // CGAL_ARITHMETIC_FILTER_IN_SMALLEST_ORTHOGONALCIRCLE_FTC2_H