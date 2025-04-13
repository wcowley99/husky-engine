#pragma once

#include "vk_base.h"

#include "allocator.h"

#include <memory>
#include <optional>

namespace Render {

class Image {
public:
  Image(vk::Image image, vk::Extent2D extent, vk::Format format,
        vk::ImageLayout layout, vk::Device &device);
  Image(vk::Image image, vk::Extent3D extent, vk::Format format,
        vk::ImageLayout layout, vk::Device &device);
  virtual ~Image() = default;

public:
  vk::Image image;
  vk::UniqueImageView image_view;
  vk::Extent3D extent;
  vk::Format format;
  vk::ImageLayout layout;
};

class AllocatedImage : public Image {
public:
  AllocatedImage(vk::Image image, vk::Extent3D extent, vk::Format format,
                 vk::ImageLayout layout, std::shared_ptr<Allocator> allocator,
                 VmaAllocation allocation, vk::Device &device);
  ~AllocatedImage();

private:
  VmaAllocation allocation;
  std::shared_ptr<Allocator> allocator;
};

class ImageBuilder {
public:
  ImageBuilder(vk::Extent2D extent);
  ImageBuilder(vk::Extent3D extent);

  ImageBuilder &with_format(vk::Format format);
  ImageBuilder &with_usage_flags(vk::ImageUsageFlags usage_flags);

  ImageBuilder &with_memory_usage(VmaMemoryUsage usage);
  ImageBuilder &with_memory_properties(vk::MemoryPropertyFlags props);

  std::unique_ptr<Image> build(std::shared_ptr<Allocator> allocator,
                               vk::Device &device);

private:
  vk::Extent3D extent;
  std::optional<vk::Format> format;
  vk::ImageUsageFlags usage_flags;
  VmaMemoryUsage memory_usage;
  vk::MemoryPropertyFlags memory_props;
};

} // namespace Render
