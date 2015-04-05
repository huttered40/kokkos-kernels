/*
//@HEADER
// ************************************************************************
//
//          Kokkos: Node API and Parallel Node Kernels
//              Copyright (2008) Sandia Corporation
//
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact Michael A. Heroux (maherou@sandia.gov)
//
// ************************************************************************
//@HEADER
*/
#ifndef KOKKOS_BLAS1_MV_IMPL_HPP_
#define KOKKOS_BLAS1_MV_IMPL_HPP_

#include <TpetraKernels_config.h>
#include <Kokkos_Core.hpp>
#include <Kokkos_InnerProductSpaceTraits.hpp>

namespace KokkosBlas {
namespace Impl {

/// \brief Dot product functor for single vectors.
///
/// \tparam RV 0-D output View
/// \tparam XV 1-D input View
/// \tparam YV 1-D input View
/// \tparam SizeType Index type.  Use int (32 bits) if possible.
template<class RV, class XV, class YV, class SizeType = typename XV::size_type>
struct V_Dot_Functor
{
  typedef typename XV::execution_space              execution_space;
  typedef SizeType                                        size_type;
  typedef typename XV::non_const_value_type             xvalue_type;
  typedef Kokkos::Details::InnerProductSpaceTraits<xvalue_type> IPT;
  typedef Kokkos::Details::ArithTraits<typename IPT::dot_type>   AT;
  typedef typename IPT::dot_type                         value_type;

  RV m_r;
  XV m_x;
  YV m_y;

  V_Dot_Functor (const RV& r, const XV& x, const YV& y) :
    m_r (r), m_x (x), m_y (y)
  {}

  KOKKOS_INLINE_FUNCTION
  void operator() (const size_type& i, value_type& sum) const
  {
    sum += IPT::dot (m_x(i), m_y(i)); // m_x(i) * m_y(i)
  }

  KOKKOS_INLINE_FUNCTION void init (value_type& update) const
  {
    update = AT::zero ();
  }

  KOKKOS_INLINE_FUNCTION void
  join (volatile value_type& update,
        const volatile value_type& source) const
  {
    update += source;
  }

  // On device, write the reduction result to the output View.
  KOKKOS_INLINE_FUNCTION void
  final (const value_type& dst) const
  {
    m_r() = dst;
  }
};

/// \brief Column-wise dot product functor for multivectors; works for
///   any layout, but best performance with LayoutRight.
///
/// \tparam RV 1-D output View
/// \tparam XMV 2-D input View
/// \tparam YMV 2-D input View
/// \tparam SizeType Index type.  Use int (32 bits) if possible.
template<class RV, class XMV, class YMV, class SizeType = typename XMV::size_type>
struct MV_Dot_Right_FunctorVector
{
  typedef typename XMV::execution_space             execution_space;
  typedef SizeType                                        size_type;
  typedef typename XMV::non_const_value_type            xvalue_type;
  typedef Kokkos::Details::InnerProductSpaceTraits<xvalue_type> IPT;
  typedef Kokkos::Details::ArithTraits<typename IPT::dot_type>   AT;
  typedef typename IPT::dot_type                       value_type[];

  size_type value_count;
  RV m_r;
  typename XMV::const_type m_x;
  typename YMV::const_type m_y;

  MV_Dot_Right_FunctorVector (const RV& r, const XMV& x, const YMV& y) :
    value_count (x.dimension_1 ()), m_r (r), m_x (x), m_y (y)
  {}

  KOKKOS_INLINE_FUNCTION
  void operator() (const size_type& i, value_type sum) const
  {
    const size_type numVecs = value_count;
#ifdef KOKKOS_HAVE_PRAGMA_IVDEP
#pragma ivdep
#endif
#ifdef KOKKOS_HAVE_PRAGMA_VECTOR
#pragma vector always
#endif
    for (size_type k = 0; k < numVecs; ++k) {
      sum[k] += IPT::dot (m_x(i,k), m_y(i,k)); // m_x(i,k) * m_y(i,k)
    }
  }

  KOKKOS_INLINE_FUNCTION void init (value_type update) const
  {
    const size_type numVecs = value_count;
#ifdef KOKKOS_HAVE_PRAGMA_IVDEP
#pragma ivdep
#endif
#ifdef KOKKOS_HAVE_PRAGMA_VECTOR
#pragma vector always
#endif
    for (size_type k = 0; k < numVecs; ++k) {
      update[k] = AT::zero ();
    }
  }

  KOKKOS_INLINE_FUNCTION void
  join (volatile value_type update,
        const volatile value_type source) const
  {
    const size_type numVecs = value_count;
#ifdef KOKKOS_HAVE_PRAGMA_IVDEP
#pragma ivdep
#endif
#ifdef KOKKOS_HAVE_PRAGMA_VECTOR
#pragma vector always
#endif
    for (size_type k = 0; k < numVecs; ++k) {
      update[k] += source[k];
    }
  }

  // On device, write the reduction result to the output View.
  KOKKOS_INLINE_FUNCTION void
  final (const value_type dst) const
  {
    const size_type numVecs = value_count;
#ifdef KOKKOS_HAVE_PRAGMA_IVDEP
#pragma ivdep
#endif
#ifdef KOKKOS_HAVE_PRAGMA_VECTOR
#pragma vector always
#endif
    for (size_type k = 0; k < numVecs; ++k) {
      m_r(k) = dst[k];
    }
  }
};

/// \brief Column-wise dot product functor for multivectors with
///   number of columns known at compile time; works for any layout,
///   but best performance with LayoutRight.
///
/// \tparam RV 1-D output View
/// \tparam XMV 2-D input View
/// \tparam YMV 2-D input View
/// \tparam UNROLL Number of columns (vectors)
/// \tparam SizeType Index type.  Use int (32 bits) if possible.
template<class RV, class XMV, class YMV, int UNROLL, class SizeType = typename XMV::size_type>
struct MV_Dot_Right_FunctorUnroll
{
  typedef typename XMV::execution_space             execution_space;
  typedef SizeType                                        size_type;
  typedef typename XMV::non_const_value_type            xvalue_type;
  typedef Kokkos::Details::InnerProductSpaceTraits<xvalue_type> IPT;
  typedef Kokkos::Details::ArithTraits<typename IPT::dot_type>   AT;
  typedef typename IPT::dot_type                       value_type[];

