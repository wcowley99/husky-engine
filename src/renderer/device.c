#include "device.h"

#include "platform.h"
#include "vkb.h"

#include "husky.h"

#include <SDL3/SDL_vulkan.h>

VKAPI_ATTR VkBool32 VKAPI_CALL validation_message_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData) {
        printf("[Validation]: %s\n", pCallbackData->pMessage);

        return VK_FALSE;
}

bool create_instance(device_t *device) {
        const static char *APP_NAME = "Husky Engine";

        VkApplicationInfo app_info = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                      .pApplicationName = APP_NAME,
                                      .applicationVersion = VK_MAKE_VERSION(1, 3, 0),
                                      .pEngineName = "No Engine",
                                      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                                      .apiVersion = VK_API_VERSION_1_3};

        VkDebugUtilsMessengerCreateInfoEXT validation_callback = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = validation_message_callback,
        };

        uint32_t count = 0;
        const char *const *sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&count);

        printf("SDL Extensions:\n");
        for (uint32_t i = 0; i < count; i += 1) {
                printf("  - %s\n", sdl_extensions[i]);
        }

        const char **extensions = malloc(sizeof(const char *) * (count + 1));
        for (uint32_t i = 0; i < count; i += 1) {
                extensions[i] = sdl_extensions[i];
        }
        extensions[count] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

        const char *layers[] = {"VK_LAYER_KHRONOS_validation"};

        VkInstanceCreateInfo create_info = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                            .pApplicationInfo = &app_info,
                                            .enabledExtensionCount = count + 1,
                                            .ppEnabledExtensionNames = extensions,
                                            .enabledLayerCount = 1,
                                            .ppEnabledLayerNames = layers,
                                            .pNext = &validation_callback};

        VK_EXPECT(vkCreateInstance(&create_info, NULL, &device->instance));
        volkLoadInstance(device->instance);

        free(extensions);
        return true;
}

bool create_debug_messenger(device_t *device) {
        VkDebugUtilsMessengerCreateInfoEXT create_info = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = validation_message_callback,
        };

        VK_EXPECT(vkCreateDebugUtilsMessengerEXT(device->instance, &create_info, NULL,
                                                 &device->debug_messenger));
        return true;
}

bool is_gpu_suitable(device_t *device, VkPhysicalDevice gpu) {
        VkPhysicalDeviceProperties p;
        VkPhysicalDeviceVulkan12Features f12 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        VkPhysicalDeviceVulkan13Features f13 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES, .pNext = &f12};
        VkPhysicalDeviceFeatures2 f = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                                       .pNext = &f13};

        vkGetPhysicalDeviceProperties(gpu, &p);
        vkGetPhysicalDeviceFeatures2(gpu, &f);

        if (p.apiVersion < VK_API_VERSION_1_3) {
                return false;
        }

        if (!f12.bufferDeviceAddress || !f12.descriptorIndexing || !f13.dynamicRendering ||
            !f13.synchronization2 || !f.features.robustBufferAccess ||
            !f12.descriptorBindingPartiallyBound || !f12.runtimeDescriptorArray) {
                return false;
        }

        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, NULL);

        VkQueueFamilyProperties *properties = malloc(sizeof(VkQueueFamilyProperties) * count);
        vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, properties);

        for (uint32_t i = 0; i < count; i += 1) {
                VkBool32 support;
                VK_EXPECT(vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, device->surface, &support));

                if (support && properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                        device->queue_family_index = i;
                        printf("Selected Device: %s.\n", p.deviceName);
                }
        }

        free(properties);
        return true;
}

bool select_gpu(device_t *device) {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(device->instance, &count, NULL);

        if (count == 0) {
                printf("Failed to find GPUs with Vulkan support.\n");
                return false;
        }

        VkPhysicalDevice *gpus = malloc(sizeof(VkPhysicalDevice *) * count);
        vkEnumeratePhysicalDevices(device->instance, &count, gpus);

        for (uint32_t i = 0; i < count; i += 1) {
                if (is_gpu_suitable(device, gpus[i])) {
                        device->physical_device = gpus[i];
                        break;
                }
        }

        free(gpus);
        return true;
}

bool create_logical_device(device_t *device) {
        float priorities[] = {1.0f};
        VkDeviceQueueCreateInfo queue_info = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = device->queue_family_index,
            .queueCount = 1,
            .pQueuePriorities = priorities,
        };

        const char *extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        VkPhysicalDeviceVulkan12Features f12 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .bufferDeviceAddress = VK_TRUE,
            .descriptorIndexing = VK_TRUE,
            .runtimeDescriptorArray = VK_TRUE,
            .descriptorBindingPartiallyBound = VK_TRUE,
            .descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
            .descriptorBindingVariableDescriptorCount = VK_TRUE,
            .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
        };
        VkPhysicalDeviceVulkan13Features f13 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
            .dynamicRendering = VK_TRUE,
            .synchronization2 = VK_TRUE,
            .pNext = &f12,
        };
        VkPhysicalDeviceFeatures2 f = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                                       .features.robustBufferAccess = VK_TRUE,
                                       .pNext = &f13};

        VkDeviceCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pQueueCreateInfos = &queue_info,
            .queueCreateInfoCount = 1,
            .ppEnabledExtensionNames = extensions,
            .enabledExtensionCount = 1,
            .pNext = &f,
        };

        VK_EXPECT(vkCreateDevice(device->physical_device, &create_info, NULL, &device->device));
        volkLoadDevice(device->device);

        vkGetDeviceQueue(device->device, device->queue_family_index, 0, &device->graphics_queue);

        return true;
}

device_t device_create() {
        device_t device = {0};

        ASSERT(create_instance(&device));
        ASSERT(create_debug_messenger(&device));

        device.surface = platform_create_surface(device.instance);

        ASSERT(select_gpu(&device));
        ASSERT(create_logical_device(&device));

        return device;
}

void device_shutdown(device_t *device) {
        vkDestroyDevice(device->device, NULL);

        vkDestroySurfaceKHR(device->instance, device->surface, NULL);
        vkDestroyDebugUtilsMessengerEXT(device->instance, device->debug_messenger, NULL);
        vkDestroyInstance(device->instance, NULL);
}
