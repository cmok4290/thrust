/*
 *  Copyright 2008-2011 NVIDIA Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */


/*! \file for_each.inl
 *  \brief Inline file for for_each.h.
 */

#include <thrust/detail/config.h>
#include <thrust/detail/minmax.h>
#include <thrust/detail/static_assert.h>

#include <thrust/detail/backend/dereference.h>
#include <thrust/detail/backend/cuda/detail/launch_closure.h>
#include <thrust/detail/backend/cuda/detail/launch_calculator.h>

#include <limits>

namespace thrust
{
namespace detail
{
namespace backend
{
namespace cuda
{


template<typename RandomAccessIterator,
         typename Size,
         typename UnaryFunction,
         typename Context>
struct for_each_n_closure
{
  typedef void result_type;
  typedef Context context_type;

  RandomAccessIterator first;
  Size n;
  UnaryFunction f;
  Context context;

  for_each_n_closure(RandomAccessIterator first,
                     Size n,
                     UnaryFunction f,
                     Context context = Context())
    : first(first), n(n), f(f), context(context)
  {}

  __device__ __forceinline__
  result_type operator()(void)
  {
    const Size grid_size = context.block_dimension() * context.grid_dimension();

    Size i = context.linear_index();

    // advance iterator
    first += i;

    while(i < n)
    {
      f(thrust::detail::backend::dereference(first));
      i += grid_size;
      first += grid_size;
    }
  }
}; // end or_each_n_closure


template<typename RandomAccessIterator,
         typename Size,
         typename UnaryFunction>
RandomAccessIterator for_each_n(RandomAccessIterator first,
                                Size n,
                                UnaryFunction f)
{
  // we're attempting to launch a kernel, assert we're compiling with nvcc
  // ========================================================================
  // X Note to the user: If you've found this line due to a compiler error, X
  // X you need to compile your code using nvcc, rather than g++ or cl.exe  X
  // ========================================================================
  THRUST_STATIC_ASSERT( (depend_on_instantiation<RandomAccessIterator, THRUST_DEVICE_COMPILER == THRUST_DEVICE_COMPILER_NVCC>::value) );

  if (n <= 0) return first;  //empty range
  
  if ((sizeof(Size) > sizeof(unsigned int))
       && n > Size((std::numeric_limits<unsigned int>::max)())) // convert to Size to avoid a warning
  {
    // n is large, must use 64-bit indices
    typedef cuda::detail::blocked_thread_array Context;
    typedef for_each_n_closure<RandomAccessIterator, Size, UnaryFunction, Context> Closure;
    Closure closure(first, n, f);

    // calculate launch configuration
    thrust::detail::backend::cuda::detail::launch_calculator<Closure> calculator;

    thrust::tuple<size_t, size_t, size_t> config = calculator.with_variable_block_size();
    size_t max_blocks = thrust::get<0>(config);
    size_t block_size = thrust::get<1>(config);
    size_t num_blocks = thrust::min(max_blocks, (static_cast<size_t>(n) + (block_size - 1)) / block_size);

    // launch closure
    thrust::detail::backend::cuda::detail::launch_closure(closure, num_blocks, block_size);
  }
  else
  {
    // n is small, 32-bit indices are sufficient
    typedef cuda::detail::blocked_thread_array Context;
    typedef for_each_n_closure<RandomAccessIterator, unsigned int, UnaryFunction,Context> Closure;
    Closure closure(first, static_cast<unsigned int>(n), f);
    
    // calculate launch configuration
    thrust::detail::backend::cuda::detail::launch_calculator<Closure> calculator;

    thrust::tuple<size_t, size_t, size_t> config = calculator.with_variable_block_size();
    size_t max_blocks = thrust::get<0>(config);
    size_t block_size = thrust::get<1>(config);
    size_t num_blocks = thrust::min(max_blocks, (static_cast<size_t>(n) + (block_size - 1)) / block_size);

    // launch closure
    thrust::detail::backend::cuda::detail::launch_closure(closure, num_blocks, block_size);
  }

  return first + n;
} 

} // end namespace cuda
} // end namespace backend
} // end namespace detail
} // end namespace thrust

