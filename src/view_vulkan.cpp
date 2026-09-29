#include "view_vulkan.h"
#include <algorithm>
#include <cstring>
#include <utility>
#include <vector>

#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

using namespace Steinberg;

#define WINEINFO_VULKAN_FUNCTIONS(X)                                           \
  X(vkDestroyInstance)                                                         \
  X(vkEnumeratePhysicalDevices)                                                \
  X(vkGetPhysicalDeviceQueueFamilyProperties)                                  \
  X(vkGetPhysicalDeviceMemoryProperties)                                       \
  X(vkGetPhysicalDeviceSurfaceSupportKHR)                                      \
  X(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)                                 \
  X(vkGetPhysicalDeviceSurfaceFormatsKHR)                                      \
  X(vkCreateWin32SurfaceKHR)                                                   \
  X(vkDestroySurfaceKHR)                                                       \
  X(vkCreateDevice)                                                            \
  X(vkDestroyDevice)                                                           \
  X(vkDeviceWaitIdle)                                                          \
  X(vkGetDeviceQueue)                                                          \
  X(vkCreateSwapchainKHR)                                                      \
  X(vkDestroySwapchainKHR)                                                     \
  X(vkGetSwapchainImagesKHR)                                                   \
  X(vkAcquireNextImageKHR)                                                     \
  X(vkQueuePresentKHR)                                                         \
  X(vkQueueSubmit)                                                             \
  X(vkCreateCommandPool)                                                       \
  X(vkDestroyCommandPool)                                                      \
  X(vkAllocateCommandBuffers)                                                  \
  X(vkResetCommandBuffer)                                                      \
  X(vkBeginCommandBuffer)                                                      \
  X(vkEndCommandBuffer)                                                        \
  X(vkCmdPipelineBarrier)                                                      \
  X(vkCmdCopyBufferToImage)                                                    \
  X(vkCreateBuffer)                                                            \
  X(vkDestroyBuffer)                                                           \
  X(vkGetBufferMemoryRequirements)                                             \
  X(vkAllocateMemory)                                                          \
  X(vkFreeMemory)                                                              \
  X(vkBindBufferMemory)                                                        \
  X(vkMapMemory)                                                               \
  X(vkCreateSemaphore)                                                         \
  X(vkDestroySemaphore)                                                        \
  X(vkCreateFence)                                                             \
  X(vkDestroyFence)                                                            \
  X(vkWaitForFences)                                                           \
  X(vkResetFences)

namespace WineInfo {

class VulkanPresenter {
public:
  static std::unique_ptr<VulkanPresenter> create(HWND hwnd);
  ~VulkanPresenter();

  bool present(const void *pixels, uint32_t width, uint32_t height);

private:
  VulkanPresenter() = default;
  bool initialize(HWND hwnd);
  bool selectQueueFamily();
  bool createDevice();
  bool createSwapchain(uint32_t width, uint32_t height);
  void destroySwapchain();
  bool reserveStagingBuffer(VkDeviceSize size);
  void destroyStagingBuffer();
  bool acquireImage(uint32_t width, uint32_t height, uint32_t &imageIndex);
  void recordCopy(uint32_t imageIndex, uint32_t width, uint32_t height);

  HMODULE library_ = nullptr;
  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;
#define WINEINFO_DECLARE_VULKAN_FUNCTION(name) PFN_##name name = nullptr;
  WINEINFO_VULKAN_FUNCTIONS(WINEINFO_DECLARE_VULKAN_FUNCTION)
#undef WINEINFO_DECLARE_VULKAN_FUNCTION

  VkInstance instance_ = VK_NULL_HANDLE;
  VkSurfaceKHR surface_ = VK_NULL_HANDLE;
  VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
  uint32_t queueFamily_ = 0;
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue queue_ = VK_NULL_HANDLE;
  VkCommandPool commandPool_ = VK_NULL_HANDLE;
  VkCommandBuffer commandBuffer_ = VK_NULL_HANDLE;
  VkFence frameFence_ = VK_NULL_HANDLE;
  VkSemaphore imageAcquired_ = VK_NULL_HANDLE;

  VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
  VkExtent2D swapchainExtent_{};
  std::vector<VkImage> swapchainImages_;
  std::vector<VkSemaphore> copyFinished_;