  size_type value_count;
  RV m_r;
  typename XMV::const_type m_x;
  typename YMV::const_type m_y;

  MV_Dot_Right_FunctorUnroll (const RV& r, const XMV& x, const YMV& y) :
    value_count (x.dimension_1 ()), m_r (r), m_x (x), m_y (y)
  {}

  KOKKOS_INLINE_FUNCTION
  void operator() (const size_type& i, value_type sum) const
  {
#ifdef KOKKOS_HAVE_PRAGMA_UNROLL
#pragma unroll
#endif
    for (int k = 0; k < UNROLL; ++k) {
      sum[k] += IPT::dot (m_x(i,k), m_y(i,k)); // m_x(i,k) * m_y(i,k)
    }
  }

  KOKKOS_INLINE_FUNCTION void init (volatile value_type update) const
  {
#ifdef KOKKOS_HAVE_PRAGMA_UNROLL
#pragma unroll
#endif
    for (int k = 0; k < UNROLL; ++k) {
      update[k] = AT::zero ();
    }
  }

  KOKKOS_INLINE_FUNCTION void
  join (volatile value_type update,
        const volatile value_type source) const
  {
#ifdef KOKKOS_HAVE_PRAGMA_UNROLL
#pragma unroll
#endif
    for (int k = 0; k < UNROLL; ++k) {
      update[k] += source[k];
    }
  }

  // On device, write the reduction result to the output View.
  KOKKOS_INLINE_FUNCTION void
  final (const value_type dst) const
  {
#ifdef KOKKOS_HAVE_PRAGMA_UNROLL
#pragma unroll
#endif
    for (int k = 0; k < UNROLL; ++k) {
      m_r(k) = dst[k];
    }
  }
};


//! Implementation of KokkosBlas::dot for multivectors.
template<class RT, class RL, class RD, class RM, class RS,
         class XT, class XL, class XD, class XM, class XS,
         class YT, class YL, class YD, class YM, class YS>
struct Dot_MV {
  typedef Kokkos::View<RT,RL,RD,RM,RS> RV;
  typedef Kokkos::View<XT,XL,XD,XM,XS> XMV;
  typedef Kokkos::View<YT,YL,YD,YM,YS> YMV;
  typedef typename XMV::size_type size_type;

  /// \brief Compute the dot product(s) of the column(s) of the
  ///   multivectors (2-D views) x and y, and store result(s) in r.
  static void dot (const RV& r, const XMV& X, const YMV& Y)
  {
    const size_type numRows = X.dimension_0 ();
    const size_type numVecs = X.dimension_1 ();

    if (numVecs > 16) {
      MV_Dot_Right_FunctorVector<RV, XMV, YMV> op (r, X, Y);
      Kokkos::parallel_reduce (numRows, op);
    }
    else {
      switch (numVecs) {
      case 16: {
        MV_Dot_Right_FunctorUnroll<RV, XMV, YMV, 16> op (r, X, Y);
        Kokkos::parallel_reduce (numRows, op);
        break;
      }
      case 15: {
        MV_Dot_Right_FunctorUnroll<RV, XMV, YMV, 15> op (r, X, Y);
        Kokkos::parallel_reduce (numRows, op);
        break;
      }
      case 14: {
        MV_Dot_Right_FunctorUnroll<RV, XMV, YMV, 14> op (r, X, Y);
        Kokkos::parallel_reduce (numRows, op);
        break;
      }
      case 13: {
        MV_Dot_Right_FunctorUnroll<RV, XMV, YMV, 13> op (r, X, Y);
        Kokkos::parallel_reduce (numRows, op);
        break;
      }
      case 12: {
        MV_Dot_Right_FunctorUnroll<RV, XMV, YMV, 12> op (r, X, Y);
        Kokkos::parallel_reduce (numRows, op);
        break;
      }
      case 11: {
        MV_Dot_Right_FunctorUnroll<RV, XMV, YMV, 11> op (r, X, Y);
        Kokkos::parallel_reduce (numRows, op);
        break;
      }
      case 10: {
        MV_Dot_Right_FunctorUnroll<RV, XMV, YMV, 10> op (r, X, Y);
        Kokkos::parallel_reduce (numRows, op);
        break;
      }
      case 9: {
        MV_Dot_Right_FunctorUnroll<RV, XMV, YMV, 9> op (r, X, Y);
        Kokkos::parallel_reduce (numRows, op);
        break;
      }
      case 8: {
        MV_Dot_Right_FunctorUnroll<RV, XMV, YMV, 8> op (r, X, Y);
        Kokkos::parallel_reduce (numRows, op);
        break;
      }
      case 7: {
        MV_Dot_Right_FunctorUnroll<RV, XMV, YMV, 7> op (r, X, Y);
        Kokkos::parallel_reduce (numRows, op);
        break;
      }
      case 6: {
        MV_Dot_Right_FunctorUnroll<RV, XMV, YMV, 6> op (r, X, Y);
        Kokkos::parallel_reduce (numRows, op);
        break;
      }
      case 5: {
        MV_Dot_Right_FunctorUnroll<RV, XMV, YMV, 5> op (r, X, Y);
        Kokkos::parallel_reduce (numRows, op);
        break;
      }
      case 4: {
        MV_Dot_Right_FunctorUnroll<RV, XMV, YMV, 4> op (r, X, Y);
        Kokkos::parallel_reduce (numRows, op);
        break;
      }
      case 3: {
        MV_Dot_Right_FunctorUnroll<RV, XMV, YMV, 3> op (r, X, Y);
        Kokkos::parallel_reduce (numRows, op);
        break;
      }
      case 2: {
        MV_Dot_Right_FunctorUnroll<RV, XMV, YMV, 2> op (r, X, Y);
        Kokkos::parallel_reduce (numRows, op);
        break;
      }
      case 1: {
        // RV needs to turn 0-D, and XMV and YMV need to turn 1-D.
        using Kokkos::ALL;
        using Kokkos::subview;
        typedef Kokkos::View<typename RV::value_type, typename RV::array_layout,
            typename RV::device_type, typename RV::memory_traits,
            typename RV::specialize> RV1D;
        typedef Kokkos::View<typename XMV::const_value_type*,
            XL,XD,XM,XS> XMV1D;
        typedef Kokkos::View<typename YMV::const_value_type*,
            YL,YD,YM,YS> YMV1D;
        typedef V_Dot_Functor<RV1D, XMV1D, YMV1D> op_type;
        op_type op (subview (r, 0), subview (X, ALL (), 0), subview (Y, ALL (), 0));
        Kokkos::parallel_reduce (numRows, op);
        break;
      }
      } // switch
    } // if-else
  }

