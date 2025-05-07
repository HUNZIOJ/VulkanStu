#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

const int MAX_FRAMES_IN_FLIGHT = 2;

#include <vector>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <memory>
#include <iostream>
#include <mutex>
#include <cstring>
#include <optional>
#include <set>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <semaphore>

class VulkanContext final {
private:
    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };
    // 使用inline内联初始化
    static inline std::once_flag _init_flag;
    static inline std::unique_ptr<VulkanContext> _ins = nullptr;

    // 不将其默认值设置成nullptr会导致火箭运行失败！！！！
    VkInstance* vkIns = new VkInstance; // 但是这里是不能设置为nullptr的，因为这里使用指针的原因就是需要往这块地址填数据，C++中不能往nullptr的位置填数据，实际nullptr的地址是0
    GLFWwindow* window = nullptr;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice logicalDevice = VK_NULL_HANDLE;
    std::optional<uint32_t> graphicsQueueFamilyIndex;
    std::optional<uint32_t> presentQueueFamilyIndex;
    VkQueue graphicsQueue; // 进行图像处理的队列
    VkSurfaceKHR surface;
    VkQueue presentQueue; // 进行图像渲染的队列，渲染到哪，到上面的surface里面
    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    VkPipelineLayout pipelineLayout;
    VkRenderPass renderPass;
    VkPipeline graphicsPipeline;
    std::vector<VkFramebuffer> swapChainFramebuffers;
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;
    bool framebufferResized = false;

    VulkanContext(int width = 800, int height = 600){
        // 在构造函数中来实现Vulkan的整体流程
        // vkInstanceCreateInfo是C语言的写法，然后C++的写法就是vk::InstanceCreateInfo{}，但是这种写法是使用原生的vulkan头文件，我这边使用的是glfw框架来实现跨平台的操作
        // vk::InstanceCreateInfo createInfo{}; 

        InitWindow(width, height);
        MainLoop();
    }
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;
    static void CreateInstance(){
        _ins.reset(new VulkanContext);
    }

    void CreateVulkanInstance(){
        // createInfo是一个参数结构体，这样调用函数不用写一万个参数传进去，而是先准备好函数的参数结构体
        VkInstanceCreateInfo createInfo{};

        // 创建appInfo，并设置参数
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Vulkan";
        appInfo.apiVersion = VK_API_VERSION_1_3; // 设置Vulkan版本

        // 设置appInfo
        createInfo.pApplicationInfo = &appInfo;
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

        // 添加验证层，validation layer是用来调试的，如果api调用有啥错误可以直接输出到控制台
        #ifdef DEBUG
            // 没看文档自己写的
            // vulkan中有很多layer，如果忘记了layer叫啥名字，可以通过这样来遍历所有的layer
            // VkLayerProperties *pProperties;
            // uint32_t* layerCount;
            // vkEnumerateInstanceLayerProperties(layerCount, pProperties);
            // for (uint32_t i = 0; i < *layerCount; i++) {
            //     std::cout << pProperties[i].layerName << std::endl;
            // }
            // const char* validationLayers[] = { "VK_LAYER_KHRONOS_validation" };
            // // 启用validation layer
            // createInfo.ppEnabledLayerNames = validationLayers;
            // createInfo.enabledLayerCount = 1;
            // std::cout << "validation layer enabled" << std::endl;

            // 文档上写的，这里加载验证层也不会生效，好像是我现在的Windows版本太高了
            // 实际上不是这样的，因为其实不销毁window对象不会导致vulkan的验证层输出错误信息，但是不销毁逻辑设备会导致验证层输出，所以验证层只是没有文档上面写的怎么严格罢了
            const std::vector<const char*> validationLayers = {
                "VK_LAYER_KHRONOS_validation"
            };
            uint32_t layerCount;
            vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
            std::vector<VkLayerProperties> availableLayers(layerCount);
            vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
            for (const char* layerName : validationLayers) {
                bool layerFound = false;
                for (const auto& layerProperties : availableLayers) {
                    if (strcmp(layerName, layerProperties.layerName) == 0) {
                        layerFound = true;
                        std ::cout << "found validation layer: " << layerName << std::endl;
                        break;
                    }
                }
                if(!layerFound) throw std::runtime_error("validation layer requested, but not found");
            }
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();

            // 消息回调：
            // 默认的validation layer行为是将fatal error打印到控制台
            // 但是我们可以设置一个消息回调来改变这个行为，这样error信息会传递到消息回调中进行处理
            // 实际上就是添加一个VK_EXT_DEBUG_UTILS_EXTENSION_NAME扩展，这个宏的实际内容是vk_ext_debug_utils
            // std::vector<const char*> getRequiredExtensions() {
            //     uint32_t glfwExtensionCount = 0;
            //     const char** glfwExtensions;
            //     glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
            
            //     std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
            
            //     if (enableValidationLayers) {
            //         extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            //     }
            
            //     return extensions;
            // }

            // 回调函数需要设置这两个签名来保证vulkan可以正确识别并且调用
            // static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
            //     VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            //     VkDebugUtilsMessageTypeFlagsEXT messageType,
            //     const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            //     void* pUserData) {
            
            //     std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
            
            //     return VK_FALSE;
            // }
        #endif

        # pragma region 加载和验证扩展
        // 这里通过glfw加载扩展，但是个数一直为0，所以是不起作用的
        // uint32_t glfwExtensionCount = 0;
        // const char** glfwExtensions;
        // glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        // std::cout << "glfw extension count: " << glfwExtensionCount << std::endl;
        // createInfo.enabledExtensionCount = glfwExtensionCount;
        // createInfo.ppEnabledExtensionNames = glfwExtensions;
        // for (size_t  i = 0; i < glfwExtensionCount; i++)
        // {
        //     std::cout << "glfw get extension: " << glfwExtensions[i] << std::endl;
        // }

        // 加载扩展
        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> extensions(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
        std::vector<const char*> extensionNames;
        for (const auto& extension : extensions) {
            extensionNames.emplace_back(extension.extensionName);
            std::cout << "Loaded extension: " << extension.extensionName << std::endl;
        }
        createInfo.enabledExtensionCount = extensionCount;
        createInfo.ppEnabledExtensionNames = extensionNames.data();
        
        
        # pragma endregion

        // 有创建就有销毁
        vkCreateInstance(&createInfo, nullptr, vkIns);
    }
    void InitWindow(int width, int height){
        // 初始化glfw
        glfwInit();
        // 设置窗口属性，历史原因glfw最初是用来创建OpenGL上下文的，为了兼容Vulkan，这里需要告诉glfw不要创建OpenGL上下文
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        // glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);   // 禁止修改窗口大小
        // 根据上面的属性来创建窗口
        window = glfwCreateWindow(width, height, "Hello Vulkan", nullptr, nullptr);
        // 第四个参数表示是否是全屏，第五个参数是共享指定窗口的上下文，最后一个参数只和OpenGL相关

        glfwSetWindowUserPointer(window, this);
        // 绑定窗口大小变化的回调函数
        glfwSetFramebufferSizeCallback(window, FramebufferResizeCallback);
    
        Init();
    }
    void MainLoop(){
        while(!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            Update();
        }
        Destroy();
    }
    void PickPhysicalDevice(){ // 实际上物理设备就是挑选显卡
        // 获取当前设备的所有物理设备，下面是一个标准的获取vk信息的流程，首先先获取vk信息的个数，然后通过个数来获取所有vk信息
        // vk信息当然都包含layers，extensions，physical devices等等，这里只是简单获取一下physical devices
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(*vkIns, &deviceCount, nullptr);
        if (deviceCount == 0) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(*vkIns, &deviceCount, devices.data());
        // 输出一下physical devices的名字，下面则是通过获取到的physical devices来得到这个设备的信息
        for (const auto& device : devices) {
            VkPhysicalDeviceProperties properties;
            vkGetPhysicalDeviceProperties(device, &properties);
            std::cout << "physical device: " << properties.deviceName << std::endl;
            // 如果这里是使用的C++版本的vulkan头文件，那么可以直接通过vkIns.enumeratePhysicalDevices()来获取physical devices，并且这个集合中的physical device的一些参数都是集成在这个结构里面的
            // 而且这个结构可以获取到一些更加详细的东西，例如Feature特点信息，这个东西能够获取更多的和硬件相关的特性
            VkPhysicalDeviceFeatures features;
            vkGetPhysicalDeviceFeatures(device, &features);
            std::cout << "physical device features: this device supports geometry shaders: " << (features.geometryShader ? "true" : "false") << std::endl;
        }
        // 这里只选择第一个physical device，当然你也可以选择你喜欢的，然后一般做法是需要通过features来判断是否满足你的需求，例如是否支持几何着色，是否支持MVR(VR)等
        // 我这边只有一张卡反正都得用呗
        physicalDevice = devices[0];
    }
    void PickQueueFamilies(){ 
        // 选择队列族，vulkan所有的东西都是使用一个队列来对gpu进行操作的，这些队列在vulkan中被称为queue family
        // 例如上传纹理，drawcall命令等等，但是这些队列的元素又不可能一样，所以需要通过队列族来找到自己想要的队列，然后来对这个队列进行操作
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());
        for (size_t i = 0; i < queueFamilyCount; i++) {
            if (!graphicsQueueFamilyIndex.has_value() && queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                graphicsQueueFamilyIndex = i;
            }
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
            if (presentSupport) {
                presentQueueFamilyIndex = i;
            }      
            if (graphicsQueueFamilyIndex.has_value() && presentQueueFamilyIndex.has_value()) break;
        }
        if (graphicsQueueFamilyIndex.has_value() && presentQueueFamilyIndex.has_value()) std::cout << "present queue and graphics queue has been found, graphics queue index:" << graphicsQueueFamilyIndex.value() << " present queue index:" << presentQueueFamilyIndex.value() << std::endl;
    }
    void CreateLogicalDevice(){
        // att；要使用swapchain需要在logical device中启用swapchain扩展，这里有个宏VK_KHR_SWAPCHAIN_EXTENSION_NAME
        PickQueueFamilies();

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {graphicsQueueFamilyIndex.value(), presentQueueFamilyIndex.value()};

        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures{};

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        std::cout << "queue create info count: " << createInfo.queueCreateInfoCount << std::endl;
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pEnabledFeatures = &deviceFeatures;
        const std::vector<const char*> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();
        std::cout << "enabled logical device extension count: " << createInfo.enabledExtensionCount << std::endl;
        
        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &logicalDevice) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device!");
        }
        std::cout << "logical device created successfully" << std::endl;
        vkGetDeviceQueue(logicalDevice, graphicsQueueFamilyIndex.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(logicalDevice, presentQueueFamilyIndex.value(), 0, &presentQueue);
    }
    void CreateSurface(){
        // VkWin32SurfaceCreateInfoKHR createInfo{};
        // createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        // createInfo.hwnd = glfwGetWin32Window(window);
        // createInfo.hinstance = GetModuleHandle(nullptr);
        // if (vkCreateWin32SurfaceKHR(*vkIns, &createInfo, nullptr, &surface) != VK_SUCCESS) {
        //     throw std::runtime_error("failed to create window surface!");
        // }

        if (glfwCreateWindowSurface(*vkIns, window, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }
        std::cout << "surface created successfully" << std::endl;
    }
    void CreateSwapChain(){
        // 交换链是类似OpenGL中的双缓冲操作，交换链定义了一系列双缓冲buffer的行为，所以vulkan中一定要显示的创建交换链
        // 要根据显卡来判断是否支持直接显示图像，如果是服务器显卡，那么就没有直接显示的必要，服务器显卡一般用来进行逻辑运算，所以需要先判断设备是否支持，然后还要启用一个swapchain扩展
        // 我这里所有的扩展都启用了，文档中最标准的写法是在使用任何vulkan特性或者扩展之前都要判断一下当前的显卡是否支持这些特性，以保证程序的跨平台性，我这里没有弄这些东西

        // 交换链的创建麻烦的很，还需要检查是否和当前Surface兼容，检查当前的Surface的格式，检查可用的presentation modes，最后一个没搞清楚是啥
        SwapChainSupportDetails details;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &details.capabilities); // 检查surface和swapchain的兼容性

        // 填充Surface支持的格式
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, details.formats.data());
        }

        // 检查presentation modes
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, details.presentModes.data());
        }

        // 判断当前显卡是否支持swapchain，只要有一种受支持的图像格式和表现模式，那么就可以使用swapchain
        if(details.formats.empty() || details.presentModes.empty()) throw std::runtime_error("failed to find a suitable swapchain!");

        // 创建交换链需要明确三个设置，Surface formate(Color Depth)，Presentation mode(swapchain进行swap的条件，也就是图像刷新的条件)，swap extent(swapchain中图像的分辨率)
        // 选择一个兼容的Surface Color Space
        VkSurfaceFormatKHR targetFormat;
        for (const auto& availableFormat : details.formats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                targetFormat = availableFormat;
                break;
            }
        }

        // 定义swapchain的一个swap行为
        VkPresentModeKHR presentMode = VkPresentModeKHR::VK_PRESENT_MODE_FIFO_KHR; // 默认为垂直同步
        for (const auto& availablePresentMode : details.presentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                presentMode = availablePresentMode;
            }
        }

        // 设置swapchain也就是帧缓冲的大小
        VkSurfaceCapabilitiesKHR capabilities = details.capabilities;
        VkExtent2D actualExtent;
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            actualExtent = capabilities.currentExtent;
        } else {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
    
            actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };
    
            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        }

        // 创建swapchain
        // 设置swapchain的图像数量，应该就是帧缓冲中RT的数量，至少要可以进行双缓冲操作吧
        uint32_t imageCount = details.capabilities.minImageCount + 1; 
        // 同样的不能超过最大值
        if (details.capabilities.maxImageCount > 0 && imageCount > details.capabilities.maxImageCount) {
            imageCount = details.capabilities.maxImageCount;
        }
        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = targetFormat.format;
        createInfo.imageColorSpace = targetFormat.colorSpace;
        createInfo.imageExtent = actualExtent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        uint32_t queueFamilyIndices[] = {graphicsQueueFamilyIndex.value(), presentQueueFamilyIndex.value()};

        if (graphicsQueueFamilyIndex != presentQueueFamilyIndex) {
            // 共享模式：两个队列族能够使用这个图像，一般用于。。。。看条件
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            // 独享模式：只有一个队列族能够使用这个图像
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0; // Optional
            createInfo.pQueueFamilyIndices = nullptr; // Optional
        }

        // 指定变换，可以直接在底层将图片进行变换，例如旋转啥的
        createInfo.preTransform = details.capabilities.currentTransform;
        // 定义混合行为，这个宏表示不进行透明混合呗
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        // 设置当前显示行为，例如垂直同步，立即模式，三缓冲模式啥的
        createInfo.presentMode = presentMode;
        // 设置是否剔除，这是遮挡剔除啊，但是原理好像和引擎层面的遮挡剔除不一样，而是说像素级别的遮挡剔除
        createInfo.clipped = VK_TRUE;
        // 这个东西是用来指定旧的交换链的，为啥需要oldSwapchain呢，情况如下：当窗口发生改变的时候，就需要重新创建交换链来适配当前窗口大小
        // 这时候就需要使用oldSwapChain来进行一些额外的操作
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(logicalDevice, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swap chain!");
        }
        std::cout << "swap chain created successfully" << std::endl;

        vkGetSwapchainImagesKHR(logicalDevice, swapChain, &imageCount, nullptr);
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(logicalDevice, swapChain, &imageCount, swapChainImages.data());

        swapChainImageFormat = targetFormat.format;
        swapChainExtent = actualExtent;
}
    void CreateImageViews(){
        // ImageView是用来指定图像的格式，访问方式，以及mipmap设置的，应该还能设置wrap mode和filter属性等等
        swapChainImageViews.resize(swapChainImages.size());
        for (size_t i = 0; i < swapChainImages.size(); i++) {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = swapChainImages[i];
            // 指定纹理的类型，cube，2d，3d这些
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = swapChainImageFormat;
            // 设置通道的映射
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            // 设置mipmap的属性
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;


            if (vkCreateImageView(logicalDevice, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create image views!");
            }
        }
    }
    void CreatePipeline(){
        // 传统的图形api中可以直接改变渲染管线的某些状态，例如设置当前渲染管线的混合等等一系列状态，但是vulkan中不能改变状态，如果需要重新设置这些状态那么就必须重新创建一个新的渲染管线
        // Spir-V是一种字节码着色语言，所以可以将glsl和hlsl或者cg转换成spir-v
        // shader着色器：KHR发布了一个抽象于平台的编译器，可以将glsl编译成spir-v。编译器的名字是glslangValidator.exe
        // 但是文档使用的google提供的一个glslc.exe这个编译器，这个编译器有一个优点就是采用了与GCC和Clang这些编译器相同的参数格式，并且还提供了额外的包含功能
        // 这两个编译器都在vulkan的bin目录下面
        // vulkan中的裁剪空间的起点和OpenGL中的标准不同，而是与DirectX的标准相同，都是左上角为原点
        // spir-v的最大优点就是一次着色器编译后续可以一直使用这个编译后的字节码着色器，而不用每次进入游戏都需要编译着色器
        // 果然有个人说的对，在计算机中没有什么事情是添加一个抽象层解决不了的，如果有那就再添加一层抽象
        // vulkan提供了一个libshaderc库，这个库可以在代码中编译glsl着色器
        
        // 保证目录的一致性，整个程序编译之后还要能够执行，所以需要将assets整个目录复制到可执行文件的相对路径下面(在CMakeLists中实现)
        auto vertShaderCode = readFile("../assets/shader/vert.spv");
        auto fragShaderCode = readFile("../assets/shader/frag.spv");
        // std::filesystem::path curPath = std::filesystem::current_path();
        // std::cout << "current path: " << curPath.string() << std::endl;
        VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode);
        VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCode);

        // 创建渲染管线的流程
        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT; // 指定当前着色器类型
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main"; // 指定调用的函数名
        // attention：pSpecializationInfo这个属性可以设置一些shader常量值，在创建pipeline的时候指定一些特殊常量来配置shader的行为，然后vulkan在渲染的时候会根据常量来进行渲染，但是这种条件是编译层面上的
        // 类似与C++的#ifdef，在编译的时候会优化一些没有用上的分支

        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

        // dynamic state：vulkan中少量的可以在管线运行的时候动态改变的状态，例如视口大小，线宽和混合常数等等，这些都是通过下面这个结构来控制的
        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();
        // 设置了dynamic state之后这些内容在创建渲染管线的时候就会被忽略掉，然后需要在运行的时候来设置

        // 顶点输入：首先需要定义当前的数据是按照顶点输入还是按照实例输入，这里对应这OpenGL中的实例化传递参数的概念
        // 按照顶点输入那么每个顶点都会根据一个偏移值得到一个新的数据，如果是按照实例那么就会是实例按照偏移得到一个单独的数据
        // 顶点输入还需要添加OpenGL中的vao这些信息，例如数据类型，绑定方式，偏移量。
        // 实际上顶点数据在shader中，哈哈哈哈哈文档是这样实现的，所以这里也不需要指定顶点数据和加载方式、
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
        vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

        // 图元装配：VkPipelineInputAssemblyStateCreateInfo 描述了如何进行图元装配，还有是否启用ebo进行顶点重用
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE; // 启用这个东西可以在_Strip拓扑模式中分割三角形和线，更加灵活

        // 视口的设置
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) swapChainExtent.width;
        viewport.height = (float) swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        // 定义裁剪区
        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent;

        // 视口和裁剪区的设置，这里只设置数量是因为需要动态修改，如果没有动态修改，那么就要设置viewport和scissor的实际数据
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        // 光栅化
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE; // 默认情况下为false，如果设置为true，那么超出近裁剪面和远裁剪面的像素会被限制在这两个面的范围内，而不是丢弃，这种方式在生成shadow map阴影图的时候非常有用
        rasterizer.rasterizerDiscardEnable = VK_FALSE; // 设置为true会丢弃所有片段
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL; // 设置多边形的填充模式，也能设置线框和顶点模式，上面两个属性设置其他的模式的时候好像要启用GPU Features
        rasterizer.lineWidth = 1.0f; // 设置线宽，超过1需要启用特性
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT; // 设置剔除模式
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE; // 设置正面朝向，这里使用的是顺时针，那么就能推断是左手坐标系
        // 下面的参数是用来调整阴影映射的偏移值的，因为阴影图生成的阴影会导致有摩尔纹和条纹Bug，所以需要通过应用偏移来解决
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f; // Optional
        rasterizer.depthBiasClamp = 0.0f; // Optional
        rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

        // 多重采样，抗锯齿用的，这里只进行了一次采样
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f; // Optional
        multisampling.pSampleMask = nullptr; // Optional
        multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
        multisampling.alphaToOneEnable = VK_FALSE; // Optional

        // 深度测试和模板测试，这里先不用
        VkPipelineDepthStencilStateCreateInfo depthStencil{};

        // 颜色混合，这个是配置附加帧缓冲的
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;  // 不启用混合
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional
        // VkBlendFactor and VkBlendOp 这两枚举可以找到所有混合设置

        // 颜色混合，这个是全局的
        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f; // Optional
        colorBlending.blendConstants[1] = 0.0f; // Optional
        colorBlending.blendConstants[2] = 0.0f; // Optional
        colorBlending.blendConstants[3] = 0.0f; // Optional

        // 用来像shader的uniform变量传递数据的，这里现在没有用到shader的外部数据
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0; // Optional
        pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
        pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
        pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

        if (vkCreatePipelineLayout(logicalDevice, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = nullptr; // Optional
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;
        // 设置父管道，可以从父管道继承一部分功能
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
        pipelineInfo.basePipelineIndex = -1; // Optional

        // 可以创建多个pipeline和接受多个createInfo
        if (vkCreateGraphicsPipelines(logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        std::cout << "graphics pipeline created successfully" << std::endl;

        // 渲染管线在创建的时候会将这些字节码通过编译和链接成机器码，来提供给GPU执行，所以渲染管线创建完了之后就可以将这些字节码释放掉了
        vkDestroyShaderModule(logicalDevice, fragShaderModule, nullptr);
        vkDestroyShaderModule(logicalDevice, vertShaderModule, nullptr);
    }
    VkShaderModule CreateShaderModule(const std::vector<char>& code){
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        // 这个玩意要的是一个uint32_t的指针的数据，所以需要用reinterpret_cast<>强制转换指针，而且需要考虑对齐的问题
        // 这里的对齐说的不是字节对齐，而是单个数据长度对齐，char类型是一个字节，但是uint32_t是四个字节，所以需要对齐
        // 文档上面写了好像说使用的是vector的默认allocator，这个已经做好了字节不对齐的处理
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
        VkShaderModule shaderModule;
        if (vkCreateShaderModule(logicalDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("failed to create shader module!");
        }
        return shaderModule;
    }
    static std::vector<char> readFile(const std::string& filename) {
        // ate表示从文件末尾打开文件
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("failed to open file!");
        }
        size_t fileSize = (size_t) file.tellg();
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();
        return buffer;
    }
    void CreateRenderPass(){
        // 用来指定多少颜色，深度缓冲区，以及样本和整个渲染过程的处理方式
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = swapChainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        // 加载颜色之前清除原来的颜色
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        // 模板缓冲的行为：不用模板测试，所以直接Don't care
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        // 设置颜色缓冲区的作用，例如当作一个帧缓冲的颜色附件，或者是用来显示的颜色附件，或者是用来内存复制的附件
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        // 子通道：一个通道可以包含多个子通道，当前通道的结果依赖于上一个通道的渲染操作
        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
            throw std::runtime_error("failed to create render pass!");
        }
        std::cout << "render pass created successfully" << std::endl;

    }
    void CreateFramebuffers(){
        swapChainFramebuffers.resize(swapChainImageViews.size());
        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            VkImageView attachments[] = {
                swapChainImageViews[i]
            };
        
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = swapChainExtent.width;
            framebufferInfo.height = swapChainExtent.height;
            framebufferInfo.layers = 1;
        
            if (vkCreateFramebuffer(logicalDevice, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create framebuffer!");
            }
        }
        std::cout << "framebuffers created successfully, size: " << swapChainFramebuffers.size() << std::endl;
    }
    void CreateCommandPool(){
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // 设置命令池的行为，有点像FSM中的状态机的行为，就是将command buffer存起来？？
        poolInfo.queueFamilyIndex = graphicsQueueFamilyIndex.value();
        if (vkCreateCommandPool(logicalDevice, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool!");
        }
        std::cout << "command pool created successfully" << std::endl;
    }
    void CreateCommandBuffer(){
        commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // 设置主副缓冲区
        allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();
        
        if (vkAllocateCommandBuffers(logicalDevice, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }
        std::cout << "command buffer created successfully" << std::endl;
    }
    void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex){
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0; // Optional
        beginInfo.pInheritanceInfo = nullptr; // Optional
        
        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapChainExtent;
        VkClearValue clearColor = {{{82.0f / 255.0f, 82.0f / 255.0f, 136.0f / 255.0f, 1.0f}}};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor; // 定义背景颜色，也就是OpenGL中的clear color
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline); // 第二个参数指定pipeline类型，一种图形管道用来渲染图形，一种计算管道用来并行计算
        
        // 前面将viewport和scissor都设置成了动态属性了，所以需要在这里设置这两玩意
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(swapChainExtent.width);
        viewport.height = static_cast<float>(swapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
        // 第一参数懂的都懂
        // 第二个参数是vertex count，也就是顶点数量
        // 第三个参数是instance count，也就是实例数量，不用instance就设置为1
        // 第四个参数是起始vertex index，也就是gl_VertexIndex的起始值，也能说是偏移值
        // 第五个参数是起始instance index，也就是gl_InstanceIndex的起始值，也能说是偏移值

        vkCmdEndRenderPass(commandBuffer);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }   
    }
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame = 0;
    void CreateSyncObjects(){
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(logicalDevice, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {

                throw std::runtime_error("failed to create synchronization objects for a frame!");
            }
        }
        std::cout << "sync objects created successfully" << std::endl;
    }
    void DrawFrame(){
        // 执行流程：
        // 等待前一帧渲染完成
        // 从交换链中获取一个图像
        // 用commandbuffer绘制场景到图像上
        // 将图像渲染到屏幕上，然后将图像返回到交换链中
        // 这上面是异步的，但是每一步的执行都依赖上一步的结果，所以引入同步机制
        // 同步机制分为两种：一种是信号量，一种是Fance栅栏
        // 信号量是一种非阻塞的同步机制，有点像事件触发机制，不会阻塞当前线程，而是满足某些条件之后才触发某些逻辑
        // Fance栅栏是一种阻塞的同步机制，在wait的过程会阻塞当前线程，知道满足Fance的条件才会往下执行
        vkWaitForFences(logicalDevice, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX); // 等待上一帧渲染完成
        uint32_t imageIndex;
        auto result = vkAcquireNextImageKHR(logicalDevice, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
            framebufferResized = false;
            RecreateSwapChain();
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }
        vkResetFences(logicalDevice, 1, &inFlightFences[currentFrame]);

        vkResetCommandBuffer(commandBuffers[currentFrame], 0);
        RecordCommandBuffer(commandBuffers[currentFrame], imageIndex);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[currentFrame];
        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;
        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = {swapChain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;
        presentInfo.pResults = nullptr; // Optional

        auto res = vkQueuePresentKHR(presentQueue, &presentInfo);
        if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
            RecreateSwapChain();
        } else if (res != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }
    void RecreateSwapChain(){
        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(window, &width, &height);
            glfwWaitEvents();
        }
        // 理论上渲染通道也需要重新创建，因为渲染通道依赖于SwapChain的Format，重建交换链之后这个Format可能会发生改变
        vkDeviceWaitIdle(logicalDevice);
        ClearSwapChain();
        CreateSwapChain();
        CreateImageViews();
        CreateFramebuffers();
        std::cout << "window resized, swap chain recreated successfully" << std::endl;
    }
    void ClearSwapChain(){
        for (size_t i = 0; i < swapChainFramebuffers.size(); i++) {
            vkDestroyFramebuffer(logicalDevice, swapChainFramebuffers[i], nullptr);
        }
    
        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            vkDestroyImageView(logicalDevice, swapChainImageViews[i], nullptr);
        }
    
        vkDestroySwapchainKHR(logicalDevice, swapChain, nullptr);
    }
    static void FramebufferResizeCallback(GLFWwindow* window, int width, int height) {
        auto app = reinterpret_cast<VulkanContext*>(glfwGetWindowUserPointer(window));
        app->framebufferResized = true;
    }

    // 定义一些基本的生命周期函数
    void Init(){
        CreateVulkanInstance(); // 设置layer和extension
        CreateSurface(); // 这个会影响后面的物理设备和逻辑设备
        PickPhysicalDevice();
        CreateLogicalDevice();
        CreateSwapChain();
        CreateImageViews();
        CreateRenderPass();
        CreatePipeline();
        CreateFramebuffers();
        CreateCommandPool();
        CreateCommandBuffer();
        CreateSyncObjects();
    }
    void Update(){
        DrawFrame();
    }
    void Destroy(){
        vkDeviceWaitIdle(logicalDevice);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(logicalDevice, renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(logicalDevice, imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(logicalDevice, inFlightFences[i], nullptr);
        }

        vkDestroyCommandPool(logicalDevice, commandPool, nullptr);

        for (auto framebuffer : swapChainFramebuffers) {
            vkDestroyFramebuffer(logicalDevice, framebuffer, nullptr);
        }

        vkDestroyPipeline(logicalDevice, graphicsPipeline, nullptr);

        vkDestroyRenderPass(logicalDevice, renderPass, nullptr);

        vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);

        for (auto imageView : swapChainImageViews) {
            vkDestroyImageView(logicalDevice, imageView, nullptr);
        }

        vkDestroySwapchainKHR(logicalDevice, swapChain, nullptr);

        vkDestroyDevice(logicalDevice, nullptr);

        vkDestroySurfaceKHR(*vkIns, surface, nullptr);

        vkDestroyInstance(*vkIns, nullptr);
        // 清理创建的窗口资源
        glfwDestroyWindow(window);
        // 关闭窗口
        glfwTerminate();
    }


public:
    static VulkanContext* GetInstance(){
        std::call_once(_init_flag, CreateInstance); // 线程安全的
        return _ins.get();
    }
    ~VulkanContext(){}
};