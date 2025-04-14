#include "buffer.h"

namespace Render {

Buffer::Buffer(Allocator &allocator, size_t size, vk::BufferUsageFlags flags,
               VmaMemoryUsage usage)
    : allocator(allocator) {
  VkBufferCreateInfo buffer_info = {};
  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.pNext = nullptr;
  buffer_info.size = size;
  buffer_info.usage = static_cast<VkBufferUsageFlags>(flags);

  VmaAllocationCreateInfo alloc_info = {};
  alloc_info.usage = usage;
  alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

  VkBuffer buffer;
  VK_ASSERT(vmaCreateBuffer(allocator.gpu_allocator, &buffer_info, &alloc_info,
                            &buffer, &this->allocation,
                            &this->allocation_info));

  this->buffer = buffer;
}

Buffer::~Buffer() {
  vmaDestroyBuffer(allocator.gpu_allocator, buffer, allocation);
}

vk::Buffer &Buffer::get_buffer() { return this->buffer; }

// MeshBuffer
MeshBuffer::MeshBuffer(Allocator &allocator, vk::Device &device,
                       ImmediateCommand &immediate_command,
                       const std::vector<uint32_t> &indices,
                       const std::vector<Vertex> &vertices)
    : vertex_buffer(allocator, vertices.size() * sizeof(Vertex), vertex_usage,
                    VMA_MEMORY_USAGE_GPU_ONLY),
      index_buffer(allocator, indices.size() * sizeof(uint32_t), index_usage,
                   VMA_MEMORY_USAGE_GPU_ONLY) {
  vk::BufferDeviceAddressInfo address_info(vertex_buffer.get_buffer());
  this->vertex_address = device.getBufferAddress(&address_info);

  size_t vertex_size = vertices.size() * sizeof(Vertex);
  size_t index_size = indices.size() * sizeof(uint32_t);
  Buffer staging_buffer(allocator, vertex_size + index_size,
                        vk::BufferUsageFlagBits::eTransferSrc,
                        VMA_MEMORY_USAGE_CPU_ONLY);

  staging_buffer.copy_to(vertices, 0);
  staging_buffer.copy_to(indices, vertex_size);

  // todo - move staging buffer copy to background thread, reuse staging buffer
  immediate_command.submit([&](vk::CommandBuffer &command) {
    vk::BufferCopy vertex_copy(0, 0, vertex_size);
    vk::BufferCopy index_copy(vertex_size, 0, index_size);

    command.copyBuffer(staging_buffer.get_buffer(), vertex_buffer.get_buffer(),
                       1, &vertex_copy);
    command.copyBuffer(staging_buffer.get_buffer(), index_buffer.get_buffer(),
                       1, &index_copy);
  });
}

vk::DeviceAddress MeshBuffer::get_vertex_buffer_address() {
  return this->vertex_address;
}

Buffer &MeshBuffer::get_index_buffer() { return this->index_buffer; }

} // namespace Render