  /// \brief Compute the dot product of X(:,X_col) and Y(:,Y_col), and
  ///   store result in r(0).
  static void
  dot (const RV& r, const XMV& X, const size_t X_col,
       const YMV& Y, const size_t Y_col)
  {
    // RV needs to turn 0-D, and XMV and YMV need to turn 1-D.
    using Kokkos::ALL;
    using Kokkos::subview;
    typedef Kokkos::View<typename RV::value_type, typename RV::array_layout,
      typename RV::device_type, typename RV::memory_traits,
      typename RV::specialize> RV0D;
    typedef Kokkos::View<typename XMV::const_value_type*, XL, XD, XM, XS> XMV1D;
    typedef Kokkos::View<typename YMV::const_value_type*, YL, YD, YM, YS> YMV1D;

    const size_type numRows = X.dimension_0 ();
    const size_type numCols = X.dimension_1 ();
    if (numRows < static_cast<size_type> (INT_MAX) &&
        numRows * numCols < static_cast<size_type> (INT_MAX)) {
      typedef V_Dot_Functor<RV0D, XMV1D, YMV1D, int> op_type;
      op_type op (subview (r, 0), subview (X, ALL (), X_col),
                  subview (Y, ALL (), Y_col));
      Kokkos::parallel_reduce (numRows, op);
    }
    else {
      typedef V_Dot_Functor<RV0D, XMV1D, YMV1D, size_type> op_type;
      op_type op (subview (r, 0), subview (X, ALL (), X_col),
                  subview (Y, ALL (), Y_col));
      Kokkos::parallel_reduce (numRows, op);
    }
  }
};

// Full specializations for cases of interest for Tpetra::MultiVector.
//
// Currently, we include specializations for Scalar = double,
// LayoutLeft (which is what Tpetra::MultiVector uses at the moment),
// and all execution spaces.  This may change in the future.

#ifdef KOKKOS_HAVE_SERIAL
template<>
struct Dot_MV<double*,
              Kokkos::Serial::array_layout,
              Kokkos::Device<Kokkos::Serial, Kokkos::HostSpace>,
              Kokkos::MemoryTraits<Kokkos::Unmanaged>,
              Kokkos::Impl::ViewDefault,
              const double**,
              Kokkos::LayoutLeft,
              Kokkos::Device<Kokkos::Serial, Kokkos::HostSpace>,
              Kokkos::MemoryTraits<Kokkos::Unmanaged>,
              Kokkos::Impl::ViewDefault,
              const double**,
              Kokkos::LayoutLeft,
              Kokkos::Device<Kokkos::Serial, Kokkos::HostSpace>,
              Kokkos::MemoryTraits<Kokkos::Unmanaged>,
              Kokkos::Impl::ViewDefault> {
  typedef double* RT;
  typedef Kokkos::Serial::array_layout RL;
  typedef Kokkos::Device<Kokkos::Serial, Kokkos::HostSpace> RD;
  typedef Kokkos::MemoryTraits<Kokkos::Unmanaged> RM;
  typedef Kokkos::Impl::ViewDefault RS;
  typedef Kokkos::View<RT,RL,RD,RM,RS> RV;

  typedef const double** XT;
  typedef Kokkos::LayoutLeft XL;
  typedef Kokkos::Device<Kokkos::Serial, Kokkos::HostSpace> XD;
  typedef Kokkos::MemoryTraits<Kokkos::Unmanaged> XM;
  typedef Kokkos::Impl::ViewDefault XS;
  typedef Kokkos::View<XT,XL,XD,XM,XS> XMV;

  typedef Kokkos::View<XT,XL,XD,XM,XS> YMV;
  typedef XMV::size_type size_type;

  static void dot (const RV& r, const XMV& X, const YMV& Y);

  static void
  dot (const RV& r, const XMV& X, const size_t X_col,
       const YMV& Y, const size_t Y_col);
};
#endif // KOKKOS_HAVE_SERIAL

#ifdef KOKKOS_HAVE_OPENMP
template<>
struct Dot_MV<double*,
              Kokkos::OpenMP::array_layout,
              Kokkos::Device<Kokkos::OpenMP, Kokkos::HostSpace>,
              Kokkos::MemoryTraits<Kokkos::Unmanaged>,
              Kokkos::Impl::ViewDefault,
              const double**,
              Kokkos::LayoutLeft,
              Kokkos::Device<Kokkos::OpenMP, Kokkos::HostSpace>,
              Kokkos::MemoryTraits<Kokkos::Unmanaged>,
              Kokkos::Impl::ViewDefault,
              const double**,
              Kokkos::LayoutLeft,
              Kokkos::Device<Kokkos::OpenMP, Kokkos::HostSpace>,
              Kokkos::MemoryTraits<Kokkos::Unmanaged>,
              Kokkos::Impl::ViewDefault> {
  typedef double* RT;
  typedef Kokkos::OpenMP::array_layout RL;
  typedef Kokkos::Device<Kokkos::OpenMP, Kokkos::HostSpace> RD;
  typedef Kokkos::MemoryTraits<Kokkos::Unmanaged> RM;
  typedef Kokkos::Impl::ViewDefault RS;
  typedef Kokkos::View<RT,RL,RD,RM,RS> RV;

  typedef const double** XT;
  typedef Kokkos::LayoutLeft XL;
  typedef Kokkos::Device<Kokkos::OpenMP, Kokkos::HostSpace> XD;
  typedef Kokkos::MemoryTraits<Kokkos::Unmanaged> XM;
  typedef Kokkos::Impl::ViewDefault XS;
  typedef Kokkos::View<XT,XL,XD,XM,XS> XMV;

  typedef Kokkos::View<XT,XL,XD,XM,XS> YMV;
  typedef XMV::size_type size_type;

  static void dot (const RV& r, const XMV& X, const YMV& Y);

  static void
  dot (const RV& r, const XMV& X, const size_t X_col,
       const YMV& Y, const size_t Y_col);
};
#endif // KOKKOS_HAVE_OPENMP

#ifdef KOKKOS_HAVE_PTHREAD
template<>
struct Dot_MV<double*,
              Kokkos::Threads::array_layout,
              Kokkos::Device<Kokkos::Threads, Kokkos::HostSpace>,
              Kokkos::MemoryTraits<Kokkos::Unmanaged>,
              Kokkos::Impl::ViewDefault,
              const double**,
              Kokkos::LayoutLeft,
              Kokkos::Device<Kokkos::Threads, Kokkos::HostSpace>,
              Kokkos::MemoryTraits<Kokkos::Unmanaged>,
              Kokkos::Impl::ViewDefault,
              const double**,
              Kokkos::LayoutLeft,
              Kokkos::Device<Kokkos::Threads, Kokkos::HostSpace>,
              Kokkos::MemoryTraits<Kokkos::Unmanaged>,
              Kokkos::Impl::ViewDefault> {
  typedef double* RT;
  typedef Kokkos::Threads::array_layout RL;
  typedef Kokkos::Device<Kokkos::Threads, Kokkos::HostSpace> RD;
  typedef Kokkos::MemoryTraits<Kokkos::Unmanaged> RM;
  typedef Kokkos::Impl::ViewDefault RS;
  typedef Kokkos::View<RT,RL,RD,RM,RS> RV;

  typedef const double** XT;
  typedef Kokkos::LayoutLeft XL;
  typedef Kokkos::Device<Kokkos::Threads, Kokkos::HostSpace> XD;
  typedef Kokkos::MemoryTraits<Kokkos::Unmanaged> XM;
  typedef Kokkos::Impl::ViewDefault XS;
  typedef Kokkos::View<XT,XL,XD,XM,XS> XMV;

  typedef Kokkos::View<XT,XL,XD,XM,XS> YMV;
  typedef XMV::size_type size_type;

  static void dot (const RV& r, const XMV& X, const YMV& Y);

  static void
  dot (const RV& r, const XMV& X, const size_t X_col,
       const YMV& Y, const size_t Y_col);
};
#endif // KOKKOS_HAVE_PTHREAD

#ifdef KOKKOS_HAVE_CUDA
template<>
struct Dot_MV<double*,
              Kokkos::Cuda::array_layout,
              Kokkos::Device<Kokkos::Cuda, Kokkos::CudaSpace>,
              Kokkos::MemoryTraits<Kokkos::Unmanaged>,
              Kokkos::Impl::ViewDefault,
              const double**,
              Kokkos::LayoutLeft,
              Kokkos::Device<Kokkos::Cuda, Kokkos::CudaSpace>,
              Kokkos::MemoryTraits<Kokkos::Unmanaged>,
              Kokkos::Impl::ViewDefault,
              const double**,
              Kokkos::LayoutLeft,
              Kokkos::Device<Kokkos::Cuda, Kokkos::CudaSpace>,
              Kokkos::MemoryTraits<Kokkos::Unmanaged>,
              Kokkos::Impl::ViewDefault> {
  typedef double* RT;
  typedef Kokkos::Cuda::array_layout RL;
  typedef Kokkos::Device<Kokkos::Cuda, Kokkos::CudaSpace> RD;
  typedef Kokkos::MemoryTraits<Kokkos::Unmanaged> RM;
  typedef Kokkos::Impl::ViewDefault RS;
  typedef Kokkos::View<RT,RL,RD,RM,RS> RV;

  typedef const double** XT;
  typedef Kokkos::LayoutLeft XL;
  typedef Kokkos::Device<Kokkos::Cuda, Kokkos::CudaSpace> XD;
  typedef Kokkos::MemoryTraits<Kokkos::Unmanaged> XM;
  typedef Kokkos::Impl::ViewDefault XS;
  typedef Kokkos::View<XT,XL,XD,XM,XS> XMV;

  typedef Kokkos::View<XT,XL,XD,XM,XS> YMV;
  typedef XMV::size_type size_type;

  static void dot (const RV& r, const XMV& X, const YMV& Y);

  static void
  dot (const RV& r, const XMV& X, const size_t X_col,
       const YMV& Y, const size_t Y_col);
};
#endif // KOKKOS_HAVE_CUDA

#ifdef KOKKOS_HAVE_CUDA
template<>
struct Dot_MV<double*,
              Kokkos::Cuda::array_layout,
              Kokkos::Device<Kokkos::Cuda, Kokkos::CudaUVMSpace>,
              Kokkos::MemoryTraits<Kokkos::Unmanaged>,
              Kokkos::Impl::ViewDefault,
              const double**,
              Kokkos::LayoutLeft,
              Kokkos::Device<Kokkos::Cuda, Kokkos::CudaUVMSpace>,
              Kokkos::MemoryTraits<Kokkos::Unmanaged>,
              Kokkos::Impl::ViewDefault,
              const double**,
              Kokkos::LayoutLeft,
              Kokkos::Device<Kokkos::Cuda, Kokkos::CudaUVMSpace>,
              Kokkos::MemoryTraits<Kokkos::Unmanaged>,
              Kokkos::Impl::ViewDefault> {
  typedef double* RT;
  typedef Kokkos::Cuda::array_layout RL;
  typedef Kokkos::Device<Kokkos::Cuda, Kokkos::CudaUVMSpace> RD;
  typedef Kokkos::MemoryTraits<Kokkos::Unmanaged> RM;
  typedef Kokkos::Impl::ViewDefault RS;
  typedef Kokkos::View<RT,RL,RD,RM,RS> RV;

  typedef const double** XT;
  typedef Kokkos::LayoutLeft XL;
  typedef Kokkos::Device<Kokkos::Cuda, Kokkos::CudaUVMSpace> XD;
  typedef Kokkos::MemoryTraits<Kokkos::Unmanaged> XM;
  typedef Kokkos::Impl::ViewDefault XS;
  typedef Kokkos::View<XT,XL,XD,XM,XS> XMV;

  typedef Kokkos::View<XT,XL,XD,XM,XS> YMV;
  typedef XMV::size_type size_type;

  static void dot (const RV& r, const XMV& X, const YMV& Y);

  static void
  dot (const RV& r, const XMV& X, const size_t X_col,
       const YMV& Y, const size_t Y_col);
};
#endif // KOKKOS_HAVE_CUDA

//
// fill
//

template<class XMV, class SizeType = typename XMV::size_type>
struct MV_FillFunctor {
  typedef typename XMV::execution_space             execution_space;
  typedef SizeType                                        size_type;
  typedef typename XMV::non_const_value_type            xvalue_type;