  VkBuffer stagingBuffer_ = VK_NULL_HANDLE;
  VkDeviceMemory stagingMemory_ = VK_NULL_HANDLE;
  VkDeviceSize stagingSize_ = 0;
  void *stagingPixels_ = nullptr;
};

std::unique_ptr<VulkanPresenter> VulkanPresenter::create(HWND hwnd) {
  std::unique_ptr<VulkanPresenter> presenter(new VulkanPresenter());
  if (!presenter->initialize(hwnd))
    return nullptr;
  return presenter;
}

VulkanPresenter::~VulkanPresenter() {
  if (device_) {
    vkDeviceWaitIdle(device_);
    destroySwapchain();
    destroyStagingBuffer();
    vkDestroySemaphore(device_, imageAcquired_, nullptr);
    vkDestroyFence(device_, frameFence_, nullptr);
    vkDestroyCommandPool(device_, commandPool_, nullptr);
    vkDestroyDevice(device_, nullptr);
  }
  if (surface_)
    vkDestroySurfaceKHR(instance_, surface_, nullptr);
  if (instance_ && vkDestroyInstance)
    vkDestroyInstance(instance_, nullptr);
  if (library_)
    FreeLibrary(library_);
}

bool VulkanPresenter::initialize(HWND hwnd) {
  library_ = LoadLibraryW(L"vulkan-1.dll");
  if (!library_)
    return false;
  vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
      GetProcAddress(library_, "vkGetInstanceProcAddr"));
  if (!vkGetInstanceProcAddr)
    return false;
  auto vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(
      vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance"));
  if (!vkCreateInstance)
    return false;

  const char *extensions[] = {VK_KHR_SURFACE_EXTENSION_NAME,
                              VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
  VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO};
  application.pApplicationName = "WineInfo";
  application.apiVersion = VK_API_VERSION_1_0;
  VkInstanceCreateInfo instanceInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  instanceInfo.pApplicationInfo = &application;
  instanceInfo.enabledExtensionCount = 2;
  instanceInfo.ppEnabledExtensionNames = extensions;
  if (vkCreateInstance(&instanceInfo, nullptr, &instance_) != VK_SUCCESS)
    return false;

  bool loaded = true;
#define WINEINFO_LOAD_VULKAN_FUNCTION(name)                                    \
  name =                                                                       \
      reinterpret_cast<PFN_##name>(vkGetInstanceProcAddr(instance_, #name));   \
  loaded &= name != nullptr;
  WINEINFO_VULKAN_FUNCTIONS(WINEINFO_LOAD_VULKAN_FUNCTION)
#undef WINEINFO_LOAD_VULKAN_FUNCTION
  if (!loaded)
    return false;

  VkWin32SurfaceCreateInfoKHR surfaceInfo{
      VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
  surfaceInfo.hinstance =
      reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwnd, GWLP_HINSTANCE));
  surfaceInfo.hwnd = hwnd;
  if (vkCreateWin32SurfaceKHR(instance_, &surfaceInfo, nullptr, &surface_) !=
      VK_SUCCESS)
    return false;

  return selectQueueFamily() && createDevice();
}

bool VulkanPresenter::selectQueueFamily() {
  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

  for (VkPhysicalDevice device : devices) {
    uint32_t familyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(familyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount,
                                             families.data());
    for (uint32_t family = 0; family < familyCount; ++family) {
      VkBool32 presentable = VK_FALSE;
      vkGetPhysicalDeviceSurfaceSupportKHR(device, family, surface_,
                                           &presentable);
      if (presentable && families[family].queueCount > 0) {
        physicalDevice_ = device;
        queueFamily_ = family;
        return true;
      }
    }
  }
  return false;
}

bool VulkanPresenter::createDevice() {
  const float priority = 1.0f;
  VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
  queueInfo.queueFamilyIndex = queueFamily_;
  queueInfo.queueCount = 1;
  queueInfo.pQueuePriorities = &priority;
  const char *extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
  VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
  deviceInfo.queueCreateInfoCount = 1;
  deviceInfo.pQueueCreateInfos = &queueInfo;
  deviceInfo.enabledExtensionCount = 1;
  deviceInfo.ppEnabledExtensionNames = extensions;
  if (vkCreateDevice(physicalDevice_, &deviceInfo, nullptr, &device_) !=
      VK_SUCCESS)
    return false;
  vkGetDeviceQueue(device_, queueFamily_, 0, &queue_);

  VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = queueFamily_;
  if (vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_) !=
      VK_SUCCESS)
    return false;
  VkCommandBufferAllocateInfo bufferInfo{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  bufferInfo.commandPool = commandPool_;
  bufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  bufferInfo.commandBufferCount = 1;
  if (vkAllocateCommandBuffers(device_, &bufferInfo, &commandBuffer_) !=
      VK_SUCCESS)
    return false;

  VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  return vkCreateFence(device_, &fenceInfo, nullptr, &frameFence_) ==
             VK_SUCCESS &&
         vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &imageAcquired_) ==
             VK_SUCCESS;
}

