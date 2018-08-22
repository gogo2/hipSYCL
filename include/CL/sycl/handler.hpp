/*
 * This file is part of SYCU, a SYCL implementation based CUDA/HIP
 *
 * Copyright (c) 2018 Aksel Alpay
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef SYCU_HANDLER_HPP
#define SYCU_HANDLER_HPP

#include "access.hpp"
#include "accessor.hpp"
#include "backend/backend.hpp"
#include "types.hpp"
#include "id.hpp"
#include "range.hpp"
#include "nd_range.hpp"
#include "item.hpp"
#include "nd_item.hpp"
#include "group.hpp"
#include "detail/local_memory_allocator.hpp"

namespace cl {
namespace sycl {

namespace detail {
namespace dispatch {


template<class Function>
__global__ void single_task_kernel(Function f)
{
  f();
}

template<int dimensions, class Function>
__global__ void parallel_for_kernel(Function f,
                                    sycl::range<dimensions> execution_range)
{
  item<dimensions, false> this_item;
  if(this_item.get_linear_id() < execution_range.size())
    f(this_item);
}

template<int dimensions, class Function>
__global__ void parallel_for_kernel_with_offset(Function f,
                                                sycl::range<dimensions> execution_range,
                                                id<dimensions> offset)
{
  item<dimensions> this_item{offset};

  bool item_is_in_range = true;
  for(int i = 0; i < dimensions; ++i)
  {
    if(this_item.get_id(i) >= offset.get(i) + execution_range.get(i))
    {
      item_is_in_range = false;
      break;
    }
  }

  if(item_is_in_range)
    f(this_item);
}

template<int dimensions, class Function>
__global__ void parallel_for_ndrange_kernel(Function f, id<dimensions> offset)
{
  nd_item<dimensions> this_item{&offset};
  f(this_item);
}

template<int dimensions, class Function>
__global__ void parallel_for_workgroup(Function f,
                                       sycl::range<dimensions> work_group_size)
{
  group<dimensions> this_group;
  f(this_group);
}

} // dispatch
} // detail

class queue;

class handler
{
  friend class queue;
  const queue* _queue;
  detail::local_memory_allocator _local_mem_allocator;

  handler(const queue& q);

public:

  template <typename dataT, int dimensions, access::mode accessMode,
            access::target accessTarget>
  void require(accessor<dataT, dimensions, accessMode, accessTarget,
               access::placeholder::true_t> acc)
  {

  }

  //----- OpenCL interoperability interface is not supported
  /*

template <typename T>
void set_arg(int argIndex, T && arg);

template <typename... Ts>
void set_args(Ts &&... args);
*/
  //------ Kernel dispatch API


  template <typename KernelName, typename KernelType>
  void single_task(KernelType kernelFunc)
  {
    std::size_t shared_mem_size = 0;
    hipStream_t stream = this->get_execution_stream();

    detail::dispatch::single_task_kernel<<<1,1,shared_mem_size,stream>>>(kernelFunc);
  }

  template <typename KernelName, typename KernelType, int dimensions>
  void parallel_for(range<dimensions> numWorkItems, KernelType kernelFunc)
  {
    dispatch_kernel_without_offset(numWorkItems, kernelFunc);
  }

  template <typename KernelName, typename KernelType, int dimensions>
  void parallel_for(range<dimensions> numWorkItems,
                    id<dimensions> workItemOffset, KernelType kernelFunc)
  {
    dispatch_kernel_with_offset(numWorkItems, workItemOffset, kernelFunc);
  }

  template <typename KernelName, typename KernelType, int dimensions>
  void parallel_for(nd_range<dimensions> executionRange, KernelType kernelFunc)
  {
    dispatch_ndrange_kernel(executionRange, kernelFunc);
  }


  // Hierarchical kernel dispatch API

  /// \todo flexible ranges are currently unsupported
  /*
  template <typename KernelName, typename WorkgroupFunctionType, int dimensions>
  void parallel_for_work_group(range<dimensions> numWorkGroups,
                               WorkgroupFunctionType kernelFunc)
  {
    dispatch_hierarchical_kernel(numWorkGroups,
                                 get_default_local_range<dimensions>(),
                                 kernelFunc);
  }
  */

  template <typename KernelName, typename WorkgroupFunctionType, int dimensions>
  void parallel_for_work_group(range<dimensions> numWorkGroups,
                               range<dimensions> workGroupSize,
                               WorkgroupFunctionType kernelFunc)
  {
    dispatch_hierarchical_kernel(numWorkGroups,
                                 workGroupSize,
                                 kernelFunc);
  }


  /*
  void single_task(kernel syclKernel);

  template <int dimensions>
  void parallel_for(range<dimensions> numWorkItems, kernel syclKernel);

  template <int dimensions>
  void parallel_for(range<dimensions> numWorkItems,
                    id<dimensions> workItemOffset, kernel syclKernel);

  template <int dimensions>
  void parallel_for(nd_range<dimensions> ndRange, kernel syclKernel);
  */

  //------ Explicit copy operations API

  template <typename T, int dim, access::mode mode, access::target tgt>
  void copy(accessor<T, dim, mode, tgt> src, shared_ptr_class<T> dest);

  template <typename T, int dim, access::mode mode, access::target tgt>
  void copy(shared_ptr_class<T> src, accessor<T, dim, mode, tgt> dest);

  template <typename T, int dim, access::mode mode, access::target tgt>
  void copy(accessor<T, dim, mode, tgt> src, T * dest);

  template <typename T, int dim, access::mode mode, access::target tgt>
  void copy(const T * src, accessor<T, dim, mode, tgt> dest);

  template <typename T, int dim, access::mode mode, access::target tgt>
  void copy(accessor<T, dim, mode, tgt> src, accessor<T, dim, mode, tgt> dest);

  template <typename T, int dim, access::mode mode, access::target tgt>
  void update_host(accessor<T, dim, mode, tgt> acc);

  template<typename T, int dim, access::mode mode, access::target tgt>
  void fill(accessor<T, dim, mode, tgt> dest, const T& src);

  hipStream_t get_execution_stream() const;

  detail::local_memory_allocator& get_local_memory_allocator()
  {
    return _local_mem_allocator;
  }