  const size_type numCols_;
  const xvalue_type val_;
  XMV X_;

  MV_FillFunctor (const XMV& X, const xvalue_type& val) :
    numCols_ (X.dimension_1 ()), val_ (val), X_ (X)
  {}

  KOKKOS_INLINE_FUNCTION
  void operator() (const size_type& i) const
  {
    for (size_type j = 0; j < numCols_; ++j) {
      X_(i,j) = val_;
    }
  }
};

template<class XV, class SizeType = typename XV::size_type>
struct V_FillFunctor {
  typedef typename XV::execution_space            execution_space;
  typedef SizeType                                      size_type;
  typedef typename XV::non_const_value_type           xvalue_type;

  const xvalue_type val_;
  XV x_;

  V_FillFunctor (const XV& x, const xvalue_type& val) :
    val_ (val), x_ (x)
  {}

  KOKKOS_INLINE_FUNCTION
  void operator() (const size_type& i) const
  {
    x_(i) = val_;
  }
};

//! Implementation of KokkosBlas::fill for multivectors.
template<class XT, class XL, class XD, class XM, class XS>
struct Fill_MV {
  typedef Kokkos::View<XT,XL,XD,XM,XS> XMV;
  typedef typename XMV::execution_space execution_space;
  typedef typename XMV::size_type size_type;