bool VulkanPresenter::createSwapchain(uint32_t width, uint32_t height) {
  VkSurfaceCapabilitiesKHR capabilities{};
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface_,
                                            &capabilities);
  if (!(capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
    return false;

  VkExtent2D extent = capabilities.currentExtent;
  if (extent.width == UINT32_MAX) {
    extent.width = std::clamp(width, capabilities.minImageExtent.width,
                              capabilities.maxImageExtent.width);
    extent.height = std::clamp(height, capabilities.minImageExtent.height,
                               capabilities.maxImageExtent.height);
  }
  if (extent.width == 0 || extent.height == 0)
    return false;

  uint32_t formatCount = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount,
                                       nullptr);
  std::vector<VkSurfaceFormatKHR> formats(formatCount);
  vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount,
                                       formats.data());
  auto format = std::find_if(formats.begin(), formats.end(),
                             [](const VkSurfaceFormatKHR &f) {
                               return f.format == VK_FORMAT_B8G8R8A8_UNORM ||
                                      f.format == VK_FORMAT_B8G8R8A8_SRGB;
                             });
  if (format == formats.end())
    return false;

  uint32_t imageCount = capabilities.minImageCount + 1;
  if (capabilities.maxImageCount > 0)
    imageCount = std::min(imageCount, capabilities.maxImageCount);

  VkSwapchainCreateInfoKHR swapchainInfo{
      VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
  swapchainInfo.surface = surface_;
  swapchainInfo.minImageCount = imageCount;
  swapchainInfo.imageFormat = format->format;
  swapchainInfo.imageColorSpace = format->colorSpace;
  swapchainInfo.imageExtent = extent;
  swapchainInfo.imageArrayLayers = 1;
  swapchainInfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  swapchainInfo.preTransform = capabilities.currentTransform;
  swapchainInfo.compositeAlpha =
      (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
          ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
          : VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
  swapchainInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
  swapchainInfo.clipped = VK_TRUE;
  if (vkCreateSwapchainKHR(device_, &swapchainInfo, nullptr, &swapchain_) !=
      VK_SUCCESS)
    return false;
  swapchainExtent_ = extent;

  uint32_t swapchainImageCount = 0;
  vkGetSwapchainImagesKHR(device_, swapchain_, &swapchainImageCount, nullptr);
  swapchainImages_.resize(swapchainImageCount);
  vkGetSwapchainImagesKHR(device_, swapchain_, &swapchainImageCount,
                          swapchainImages_.data());

  VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  copyFinished_.assign(swapchainImageCount, VK_NULL_HANDLE);
  for (VkSemaphore &semaphore : copyFinished_) {
    if (vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &semaphore) !=
        VK_SUCCESS)
      return false;
  }
  return true;
}

void VulkanPresenter::destroySwapchain() {
  vkDeviceWaitIdle(device_);
  for (VkSemaphore semaphore : copyFinished_)
    vkDestroySemaphore(device_, semaphore, nullptr);
  copyFinished_.clear();
  swapchainImages_.clear();
  vkDestroySwapchainKHR(device_, swapchain_, nullptr);
  swapchain_ = VK_NULL_HANDLE;
}

bool VulkanPresenter::reserveStagingBuffer(VkDeviceSize size) {
  if (size <= stagingSize_)
    return true;
  destroyStagingBuffer();

  VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bufferInfo.size = size;
  bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  if (vkCreateBuffer(device_, &bufferInfo, nullptr, &stagingBuffer_) !=
      VK_SUCCESS)
    return false;

  VkMemoryRequirements requirements{};
  vkGetBufferMemoryRequirements(device_, stagingBuffer_, &requirements);
  VkPhysicalDeviceMemoryProperties memory{};
  vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memory);
  const VkMemoryPropertyFlags wanted = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  uint32_t type = 0;
  while (type < memory.memoryTypeCount &&
         !((requirements.memoryTypeBits & (1u << type)) &&
           (memory.memoryTypes[type].propertyFlags & wanted) == wanted))
    ++type;
  if (type == memory.memoryTypeCount)
    return false;

  VkMemoryAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  allocateInfo.allocationSize = requirements.size;
  allocateInfo.memoryTypeIndex = type;
  if (vkAllocateMemory(device_, &allocateInfo, nullptr, &stagingMemory_) !=
          VK_SUCCESS ||
      vkBindBufferMemory(device_, stagingBuffer_, stagingMemory_, 0) !=
          VK_SUCCESS ||
      vkMapMemory(device_, stagingMemory_, 0, size, 0, &stagingPixels_) !=
          VK_SUCCESS)
    return false;
  stagingSize_ = size;
  return true;
}

