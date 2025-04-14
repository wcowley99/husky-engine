#pragma once

#include "vk_base.h"

#include "allocator.h"
#include "immediate_command.h"

#include <vector>

namespace Render {

class Buffer {
public:
  Buffer(Allocator &allocator, size_t size, vk::BufferUsageFlags flags,
         VmaMemoryUsage usage);
  ~Buffer();

  vk::Buffer &get_buffer();

  void *get_mapped_data();

  template <typename T>
  void copy_to(const std::vector<T> &data, size_t offset) {
    vmaCopyMemoryToAllocation(allocator.gpu_allocator, data.data(), allocation,
                              offset, data.size() * sizeof(T));
  }

private:
  Allocator &allocator;

  vk::Buffer buffer;
  VmaAllocation allocation;
  VmaAllocationInfo allocation_info;
};

class MeshBuffer {
public:
  MeshBuffer(Allocator &allocator, vk::Device &device,
             ImmediateCommand &immediate_command,
             const std::vector<uint32_t> &indices,
             const std::vector<Vertex> &vertices);

  vk::DeviceAddress get_vertex_buffer_address();

  Buffer &get_index_buffer();

private:
  static constexpr vk::BufferUsageFlags vertex_usage =
      vk::BufferUsageFlagBits::eStorageBuffer |
      vk::BufferUsageFlagBits::eTransferDst |
      vk::BufferUsageFlagBits::eShaderDeviceAddress;

  static constexpr vk::BufferUsageFlags index_usage =
      vk::BufferUsageFlagBits::eIndexBuffer |
      vk::BufferUsageFlagBits::eTransferDst;

private:
  Buffer vertex_buffer;
  Buffer index_buffer;
  vk::DeviceAddress vertex_address;
};

} // namespace Render