  static void fill (const XMV& X, const typename XMV::non_const_value_type& val)
  {
    const size_type numRows = X.dimension_0 ();
    const size_type numCols = X.dimension_1 ();

    // The first condition helps avoid overflow with the
    // multiplication in the second condition.
    if (numRows < static_cast<size_type> (INT_MAX) &&
        numRows * numCols < static_cast<size_type> (INT_MAX)) {
      Kokkos::RangePolicy<execution_space, int> policy (0, numRows);
      MV_FillFunctor<XMV, int> op (X, val);
      Kokkos::parallel_for (policy, op);
    }
    else {
      Kokkos::RangePolicy<execution_space, size_type> policy (0, numRows);
      MV_FillFunctor<XMV, size_type> op (X, val);
      Kokkos::parallel_for (policy, op);
    }
  }
};

#ifdef KOKKOS_HAVE_SERIAL
template<>
struct Fill_MV<double**, Kokkos::LayoutLeft, Kokkos::Device<Kokkos::Serial, Kokkos::HostSpace>,
               Kokkos::MemoryTraits<Kokkos::Unmanaged>, Kokkos::Impl::ViewDefault> {
  typedef Kokkos::View<double**, Kokkos::LayoutLeft, Kokkos::Device<Kokkos::Serial, Kokkos::HostSpace>,
                       Kokkos::MemoryTraits<Kokkos::Unmanaged>, Kokkos::Impl::ViewDefault> XMV;
  typedef Kokkos::Serial execution_space;
  typedef XMV::size_type size_type;

  static void fill (const XMV& X, const double& val);
};
#endif // KOKKOS_HAVE_SERIAL

#ifdef KOKKOS_HAVE_OPENMP
template<>
struct Fill_MV<double**, Kokkos::LayoutLeft, Kokkos::Device<Kokkos::OpenMP, Kokkos::HostSpace>,
               Kokkos::MemoryTraits<Kokkos::Unmanaged>, Kokkos::Impl::ViewDefault> {
  typedef Kokkos::View<double**, Kokkos::LayoutLeft, Kokkos::Device<Kokkos::OpenMP, Kokkos::HostSpace>,
                       Kokkos::MemoryTraits<Kokkos::Unmanaged>, Kokkos::Impl::ViewDefault> XMV;
  typedef Kokkos::OpenMP execution_space;
  typedef XMV::size_type size_type;

  static void fill (const XMV& X, const double& val);
};
#endif // KOKKOS_HAVE_OPENMP

#ifdef KOKKOS_HAVE_PTHREAD
template<>
struct Fill_MV<double**, Kokkos::LayoutLeft, Kokkos::Device<Kokkos::Threads, Kokkos::HostSpace>,
               Kokkos::MemoryTraits<Kokkos::Unmanaged>, Kokkos::Impl::ViewDefault> {
  typedef Kokkos::View<double**, Kokkos::LayoutLeft, Kokkos::Device<Kokkos::Threads, Kokkos::HostSpace>,
                       Kokkos::MemoryTraits<Kokkos::Unmanaged>, Kokkos::Impl::ViewDefault> XMV;
  typedef Kokkos::Threads execution_space;
  typedef XMV::size_type size_type;

  static void fill (const XMV& X, const double& val);
};
#endif // KOKKOS_HAVE_PTHREAD

#ifdef KOKKOS_HAVE_CUDA
template<>
struct Fill_MV<double**, Kokkos::LayoutLeft, Kokkos::Device<Kokkos::Cuda, Kokkos::CudaSpace>,
               Kokkos::MemoryTraits<Kokkos::Unmanaged>, Kokkos::Impl::ViewDefault> {
  typedef Kokkos::View<double**, Kokkos::LayoutLeft, Kokkos::Device<Kokkos::Cuda, Kokkos::CudaSpace>,
                       Kokkos::MemoryTraits<Kokkos::Unmanaged>, Kokkos::Impl::ViewDefault> XMV;
  typedef Kokkos::Cuda execution_space;
  typedef XMV::size_type size_type;

  static void fill (const XMV& X, const double& val);
};
#endif // KOKKOS_HAVE_CUDA

#ifdef KOKKOS_HAVE_CUDA
template<>
struct Fill_MV<double**, Kokkos::LayoutLeft, Kokkos::Device<Kokkos::Cuda, Kokkos::CudaUVMSpace>,
               Kokkos::MemoryTraits<Kokkos::Unmanaged>, Kokkos::Impl::ViewDefault> {
  typedef Kokkos::View<double**, Kokkos::LayoutLeft, Kokkos::Device<Kokkos::Cuda, Kokkos::CudaUVMSpace>,
                       Kokkos::MemoryTraits<Kokkos::Unmanaged>, Kokkos::Impl::ViewDefault> XMV;
  typedef Kokkos::Cuda execution_space;
  typedef XMV::size_type size_type;

  static void fill (const XMV& X, const double& val);
};
#endif // KOKKOS_HAVE_CUDA

//
// nrm2_squared
//

/// \brief 2-norm (squared) functor for single vectors.
///
/// \tparam RV 0-D output View
/// \tparam XV 1-D input View
/// \tparam SizeType Index type.  Use int (32 bits) if possible.
template<class RV, class XV, class SizeType = typename XV::size_type>
struct V_Nrm2Squared_Functor
{
  typedef typename XV::execution_space              execution_space;
  typedef SizeType                                        size_type;
  typedef typename XV::non_const_value_type             xvalue_type;
  typedef Kokkos::Details::InnerProductSpaceTraits<xvalue_type> IPT;
  typedef Kokkos::Details::ArithTraits<typename IPT::mag_type>   AT;
  typedef typename IPT::mag_type                         value_type;