void VulkanPresenter::destroyStagingBuffer() {
  vkDestroyBuffer(device_, stagingBuffer_, nullptr);
  vkFreeMemory(device_, stagingMemory_, nullptr);
  stagingBuffer_ = VK_NULL_HANDLE;
  stagingMemory_ = VK_NULL_HANDLE;
  stagingSize_ = 0;
  stagingPixels_ = nullptr;
}

bool VulkanPresenter::acquireImage(uint32_t width, uint32_t height,
                                   uint32_t &imageIndex) {
  for (int attempt = 0; attempt < 2; ++attempt) {
    if (swapchain_ &&
        (swapchainExtent_.width != width || swapchainExtent_.height != height))
      destroySwapchain();
    if (!swapchain_ && !createSwapchain(width, height))
      return false;

    VkResult result =
        vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, imageAcquired_,
                              VK_NULL_HANDLE, &imageIndex);
    if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR)
      return true;
    if (result != VK_ERROR_OUT_OF_DATE_KHR)
      return false;
    destroySwapchain();
  }
  return false;
}

void VulkanPresenter::recordCopy(uint32_t imageIndex, uint32_t width,
                                 uint32_t height) {
  VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = swapchainImages_[imageIndex];
  barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

  VkCommandBufferBeginInfo beginInfo{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkResetCommandBuffer(commandBuffer_, 0);
  vkBeginCommandBuffer(commandBuffer_, &beginInfo);

  barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.srcAccessMask = 0;
  barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  vkCmdPipelineBarrier(commandBuffer_, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  VkBufferImageCopy region{};
  region.bufferRowLength = width;
  region.bufferImageHeight = height;
  region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  region.imageExtent = {std::min(width, swapchainExtent_.width),
                        std::min(height, swapchainExtent_.height), 1};
  vkCmdCopyBufferToImage(commandBuffer_, stagingBuffer_, barrier.image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = 0;
  vkCmdPipelineBarrier(commandBuffer_, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  vkEndCommandBuffer(commandBuffer_);
}

bool VulkanPresenter::present(const void *pixels, uint32_t width,
                              uint32_t height) {
  vkWaitForFences(device_, 1, &frameFence_, VK_TRUE, UINT64_MAX);

  const VkDeviceSize size = VkDeviceSize{width} * height * 4;
  uint32_t imageIndex = 0;
  if (!reserveStagingBuffer(size) || !acquireImage(width, height, imageIndex))
    return false;
  std::memcpy(stagingPixels_, pixels, static_cast<size_t>(size));
  recordCopy(imageIndex, width, height);

  const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = &imageAcquired_;
  submitInfo.pWaitDstStageMask = &waitStage;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer_;
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = &copyFinished_[imageIndex];
  vkResetFences(device_, 1, &frameFence_);
  if (vkQueueSubmit(queue_, 1, &submitInfo, frameFence_) != VK_SUCCESS)
    return false;

  VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = &copyFinished_[imageIndex];
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = &swapchain_;
  presentInfo.pImageIndices = &imageIndex;
  VkResult result = vkQueuePresentKHR(queue_, &presentInfo);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    destroySwapchain();
  return result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR;
}

static constexpr wchar_t kWindowClassName[] = L"WineInfoVST3VulkanView";
static const ViewRect kInitialRect{0, 0, 560, 150};

static HINSTANCE thisModule() {
  HMODULE module = nullptr;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                         GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     reinterpret_cast<LPCWSTR>(&thisModule), &module);
  return module;
}

ViewVulkan::ViewVulkan(InfoStrings info)
    : CPluginView(&kInitialRect), info_(std::move(info)) {}

ViewVulkan::~ViewVulkan() = default;

tresult PLUGIN_API ViewVulkan::isPlatformTypeSupported(FIDString type) {
  return (type && std::strcmp(type, kPlatformTypeHWND) == 0) ? kResultTrue
                                                             : kResultFalse;
}

tresult PLUGIN_API ViewVulkan::attached(void *parent, FIDString type) {
  if (!parent || isPlatformTypeSupported(type) != kResultTrue)
    return kResultFalse;

  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = wndProc;
  wc.hInstance = thisModule();
  wc.lpszClassName = kWindowClassName;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  RegisterClassExW(&wc);

  hwnd_ = CreateWindowExW(
      0, kWindowClassName, L"", WS_CHILD | WS_VISIBLE, 0, 0, rect.getWidth(),
      rect.getHeight(), static_cast<HWND>(parent), nullptr, thisModule(), this);
  if (!hwnd_)
    return kResultFalse;
  presenter_ = VulkanPresenter::create(hwnd_);
  if (!presenter_) {
    removed();
    return kResultFalse;
  }
  return CPluginView::attached(parent, type);
}

tresult PLUGIN_API ViewVulkan::removed() {
  presenter_.reset();
  if (hwnd_) {
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
  }
  // Classes registered by a DLL outlive its unloading, which would leave the
  // class pointing at a stale wndProc. This fails harmlessly while another
  // instance still has a window of this class.
  UnregisterClassW(kWindowClassName, thisModule());
  return CPluginView::removed();
}

tresult PLUGIN_API ViewVulkan::onSize(ViewRect *newSize) {
  tresult result = CPluginView::onSize(newSize);
  if (result == kResultTrue && hwnd_) {
    SetWindowPos(hwnd_, nullptr, 0, 0, rect.getWidth(), rect.getHeight(),
                 SWP_NOZORDER | SWP_NOMOVE);
    InvalidateRect(hwnd_, nullptr, FALSE);
  }
  return result;
}

LRESULT CALLBACK ViewVulkan::wndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                     LPARAM lParam) {
  if (msg == WM_NCCREATE) {
    auto *cs = reinterpret_cast<CREATESTRUCTW *>(lParam);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
  }

  auto *self =
      reinterpret_cast<ViewVulkan *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  if (msg == WM_PAINT && self) {
    ValidateRect(hwnd, nullptr);
    self->render();
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void ViewVulkan::render() {
  RECT client{};
  GetClientRect(hwnd_, &client);
  if (!presenter_ || client.right <= 0 || client.bottom <= 0)
    return;

  BITMAPINFO bi{};
  bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
  bi.bmiHeader.biWidth = client.right;
  bi.bmiHeader.biHeight = -client.bottom;
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;
  void *pixels = nullptr;
  HBITMAP bitmap =
      CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &pixels, nullptr, 0);
  if (!bitmap)
    return;
  HDC hdc = CreateCompatibleDC(nullptr);
  HGDIOBJ previousBitmap = SelectObject(hdc, bitmap);

  FillRect(hdc, &client, GetSysColorBrush(COLOR_WINDOW));
  paint(hdc, client);
  GdiFlush();
  presenter_->present(pixels, client.right, client.bottom);

  SelectObject(hdc, previousBitmap);
  DeleteDC(hdc);
  DeleteObject(bitmap);
}

void ViewVulkan::paint(HDC hdc, const RECT &client) const {
  const std::pair<const wchar_t *, const std::wstring *> rows[] = {
      {L"Wine version", &info_.version},
      {L"Wine build-id", &info_.build_id},
      {L"/proc/self/exe", &info_.proc_self_exe},
      {L"WINELOADER", &info_.wineloader},
      {L"WINEPREFIX", &info_.wineprefix},
      {L"WINESERVER", &info_.wineserver},
  };

  HGDIOBJ previousFont = SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
  SetBkMode(hdc, TRANSPARENT);

  TEXTMETRICW tm{};
  GetTextMetricsW(hdc, &tm);
  LONG labelWidth = 0;
  for (const auto &[label, value] : rows) {
    SIZE extent{};
    GetTextExtentPoint32W(hdc, label, lstrlenW(label), &extent);
    labelWidth = std::max(labelWidth, extent.cx);
  }

  constexpr LONG margin = 12;
  const LONG rowHeight = tm.tmHeight + tm.tmExternalLeading + 6;
  const UINT format = DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX;

  LONG y = margin;
  for (const auto &[label, value] : rows) {
    RECT labelRect{margin, y, margin + labelWidth, y + rowHeight};
    SetTextColor(hdc, GetSysColor(COLOR_GRAYTEXT));
    DrawTextW(hdc, label, -1, &labelRect, format);

    RECT valueRect{labelRect.right + margin, y, client.right - margin,
                   y + rowHeight};
    SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
    DrawTextW(hdc, value->c_str(), -1, &valueRect, format | DT_PATH_ELLIPSIS);

    y += rowHeight;
  }

  SelectObject(hdc, previousFont);
}

} // namespace WineInfo