private:

  void select_device() const;

  template<int dimensions>
  dim3 get_default_local_range() const
  {
    if(dimensions == 1)
      return dim3(128);
    else if(dimensions == 2)
      return dim3(16,16);
    else if(dimensions == 3)
      return dim3(8,8,8);

    return dim3(1);
  }

  std::size_t ceil_division(std::size_t n,
                           std::size_t divisor) const
  {
    return (n + divisor - 1) / divisor;
  }

  template<int dimensions>
  dim3 range_to_dim3(const range<dimensions>& r) const
  {
    if(dimensions == 1)
      return dim3(r.get(0));
    else if(dimensions == 2)
      return dim3(r.get(0), r.get(1));
    else if(dimensions == 3)
      return dim3(r.get(0), r.get(1), r.get(2));

    return dim3(1);
  }

  template<int dimensions>
  void determine_grid_configuration(const range<dimensions>& num_work_items,
                                    dim3& grid,
                                    dim3& block) const
  {
    block = get_default_local_range<dimensions>();

    if(dimensions == 1)
      grid = dim3(ceil_division(num_work_items.get(0), block.x));
    else if (dimensions == 2)
      grid = dim3(ceil_division(num_work_items.get(0), block.x),
                  ceil_division(num_work_items.get(1), block.y));
    else if (dimensions == 3)
      grid = dim3(ceil_division(num_work_items.get(0), block.x),
                  ceil_division(num_work_items.get(1), block.y),
                  ceil_division(num_work_items.get(2), block.z));
    else
      grid = dim3(1);
  }


  template <typename KernelType, int dimensions>
  void dispatch_kernel_without_offset(range<dimensions> numWorkItems,
                                      KernelType kernelFunc)
  {
    this->select_device();

    std::size_t shared_mem_size =
        _local_mem_allocator.get_allocation_size();

    hipStream_t stream = this->get_execution_stream();

    dim3 grid, block;
    determine_grid_configuration(numWorkItems, grid, block);

    detail::dispatch::parallel_for_kernel
        <<<grid,block,shared_mem_size,stream>>>(kernelFunc,
                                                numWorkItems);
  }

  template <typename KernelType, int dimensions>
  void dispatch_kernel_with_offset(range<dimensions> numWorkItems,
                                   id<dimensions> offset,
                                   KernelType kernelFunc)
  {
    this->select_device();

    std::size_t shared_mem_size =
        _local_mem_allocator.get_allocation_size();

    hipStream_t stream = this->get_execution_stream();

    dim3 grid, block;
    determine_grid_configuration(numWorkItems, grid, block);

    detail::dispatch::parallel_for_kernel_with_offset
        <<<grid,block,shared_mem_size,stream>>>(kernelFunc,
                                                numWorkItems,
                                                offset);
  }


  template <typename KernelType, int dimensions>
  void dispatch_ndrange_kernel(nd_range<dimensions> executionRange, KernelType kernelFunc)
  {
    for(int i = 0; i < dimensions; ++i)
    {
      if(executionRange.get_global()[i] % executionRange.get_local()[i] != 0)
        throw invalid_parameter_error{"Global size must be a multiple of the local size"};
    }

    this->select_device();

    std::size_t shared_mem_size =
        _local_mem_allocator.get_allocation_size();

    hipStream_t stream = this->get_execution_stream();

    id<dimensions> offset = executionRange.get_offset();
    range<dimensions> grid_range = executionRange.get_group();
    range<dimensions> block_range = executionRange.get_local();

    dim3 grid = range_to_dim3(grid_range);
    dim3 block = range_to_dim3(block_range);

    detail::dispatch::parallel_for_ndrange_kernel
        <<<grid,block,shared_mem_size,stream>>>(kernelFunc, offset);
  }

  template <typename WorkgroupFunctionType, int dimensions>
  void dispatch_hierarchical_kernel(range<dimensions> numWorkGroups,
                                    range<dimensions> workGroupSize,
                                    WorkgroupFunctionType kernelFunc)
  {

    std::size_t shared_mem_size =
        _local_mem_allocator.get_allocation_size();

    hipStream_t stream = this->get_execution_stream();

    dim3 grid = range_to_dim3(numWorkGroups);
    dim3 block = range_to_dim3(workGroupSize);

    detail::dispatch::parallel_for_workgroup
        <<<grid,block,shared_mem_size,stream>>>(kernelFunc, workGroupSize);
  }

};

namespace detail {
namespace handler {


template<class T>
detail::local_memory::address allocate_local_mem(cl::sycl::handler& cgh,
                                                 size_t num_elements)
{
  return cgh.get_local_memory_allocator().alloc<T>(num_elements);
}

}
}

} // namespace sycl
} // namespace cl

#endif