  RV m_r;
  typename XV::const_type m_x;

  V_Nrm2Squared_Functor (const RV& r, const XV& x) :
    m_r (r), m_x (x)
  {}

  KOKKOS_INLINE_FUNCTION
  void operator() (const size_type& i, value_type& sum) const
  {
    const typename IPT::mag_type tmp = IPT::norm (m_x(i));
    sum += tmp * tmp;
  }

  KOKKOS_INLINE_FUNCTION void init (value_type& update) const
  {
    update = AT::zero ();
  }

  KOKKOS_INLINE_FUNCTION void
  join (volatile value_type& update,
        const volatile value_type& source) const
  {
    update += source;
  }

  // On device, write the reduction result to the output View.
  KOKKOS_INLINE_FUNCTION void
  final (const value_type& dst) const
  {
    m_r() = dst;
  }
};

// Partial specialization for Scalar=double.
template<class RL, class RD, class RM, class RS,
         class XL, class XD, class XM, class XS>
struct V_Nrm2Squared_Functor<
  Kokkos::View<double, RL, RD, RM, RS>,
  Kokkos::View<const double*, XL, XD, XM, XS>,
  int>
{
  typedef Kokkos::View<double, RL, RD, RM, RS> RV;
  typedef Kokkos::View<const double*, XL, XD, XM, XS> XV;

  typedef typename XV::execution_space execution_space;
  typedef int                                size_type;
  typedef double                           xvalue_type;
  typedef double                            value_type;

  RV m_r;
  XV m_x;

  V_Nrm2Squared_Functor (const RV& r, const XV& x) :
    m_r (r), m_x (x)
  {}

  KOKKOS_INLINE_FUNCTION
  void operator() (const size_type& i, value_type& sum) const
  {
    const double tmp = m_x(i);
    sum += tmp * tmp;
  }

  KOKKOS_INLINE_FUNCTION void init (value_type& update) const
  {
    update = 0.0;
  }

  KOKKOS_INLINE_FUNCTION void
  join (volatile value_type& update,
        const volatile value_type& source) const
  {
    update += source;
  }

  // On device, write the reduction result to the output View.
  KOKKOS_INLINE_FUNCTION void
  final (const value_type& dst) const
  {
    m_r() = dst;
  }
};


/// \brief Column-wise 2-norm functor for multivectors; works for
///   any layout, but best performance with LayoutRight.
///
/// \tparam RV 1-D output View
/// \tparam XMV 2-D input View
/// \tparam SizeType Index type.  Use int (32 bits) if possible.
template<class RV, class XMV, class SizeType = typename XMV::size_type>
struct MV_Nrm2Squared_Right_FunctorVector
{
  typedef typename XMV::execution_space             execution_space;
  typedef SizeType                                        size_type;
  typedef typename XMV::non_const_value_type            xvalue_type;
  typedef Kokkos::Details::InnerProductSpaceTraits<xvalue_type> IPT;
  typedef Kokkos::Details::ArithTraits<typename IPT::mag_type>   AT;
  typedef typename IPT::mag_type                       value_type[];

  size_type value_count;
  RV m_r;
  typename XMV::const_type m_x;

  MV_Nrm2Squared_Right_FunctorVector (const RV& r, const XMV& x) :
    value_count (x.dimension_1 ()), m_r (r), m_x (x)
  {}

