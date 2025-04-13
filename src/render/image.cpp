#include "image.h"

#include <iostream>

namespace Render {

Image::Image(vk::Image image, vk::Extent2D extent, vk::Format format,
             vk::ImageLayout layout, vk::Device &device)
    : Image(image, vk::Extent3D(extent, 1), format, layout, device) {}

Image::Image(vk::Image image, vk::Extent3D extent, vk::Format format,
             vk::ImageLayout layout, vk::Device &device) {
  this->extent = extent;
  this->format = format;
  this->layout = layout;
  this->image = image;

  auto view_create_info = VkUtil::image_view_create_info(
      format, this->image, vk::ImageAspectFlagBits::eColor);

  this->image_view = device.createImageViewUnique(view_create_info);
}

AllocatedImage::AllocatedImage(vk::Image image, vk::Extent3D extent,
                               vk::Format format, vk::ImageLayout layout,
                               std::shared_ptr<Allocator> allocator,
                               VmaAllocation allocation, vk::Device &device)
    : Image(image, extent, format, layout, device) {
  this->allocator = allocator;
  this->allocation = allocation;
}

AllocatedImage::~AllocatedImage() {
  vmaDestroyImage(allocator->gpu_allocator, image, allocation);
}

ImageBuilder::ImageBuilder(vk::Extent2D extent)
    : ImageBuilder(vk::Extent3D(extent, 1)) {}

ImageBuilder::ImageBuilder(vk::Extent3D extent) { this->extent = extent; }

ImageBuilder &ImageBuilder::with_format(vk::Format format) {
  this->format = format;

  return *this;
}

ImageBuilder &ImageBuilder::with_usage_flags(vk::ImageUsageFlags usage_flags) {
  this->usage_flags = usage_flags;

  return *this;
}

ImageBuilder &ImageBuilder::with_memory_usage(VmaMemoryUsage usage) {
  this->memory_usage = usage;

  return *this;
}

ImageBuilder &
ImageBuilder::with_memory_properties(vk::MemoryPropertyFlags props) {
  this->memory_props = props;

  return *this;
}

std::unique_ptr<Image> ImageBuilder::build(std::shared_ptr<Allocator> allocator,
                                           vk::Device &device) {
  auto image_format = format.value_or(vk::Format::eR16G16B16A16Sfloat);
  vk::ImageCreateInfo create_info({}, vk::ImageType::e2D, image_format, extent,
                                  1, 1, vk::SampleCountFlagBits::e1,
                                  vk::ImageTiling::eOptimal, usage_flags);

  auto vulkan_h_create_info = static_cast<VkImageCreateInfo>(create_info);

  VmaAllocationCreateInfo alloc_info = {};
  alloc_info.usage = memory_usage;
  alloc_info.requiredFlags = static_cast<VkMemoryPropertyFlags>(memory_props);

  VkImage image;
  VmaAllocation allocation;

  vmaCreateImage(allocator->gpu_allocator, &vulkan_h_create_info, &alloc_info,
                 &image, &allocation, nullptr);

  return std::make_unique<AllocatedImage>(image, extent, image_format,
                                          vk::ImageLayout::eUndefined,
                                          allocator, allocation, device);
}

} // namespace Render