  KOKKOS_INLINE_FUNCTION void
  operator() (const size_type i, value_type sum) const
  {
    const size_type numVecs = value_count;
#ifdef KOKKOS_HAVE_PRAGMA_IVDEP
#pragma ivdep
#endif
#ifdef KOKKOS_HAVE_PRAGMA_VECTOR
#pragma vector always
#endif
    for (size_type j = 0; j < numVecs; ++j) {
      const typename IPT::mag_type tmp = IPT::norm (m_x(i,j));
      sum[j] += tmp * tmp;
    }
  }

  KOKKOS_INLINE_FUNCTION void
  init (value_type update) const
  {
    const size_type numVecs = value_count;
#ifdef KOKKOS_HAVE_PRAGMA_IVDEP
#pragma ivdep
#endif
#ifdef KOKKOS_HAVE_PRAGMA_VECTOR
#pragma vector always
#endif
    for (size_type j = 0; j < numVecs; ++j) {
      update[j] = AT::zero ();
    }
  }

  KOKKOS_INLINE_FUNCTION void
  join (volatile value_type update,
        const volatile value_type source) const
  {
    const size_type numVecs = value_count;
#ifdef KOKKOS_HAVE_PRAGMA_IVDEP
#pragma ivdep
#endif
#ifdef KOKKOS_HAVE_PRAGMA_VECTOR
#pragma vector always
#endif
    for (size_type j = 0; j < numVecs; ++j) {
      update[j] += source[j];
    }
  }

  // On device, write the reduction result to the output View.
  KOKKOS_INLINE_FUNCTION void
  final (const value_type dst) const
  {
    const size_type numVecs = value_count;
#ifdef KOKKOS_HAVE_PRAGMA_IVDEP
#pragma ivdep
#endif
#ifdef KOKKOS_HAVE_PRAGMA_VECTOR
#pragma vector always
#endif
    for (size_type j = 0; j < numVecs; ++j) {
      m_r(j) = dst[j];
    }
  }
};

//! Implementation of KokkosBlas::nrm2_squared for multivectors.
template<class RT, class RL, class RD, class RM, class RS,
         class XT, class XL, class XD, class XM, class XS>
struct Nrm2_MV {
  typedef Kokkos::View<RT,RL,RD,RM,RS> RV;
  typedef Kokkos::View<XT,XL,XD,XM,XS> XMV;
  typedef typename XMV::execution_space execution_space;
  typedef typename XMV::size_type size_type;

  /// \brief Compute the square of the 2-norm(s) of the column(s) of
  ///   the multivector (2-D View) X, and store result(s) in r.
  static void nrm2_squared (const RV& r, const XMV& X)
  {
    const size_type numRows = X.dimension_0 ();
    const size_type numCols = X.dimension_1 ();

    // int is generally faster than size_t, but check for overflow first.
    if (numRows < static_cast<size_type> (INT_MAX) &&
        numRows * numCols < static_cast<size_type> (INT_MAX)) {
      typedef MV_Nrm2Squared_Right_FunctorVector<RV, XMV, int> functor_type;
      Kokkos::RangePolicy<execution_space, int> policy (0, numRows);
      functor_type op (r, X);
      Kokkos::parallel_reduce (policy, op);
    }
    else {
      typedef MV_Nrm2Squared_Right_FunctorVector<RV, XMV, size_type> functor_type;
      Kokkos::RangePolicy<execution_space, int> policy (0, numRows);
      functor_type op (r, X);
      Kokkos::parallel_reduce (policy, op);
    }
  }
};

// Full specializations for cases of interest for Tpetra::MultiVector.
//
// Currently, we include specializations for Scalar = double,
// LayoutLeft (which is what Tpetra::MultiVector uses at the moment),
// and all execution spaces.  This may change in the future.  The
// output 1-D View _always_ uses the execution space's default array
// layout, which is what Tpetra::MultiVector wants for the output
// argument of norm2().

#ifdef KOKKOS_HAVE_SERIAL
template<>
struct Nrm2_MV<double*,
               Kokkos::Serial::array_layout,
               Kokkos::Device<Kokkos::Serial, Kokkos::HostSpace>,
               Kokkos::MemoryTraits<Kokkos::Unmanaged>,
               Kokkos::Impl::ViewDefault,
               const double**,
               Kokkos::LayoutLeft,
               Kokkos::Device<Kokkos::Serial, Kokkos::HostSpace>,
               Kokkos::MemoryTraits<Kokkos::Unmanaged>,
               Kokkos::Impl::ViewDefault> {
  typedef double* RT;
  typedef Kokkos::Serial::array_layout RL;
  typedef Kokkos::Device<Kokkos::Serial, Kokkos::HostSpace> RD;
  typedef Kokkos::MemoryTraits<Kokkos::Unmanaged> RM;
  typedef Kokkos::Impl::ViewDefault RS;
  typedef Kokkos::View<RT,RL,RD,RM,RS> RV;

  typedef const double** XT;
  typedef Kokkos::LayoutLeft XL;
  typedef Kokkos::Device<Kokkos::Serial, Kokkos::HostSpace> XD;
  typedef Kokkos::MemoryTraits<Kokkos::Unmanaged> XM;
  typedef Kokkos::Impl::ViewDefault XS;
  typedef Kokkos::View<XT,XL,XD,XM,XS> XMV;

  typedef XMV::execution_space execution_space;
  typedef XMV::size_type size_type;

  static void nrm2_squared (const RV& r, const XMV& X);
};
#endif // KOKKOS_HAVE_SERIAL

#ifdef KOKKOS_HAVE_OPENMP
template<>
struct Nrm2_MV<double*,
               Kokkos::OpenMP::array_layout,
               Kokkos::Device<Kokkos::OpenMP, Kokkos::HostSpace>,
               Kokkos::MemoryTraits<Kokkos::Unmanaged>,
               Kokkos::Impl::ViewDefault,
               const double**,
               Kokkos::LayoutLeft,
               Kokkos::Device<Kokkos::OpenMP, Kokkos::HostSpace>,
               Kokkos::MemoryTraits<Kokkos::Unmanaged>,
               Kokkos::Impl::ViewDefault> {
  typedef double* RT;
  typedef Kokkos::OpenMP::array_layout RL;
  typedef Kokkos::Device<Kokkos::OpenMP, Kokkos::HostSpace> RD;
  typedef Kokkos::MemoryTraits<Kokkos::Unmanaged> RM;
  typedef Kokkos::Impl::ViewDefault RS;
  typedef Kokkos::View<RT,RL,RD,RM,RS> RV;

  typedef const double** XT;
  typedef Kokkos::LayoutLeft XL;
  typedef Kokkos::Device<Kokkos::OpenMP, Kokkos::HostSpace> XD;
  typedef Kokkos::MemoryTraits<Kokkos::Unmanaged> XM;
  typedef Kokkos::Impl::ViewDefault XS;
  typedef Kokkos::View<XT,XL,XD,XM,XS> XMV;

  typedef XMV::execution_space execution_space;
  typedef XMV::size_type size_type;

  static void nrm2_squared (const RV& r, const XMV& X);
};
#endif // KOKKOS_HAVE_OPENMP

#ifdef KOKKOS_HAVE_PTHREAD
template<>
struct Nrm2_MV<double*,
               Kokkos::Threads::array_layout,
               Kokkos::Device<Kokkos::Threads, Kokkos::HostSpace>,
               Kokkos::MemoryTraits<Kokkos::Unmanaged>,
               Kokkos::Impl::ViewDefault,
               const double**,
               Kokkos::LayoutLeft,
               Kokkos::Device<Kokkos::Threads, Kokkos::HostSpace>,
               Kokkos::MemoryTraits<Kokkos::Unmanaged>,
               Kokkos::Impl::ViewDefault> {
  typedef double* RT;
  typedef Kokkos::Threads::array_layout RL;
  typedef Kokkos::Device<Kokkos::Threads, Kokkos::HostSpace> RD;
  typedef Kokkos::MemoryTraits<Kokkos::Unmanaged> RM;
  typedef Kokkos::Impl::ViewDefault RS;
  typedef Kokkos::View<RT,RL,RD,RM,RS> RV;

  typedef const double** XT;
  typedef Kokkos::LayoutLeft XL;
  typedef Kokkos::Device<Kokkos::Threads, Kokkos::HostSpace> XD;
  typedef Kokkos::MemoryTraits<Kokkos::Unmanaged> XM;
  typedef Kokkos::Impl::ViewDefault XS;
  typedef Kokkos::View<XT,XL,XD,XM,XS> XMV;

  typedef XMV::execution_space execution_space;
  typedef XMV::size_type size_type;

  static void nrm2_squared (const RV& r, const XMV& X);
};
#endif // KOKKOS_HAVE_PTHREAD

#ifdef KOKKOS_HAVE_CUDA
struct Nrm2_MV<double*,
               Kokkos::Cuda::array_layout,
               Kokkos::Device<Kokkos::Cuda, Kokkos::CudaSpace>,
               Kokkos::MemoryTraits<Kokkos::Unmanaged>,
               Kokkos::Impl::ViewDefault,
               const double**,
               Kokkos::LayoutLeft,
               Kokkos::Device<Kokkos::Cuda, Kokkos::CudaSpace>,
               Kokkos::MemoryTraits<Kokkos::Unmanaged>,
               Kokkos::Impl::ViewDefault> {
  typedef double* RT;
  typedef Kokkos::Cuda::array_layout RL;
  typedef Kokkos::Device<Kokkos::Cuda, Kokkos::CudaSpace> RD;
  typedef Kokkos::MemoryTraits<Kokkos::Unmanaged> RM;
  typedef Kokkos::Impl::ViewDefault RS;
  typedef Kokkos::View<RT,RL,RD,RM,RS> RV;

  typedef const double** XT;
  typedef Kokkos::LayoutLeft XL;
  typedef Kokkos::Device<Kokkos::Cuda, Kokkos::CudaSpace> XD;
  typedef Kokkos::MemoryTraits<Kokkos::Unmanaged> XM;
  typedef Kokkos::Impl::ViewDefault XS;
  typedef Kokkos::View<XT,XL,XD,XM,XS> XMV;

  typedef XMV::execution_space execution_space;
  typedef XMV::size_type size_type;

  static void nrm2_squared (const RV& r, const XMV& X);
};
#endif // KOKKOS_HAVE_CUDA

#ifdef KOKKOS_HAVE_CUDA
template<>
struct Nrm2_MV<double*,
               Kokkos::Cuda::array_layout,
               Kokkos::Device<Kokkos::Cuda, Kokkos::CudaUVMSpace>,
               Kokkos::MemoryTraits<Kokkos::Unmanaged>,
               Kokkos::Impl::ViewDefault,
               const double**,
               Kokkos::LayoutLeft,
               Kokkos::Device<Kokkos::Cuda, Kokkos::CudaUVMSpace>,
               Kokkos::MemoryTraits<Kokkos::Unmanaged>,
               Kokkos::Impl::ViewDefault> {
  typedef double* RT;
  typedef Kokkos::Cuda::array_layout RL;
  typedef Kokkos::Device<Kokkos::Cuda, Kokkos::CudaUVMSpace> RD;
  typedef Kokkos::MemoryTraits<Kokkos::Unmanaged> RM;
  typedef Kokkos::Impl::ViewDefault RS;
  typedef Kokkos::View<RT,RL,RD,RM,RS> RV;

  typedef const double** XT;
  typedef Kokkos::LayoutLeft XL;
  typedef Kokkos::Device<Kokkos::Cuda, Kokkos::CudaUVMSpace> XD;
  typedef Kokkos::MemoryTraits<Kokkos::Unmanaged> XM;
  typedef Kokkos::Impl::ViewDefault XS;
  typedef Kokkos::View<XT,XL,XD,XM,XS> XMV;

  typedef XMV::execution_space execution_space;
  typedef XMV::size_type size_type;

  static void nrm2_squared (const RV& r, const XMV& X);
};
#endif // KOKKOS_HAVE_CUDA

} // namespace Impl
} // namespace KokkosBlas

#endif // KOKKOS_BLAS1_MV_IMPL_HPP_