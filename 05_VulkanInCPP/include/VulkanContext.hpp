#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <vulkan/vulkan.hpp>

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
    static inline std::once_flag _init_flag;
    static inline std::unique_ptr<VulkanContext> _ins = nullptr;

    #pragma region VulkanContext

    vk::Instance vkInstance;
    vk::PhysicalDevice physicalDevice;
    vk::Device device;
    struct QueueFamilyIndices final{
        std::optional<uint32_t> graphicsFamily; // 支持图像渲染的队列族索引
        std::optional<uint32_t> presentFamily; // 支持图像显示的队列族索引
        operator bool() const {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    } familyIndices;
    vk::Queue graphicsQueue;
    vk::Queue presentQueue;
    vk::SurfaceKHR surface;
    vk::SwapchainKHR swapChain;
    struct SwapChainInfo final{
        vk::SurfaceFormatKHR format;
        vk::PresentModeKHR presentMode;
        vk::Extent2D extent;
        std::vector<vk::Image> images;
        std::vector<vk::ImageView> imageViews;
        uint32_t imageCount;
        vk::SurfaceCapabilitiesKHR capabilities;
    } swapChainInfo;
    vk::RenderPass renderPass;
    vk::PipelineLayout pipelineLayout;
    vk::Pipeline graphicsPipeline;
    std::vector<vk::Framebuffer> framebuffers;
    vk::CommandPool commandPool;
    std::vector<vk::CommandBuffer> commandBuffers;

    std::vector<vk::Semaphore> imageAvailableSemaphores;
    std::vector<vk::Semaphore> renderFinishedSemaphores;
    std::vector<vk::Fence> inFlightFences;
    uint32_t currentFrame = 0;
    bool framebufferResized = false;
    vk::Buffer vertexBuffer;
    vk::DeviceMemory vertexBufferMemory;

    #pragma endregion

    #pragma region ModelData
    struct Vertex final{
        glm::vec2 pos;
        glm::vec3 color;
    };
    // 前面是顶点位置,后面是顶点颜色
    const std::vector<Vertex> vertices = {
        {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
        {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
    };
    #pragma endregion

    // 不将其默认值设置成nullptr会导致火箭运行失败！！！！
    GLFWwindow* window = nullptr;

    VulkanContext(int width = 800, int height = 600){
        InitWindow(width, height);
        MainLoop();
    }
    
    VulkanContext(const VulkanContext&) = delete;
    
    VulkanContext& operator=(const VulkanContext&) = delete;
    
    static void CreateInstance(){
        _ins.reset(new VulkanContext);
    }

    void InitWindow(int width, int height){
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        // glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);   // 禁止修改窗口大小
        window = glfwCreateWindow(width, height, "Hello Vulkan", nullptr, nullptr);
        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, FramebufferResizeCallback);
    }
    
    void MainLoop(){
        Init();
        while(!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            Update();
        }
        Destroy();
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

    void Init(){
        CreateVulkanInstance();

        CreateSurface();

        PickPhysicalDevice();

        QueryQueueFamilyIndices();

        CreateLogicalDevice();

        GetQueues();

        CreateSwapChain();

        CreateImageViews();

        CreateRenderPass();

        CreateGraphicsPipeline();

        CreateFramebuffers();

        CreateCommandPool();

        CreateVertexBuffers();

        CreateCommandBuffers();

        CreateSyncObjects();
    }

    void Update(){
        DrawPerFrame();
    }

    void Destroy(){
        device.waitIdle();

        device.freeMemory(vertexBufferMemory);

        device.destroyBuffer(vertexBuffer);

        ClearSwapChain();

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            device.destroySemaphore(imageAvailableSemaphores[i]);
            device.destroySemaphore(renderFinishedSemaphores[i]);
            device.destroyFence(inFlightFences[i]);
        }

        device.destroyCommandPool(commandPool);

        device.destroyPipeline(graphicsPipeline);

        device.destroyPipelineLayout(pipelineLayout);

        device.destroyRenderPass(renderPass);

        device.destroy();

        vkInstance.destroySurfaceKHR(surface);

        vkInstance.destroy();

        glfwDestroyWindow(window);

        glfwTerminate();
    }

    void CreateVulkanInstance(){
        auto insCreateInfo = vk::InstanceCreateInfo();
        auto appInfo = vk::ApplicationInfo();

        appInfo.setPApplicationName("Hello Vulkan")
               .setApiVersion(VK_API_VERSION_1_3);
        insCreateInfo.setPApplicationInfo(&appInfo);

        #pragma region 启用验证层
        std::vector<const char*> layers = { "VK_LAYER_KHRONOS_validation" };
        insCreateInfo.setEnabledLayerCount(1);
        insCreateInfo.setPEnabledLayerNames(layers);
        #pragma endregion

        #pragma region 启用扩展
        std::vector<const char*> extensions = {};
        uint32_t extCount;
        auto exts = glfwGetRequiredInstanceExtensions(&extCount);
        for (uint32_t i = 0; i < extCount; ++i) {
            extensions.push_back(exts[i]);
            std::cout << "Loaded extension: " << exts[i] << std::endl;
        }
        insCreateInfo.setEnabledExtensionCount(extCount).setPEnabledExtensionNames(extensions);
        #pragma endregion

        vkInstance = vk::createInstance(insCreateInfo);
    }
    
    void PickPhysicalDevice(){
        // 这里可以写你想实现的挑选规则，能根据显卡支持的功能来设置优先级，然后根据优先级来挑选显卡设备
        physicalDevice = vkInstance.enumeratePhysicalDevices().front();
        std::cout << "Picked physical device: " << physicalDevice.getProperties().deviceName << std::endl;
    }

    void QueryQueueFamilyIndices(){
        auto properties = physicalDevice.getQueueFamilyProperties();
        for (size_t i = 0; i < properties.size(); ++i)
        {
            if(properties[i].queueFlags & vk::QueueFlagBits::eGraphics){
                familyIndices.graphicsFamily = i;
            }
            if(physicalDevice.getSurfaceSupportKHR(i, surface)){
                familyIndices.presentFamily = i;
            }
            if(familyIndices) break;
        }
        
    }

    void CreateLogicalDevice(){
        auto deviceCreateInfo = vk::DeviceCreateInfo();

        std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {familyIndices.graphicsFamily.value(), familyIndices.presentFamily.value()};
        float priority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            auto queueCreateInfo = vk::DeviceQueueCreateInfo();
            queueCreateInfo.setPQueuePriorities(&priority)
                          .setQueueCount(1)
                          .setQueueFamilyIndex(familyIndices.graphicsFamily.value());
            queueCreateInfos.push_back(queueCreateInfo);
        }

        std::vector<const char*> exts = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        
        deviceCreateInfo.setQueueCreateInfos(queueCreateInfos).setPEnabledExtensionNames(exts);

        device = physicalDevice.createDevice(deviceCreateInfo);
    }

    void GetQueues(){
        graphicsQueue = device.getQueue(familyIndices.graphicsFamily.value(), 0);
        presentQueue = device.getQueue(familyIndices.presentFamily.value(), 0);
    }

    void CreateSurface(){
        VkSurfaceKHR s;
        if (glfwCreateWindowSurface(static_cast<VkInstance>(vkInstance), window, nullptr, &s) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }
        surface = vk::SurfaceKHR(s);
    }

    void CreateSwapChain(){
        QuerySwapChainInfo();

        auto createInfo = vk::SwapchainCreateInfoKHR();
        createInfo.setSurface(surface)
                  .setClipped(true)
                  .setImageArrayLayers(1)
                  .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
                  .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
                  .setMinImageCount(swapChainInfo.imageCount)
                  .setImageColorSpace(swapChainInfo.format.colorSpace)
                  .setImageExtent(swapChainInfo.extent)
                  .setImageFormat(swapChainInfo.format.format);

        std::vector<unsigned int> queueFamilyIndices = { familyIndices.graphicsFamily.value(), familyIndices.graphicsFamily.value()};

        if (familyIndices.graphicsFamily.value() != familyIndices.graphicsFamily.value()) {
            createInfo.setImageSharingMode(vk::SharingMode::eConcurrent)
                      .setQueueFamilyIndexCount(queueFamilyIndices.size())
                      .setPQueueFamilyIndices(queueFamilyIndices.data());
        } else {
            createInfo.setImageSharingMode(vk::SharingMode::eExclusive)
                      .setQueueFamilyIndexCount(0)
                      .setPQueueFamilyIndices(nullptr);
        }
        createInfo.setPreTransform(swapChainInfo.capabilities.currentTransform)
                  .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
                  .setPresentMode(swapChainInfo.presentMode)
                  .setOldSwapchain(nullptr);

        swapChain = device.createSwapchainKHR(createInfo);

        swapChainInfo.images = device.getSwapchainImagesKHR(swapChain);
        swapChainInfo.imageViews.resize(swapChainInfo.images.size());
    }

    void QuerySwapChainInfo(){
        for (const auto& availableFormat : physicalDevice.getSurfaceFormatsKHR(surface))
        {
            if (availableFormat.format == vk::Format::eR8G8B8A8Srgb && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
                swapChainInfo.format = availableFormat;
                break;
            }
        }

        for (const auto& presentMode : physicalDevice.getSurfacePresentModesKHR(surface))
        {
            if (presentMode == vk::PresentModeKHR::eMailbox) {
                swapChainInfo.presentMode = presentMode;
            }
        }

        auto capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
        swapChainInfo.imageCount = std::clamp<uint32_t>(2, capabilities.minImageCount, capabilities.maxImageCount);
        
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            swapChainInfo.extent = capabilities.currentExtent;
        } else {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
    
            swapChainInfo.extent = vk::Extent2D {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };
    
            swapChainInfo.extent.width = std::clamp(swapChainInfo.extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            swapChainInfo.extent.height = std::clamp(swapChainInfo.extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        }
        swapChainInfo.capabilities = capabilities;
    }

    void CreateImageViews(){
        swapChainInfo.imageViews.resize(swapChainInfo.images.size());
        for (size_t i = 0; i < swapChainInfo.images.size(); ++i)
        {
            auto createInfo = vk::ImageViewCreateInfo();
            createInfo.setImage(swapChainInfo.images[i])
                      .setViewType(vk::ImageViewType::e2D)
                      .setFormat(swapChainInfo.format.format)
                      .setComponents({ vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA })
                      .setSubresourceRange({ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }); // 设置mipmap
            swapChainInfo.imageViews[i] = device.createImageView(createInfo);
        }
    }

    void CreateRenderPass(){
        auto createInfo = vk::RenderPassCreateInfo();

        // 设置颜色附件
        auto colorAttachment = vk::AttachmentDescription();
        colorAttachment.setFormat(swapChainInfo.format.format)
                       .setSamples(vk::SampleCountFlagBits::e1)
                       .setLoadOp(vk::AttachmentLoadOp::eClear)
                       .setStoreOp(vk::AttachmentStoreOp::eStore)
                       .setInitialLayout(vk::ImageLayout::eUndefined)
                       .setFinalLayout(vk::ImageLayout::ePresentSrcKHR) // 这个layout表示是用来在swapchain中渲染到屏幕的Image
                       .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                       .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);

        auto colorAttachmentRef = vk::AttachmentReference();
        colorAttachmentRef.setAttachment(0)
                          .setLayout(vk::ImageLayout::eColorAttachmentOptimal);

        
        auto subpass = vk::SubpassDescription();
        subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
               .setColorAttachments(colorAttachmentRef);

        
        auto subpassDependency = vk::SubpassDependency();
        subpassDependency.setSrcSubpass(VK_SUBPASS_EXTERNAL)
                         .setDstSubpass(0)
                         .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
                         .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
                         .setSrcAccessMask(vk::AccessFlags(0))
                         .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

        createInfo.setDependencies(subpassDependency)
                  .setAttachments(colorAttachment)
                  .setSubpasses(subpass);

        renderPass = device.createRenderPass(createInfo);
    }

    vk::ShaderModule CreateShaderModule(const std::vector<char>& code){
        vk::ShaderModuleCreateInfo createInfo;
        createInfo.setCodeSize(code.size())
                  .setPCode(reinterpret_cast<const uint32_t*>(code.data()));
        return std::move(device.createShaderModule(createInfo));
    }
    
    void CreateGraphicsPipeline(){
        auto vertShaderCode = readFile("../assets/shader/vert.spv");
        auto fragShaderCode = readFile("../assets/shader/frag.spv");
        auto vertShaderModule = CreateShaderModule(vertShaderCode);
        auto fragShaderModule = CreateShaderModule(fragShaderCode);
        auto vertShaderStageInfo = vk::PipelineShaderStageCreateInfo();
        vertShaderStageInfo.setStage(vk::ShaderStageFlagBits::eVertex)
                        .setModule(vertShaderModule)
                        .setPName("main");
        auto fragShaderStageInfo = vk::PipelineShaderStageCreateInfo();
        fragShaderStageInfo.setStage(vk::ShaderStageFlagBits::eFragment)
                        .setModule(fragShaderModule)
                        .setPName("main");
        auto shaderStageInfos = std::vector<vk::PipelineShaderStageCreateInfo>{vertShaderStageInfo, fragShaderStageInfo};

        // 渲染管线中可以动态修改的数据
        std::vector<vk::DynamicState> dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
        vk::PipelineDynamicStateCreateInfo dynamicStateCreateInfo;
        dynamicStateCreateInfo.setDynamicStates(dynamicStates);

        // 这里应该对应OpenGL中的vao
        auto vertexBingdingDes = vk::VertexInputBindingDescription();
        vertexBingdingDes.setBinding(0) // 接收的vbo的数量,这里只会接受一个vbo,设置是指定vbo的索引
                         .setStride(sizeof(Vertex)) // 设置一个数据的步长
                         .setInputRate(vk::VertexInputRate::eVertex); // 移动的行为,每个顶点移动一个步长,还是每个实例移动一个步长

        auto posAttr = vk::VertexInputAttributeDescription();
        posAttr.setBinding(0) // 指定vulkan从哪个binding中获取数据,类似于OpenGL中的vbo id
               .setLocation(0)  // 指定拿到数据之后放到shader的哪个变量中
               .setFormat(vk::Format::eR32G32Sfloat) // 指定数据的格式,一个顶点有两个浮点数表示位置,所以是R32G32Sfloat
               .setOffset(offsetof(Vertex, pos)); // 计算这个元素在结构体中的一个偏移值,这里估计涉及到了GPU的一个字节对齐方式
        auto colorAttr = vk::VertexInputAttributeDescription();
        colorAttr.setBinding(0)
                 .setLocation(1)
                 .setFormat(vk::Format::eR32G32B32Sfloat)
                 .setOffset(offsetof(Vertex, color));

        std::vector<vk::VertexInputAttributeDescription> vertexAttributeDes = { posAttr, colorAttr };

        auto vertexInputInfo = vk::PipelineVertexInputStateCreateInfo();
        vertexInputInfo.setVertexBindingDescriptionCount(1)
                       .setVertexAttributeDescriptionCount(vertexAttributeDes.size())
                       .setVertexBindingDescriptions({vertexBingdingDes})
                       .setVertexAttributeDescriptions(vertexAttributeDes);

        // 设置图元装配行为，对应OpenGL中的glDraw方法的一部分逻辑
        auto inputAssemblyInfo = vk::PipelineInputAssemblyStateCreateInfo();
        inputAssemblyInfo.setTopology(vk::PrimitiveTopology::eTriangleList)
                         .setPrimitiveRestartEnable(false);

        auto viewport = vk::Viewport();
        viewport.setX(0.0f)
                .setY(0.0f)
                .setWidth(static_cast<float>(swapChainInfo.extent.width))
                .setHeight(static_cast<float>(swapChainInfo.extent.height))
                .setMinDepth(0.0f)
                .setMaxDepth(1.0f);

        auto scissor = vk::Rect2D();
        scissor.setOffset({0, 0})
               .setExtent(swapChainInfo.extent);

        // 不要设置实际的数据，因为这些是动态修改的数据
        auto viewportInfo = vk::PipelineViewportStateCreateInfo();
        viewportInfo.setViewportCount(1)
                    .setScissorCount(1);
        
        // 光栅化行为
        auto rasterizationInfo = vk::PipelineRasterizationStateCreateInfo();
        rasterizationInfo.setDepthClampEnable(false) // 设置为true会导致不在视锥范围内的像素会被clamp到视锥的范围内，这种情况适合shadow map
                         .setRasterizerDiscardEnable(false) // 文档写的很模糊，这个设置为true就会导致不会光栅化
                         .setPolygonMode(vk::PolygonMode::eFill) // 线框模式，填充模式，点模式
                         .setCullMode(vk::CullModeFlagBits::eBack) // 剔除模式
                         .setFrontFace(vk::FrontFace::eClockwise) // 设置前面的点顺序
                         .setDepthBiasEnable(false) // 偏移深度，用来解决shadow map出现摩尔纹的问题，是因为采样的频率跟不上导致会产生这些问题
                         .setLineWidth(1.0f); // 线宽

        // 采样行为
        auto multiSampleInfo = vk::PipelineMultisampleStateCreateInfo();
        multiSampleInfo.setRasterizationSamples(vk::SampleCountFlagBits::e1)
                       .setSampleShadingEnable(false)
                       .setMinSampleShading(1.0f)
                       .setPSampleMask(nullptr)
                       .setAlphaToCoverageEnable(false)
                       .setAlphaToOneEnable(false);


        // 深度测试和模板测试的设置
        auto depthStencilInfo = vk::PipelineDepthStencilStateCreateInfo();

        // 颜色混合行为
        auto colorBlendAttachment = vk::PipelineColorBlendAttachmentState();
        colorBlendAttachment.setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA)
                             .setBlendEnable(false) // 这里没有启用混合，所以后面的参数可以不用设置，但是启用了混合之后就一定要根据需求来配置参数
                             .setSrcColorBlendFactor(vk::BlendFactor::eZero)
                             .setDstColorBlendFactor(vk::BlendFactor::eZero)
                             .setColorBlendOp(vk::BlendOp::eAdd)
                             .setSrcAlphaBlendFactor(vk::BlendFactor::eZero)
                             .setDstAlphaBlendFactor(vk::BlendFactor::eZero)
                             .setAlphaBlendOp(vk::BlendOp::eAdd);

        
        // 好像是另外一种按位颜色混合的方式，文档讲的不是很清楚
        auto colorBlendInfo = vk::PipelineColorBlendStateCreateInfo();
        colorBlendInfo.setLogicOpEnable(false)
                      .setAttachments(colorBlendAttachment)
                      .setLogicOp(vk::LogicOp::eCopy)
                      .setBlendConstants({0.0f, 0.0f, 0.0f, 0.0f});

        // 通过这个结构来将uniform变量传递给shader
        auto pipelineLayoutCreateInfo = vk::PipelineLayoutCreateInfo();
        pipelineLayoutCreateInfo.setSetLayoutCount(0)
                                .setSetLayouts(nullptr)
                                .setPushConstantRangeCount(0)
                                .setPushConstantRanges(nullptr);

        pipelineLayout = device.createPipelineLayout(pipelineLayoutCreateInfo);

        auto pipelineCreateInfo = vk::GraphicsPipelineCreateInfo();
        pipelineCreateInfo.setStages(shaderStageInfos)
                          .setPVertexInputState(&vertexInputInfo)
                          .setPInputAssemblyState(&inputAssemblyInfo)
                          .setPViewportState(&viewportInfo)
                          .setPRasterizationState(&rasterizationInfo)
                          .setPMultisampleState(&multiSampleInfo)
                          .setPDepthStencilState(&depthStencilInfo)
                          .setPColorBlendState(&colorBlendInfo)
                          .setPDynamicState(&dynamicStateCreateInfo)
                          .setLayout(pipelineLayout)
                          .setRenderPass(renderPass)
                          .setSubpass(0)
                          .setBasePipelineHandle(nullptr)
                          .setBasePipelineIndex(-1);

        auto pipelineDetail = device.createGraphicsPipeline(nullptr, pipelineCreateInfo);
        if (pipelineDetail.result != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }
        graphicsPipeline = pipelineDetail.value;

        device.destroyShaderModule(vertShaderModule);
        device.destroyShaderModule(fragShaderModule);
    }

    void CreateFramebuffers(){
        framebuffers.resize(swapChainInfo.imageViews.size());
        for (size_t i = 0; i < swapChainInfo.imageViews.size(); ++i)
        {
            auto attachments = std::vector<vk::ImageView>{swapChainInfo.imageViews[i]};
            auto createInfo = vk::FramebufferCreateInfo();
            createInfo.setRenderPass(renderPass)
                      .setAttachments(attachments)
                      .setWidth(swapChainInfo.extent.width)
                      .setHeight(swapChainInfo.extent.height)
                      .setLayers(1);
            framebuffers[i] = device.createFramebuffer(createInfo);
        }
    }

    void CreateCommandPool(){
        auto createInfo = vk::CommandPoolCreateInfo();
        createInfo.setQueueFamilyIndex(familyIndices.graphicsFamily.value())
                  .setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
        commandPool = device.createCommandPool(createInfo);
    }

    void CreateCommandBuffers(){
        commandBuffers.resize(swapChainInfo.imageViews.size());
        auto allocateInfo = vk::CommandBufferAllocateInfo();
        allocateInfo.setCommandPool(commandPool)
                    .setLevel(vk::CommandBufferLevel::ePrimary)
                    .setCommandBufferCount(static_cast<uint32_t>(commandBuffers.size()));
        commandBuffers = device.allocateCommandBuffers(allocateInfo);
    }

    void RecordCommandBuffer(vk::CommandBuffer commandBuffer, uint32_t imageIndex){
        auto beginInfo = vk::CommandBufferBeginInfo();
        commandBuffer.begin(beginInfo);

        auto renderPassInfo = vk::RenderPassBeginInfo();
        auto clearColor = vk::ClearValue();
        clearColor.setColor({82.0f / 255.0f, 82.0f / 255.0f, 136.0f / 255.0f, 1.0f});
        renderPassInfo.setRenderPass(renderPass)
                      .setFramebuffer(framebuffers[imageIndex])
                      .setRenderArea({ {0, 0}, swapChainInfo.extent })
                      .setPClearValues(&clearColor)
                      .setClearValueCount(1);
        commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);

        // 更新viewport和scissor
        auto viewport = vk::Viewport();
        viewport.setX(0.0f)
                .setY(0.0f)
                .setWidth(static_cast<float>(swapChainInfo.extent.width))
                .setHeight(static_cast<float>(swapChainInfo.extent.height))
                .setMinDepth(0.0f)
                .setMaxDepth(1.0f);
        commandBuffer.setViewport(0, viewport);

        auto scissor = vk::Rect2D();
        scissor.setOffset({0, 0})
               .setExtent(swapChainInfo.extent);
        commandBuffer.setScissor(0, scissor);

        commandBuffer.bindVertexBuffers(0, {vertexBuffer}, {0}); // 绑定顶点缓冲区

        // 第1个参数是vertex count，也就是顶点数量
        // 第2个参数是instance count，也就是实例数量，不用instance就设置为1
        // 第3个参数是起始vertex index，也就是gl_VertexIndex的起始值，也能说是偏移值
        // 第4个参数是起始instance index，也就是gl_InstanceIndex的起始值，也能说是偏移值
        commandBuffer.draw(vertices.size(), 1, 0, 0);
        commandBuffer.endRenderPass();
        commandBuffer.end();

    }

    void CreateSyncObjects(){
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        auto semaphoreCreateInfo = vk::SemaphoreCreateInfo();
        auto fenceCreateInfo = vk::FenceCreateInfo();
        fenceCreateInfo.setFlags(vk::FenceCreateFlagBits::eSignaled); // 初始状态为signaled，防止第一次渲染的时候一直等待signaled导致死锁
    
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            imageAvailableSemaphores[i] = device.createSemaphore(semaphoreCreateInfo);
            renderFinishedSemaphores[i] = device.createSemaphore(semaphoreCreateInfo);
            inFlightFences[i] = device.createFence(fenceCreateInfo);
        }
    }

    void DrawPerFrame(){
        auto result = device.waitForFences(inFlightFences[currentFrame], true, UINT64_MAX); // 等待上一帧渲染完成
        if (result != vk::Result::eSuccess) {
            throw std::runtime_error("failed to wait for fence!");
        }
        uint32_t imageIndex;
        result = device.acquireNextImageKHR(swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], nullptr, &imageIndex); // 获取下一帧的imageIndex
        if (result == vk::Result::eErrorOutOfDateKHR) {
            RecreateSwapChain();
            return;
        } else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }
        device.resetFences(inFlightFences[currentFrame]); // 重置fence
        commandBuffers[imageIndex].reset(); // 重置command buffer
        RecordCommandBuffer(commandBuffers[imageIndex], imageIndex); // 记录command buffer

        auto submitInfo = vk::SubmitInfo();
        std::vector<vk::PipelineStageFlags> waitStages = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
        submitInfo.setWaitSemaphores(imageAvailableSemaphores[currentFrame])
                  .setWaitDstStageMask(waitStages)
                  .setCommandBuffers(commandBuffers[imageIndex])
                  .setSignalSemaphores(renderFinishedSemaphores[currentFrame]);
        graphicsQueue.submit(submitInfo, inFlightFences[currentFrame]); // 提交渲染命令

        auto presentInfo = vk::PresentInfoKHR();
        presentInfo.setWaitSemaphores(renderFinishedSemaphores[currentFrame])
                  .setSwapchains(swapChain)
                  .setImageIndices(imageIndex)
                  .setSwapchainCount(1);

        
        // 真抽象，C++中如果当前的swapchain过时了是直接抛出异常
        try{
            result = presentQueue.presentKHR(presentInfo); // 显示帧
        }
        catch(const vk::OutOfDateKHRError& e){
            framebufferResized = false;
            RecreateSwapChain();
        }
        if (result != vk::Result::eSuccess) {
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
        device.waitIdle();
        ClearSwapChain();
        CreateSwapChain();
        CreateImageViews();
        CreateFramebuffers();
    }

    void ClearSwapChain(){
        for (auto i = 0; i < framebuffers.size(); ++i){
            device.destroyFramebuffer(framebuffers[i]);
        }
        for (auto i = 0; i < swapChainInfo.imageViews.size(); ++i){
            device.destroyImageView(swapChainInfo.imageViews[i]);
        }
        device.destroySwapchainKHR(swapChain);
    }

    static void FramebufferResizeCallback(GLFWwindow* window, int width, int height) {
        auto app = reinterpret_cast<VulkanContext*>(glfwGetWindowUserPointer(window));
        app->framebufferResized = true;
    }

    void CreateVertexBuffers(){
        auto vertexBufferInfo = vk::BufferCreateInfo();
        vertexBufferInfo.setSize(vertices.size() * sizeof(Vertex))
                        .setUsage(vk::BufferUsageFlagBits::eVertexBuffer) // 指定这个数据的用途
                        .setSharingMode(vk::SharingMode::eExclusive); // 独占访问，不能给其他的队列使用
        vertexBuffer = device.createBuffer(vertexBufferInfo);
        // 上面是定义缓冲区,但是还没有分配GPU内存的

        // 这个能获取到指定的buffer的大小,偏移,和内存类型
        auto requirements = device.getBufferMemoryRequirements(vertexBuffer);
        // 查找合适的内存类型的索引
        auto memoryProperties = physicalDevice.getMemoryProperties();
        uint32_t memoryTypeIndex = -1;
        auto typeFilter = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
            if(requirements.memoryTypeBits & (1 << i) && (memoryProperties.memoryTypes[i].propertyFlags & typeFilter) == typeFilter){ // 找到合适的内存类型的位置
                memoryTypeIndex = i;
                break;
            }
        }
        if (memoryTypeIndex == -1) {
            throw std::runtime_error("failed to find suitable memory type!");
        }
        // 实际分配内存
        auto allocateInfo = vk::MemoryAllocateInfo();
        allocateInfo.setAllocationSize(requirements.size)
                    .setMemoryTypeIndex(memoryTypeIndex);
        vertexBufferMemory = device.allocateMemory(allocateInfo);
        device.bindBufferMemory(vertexBuffer, vertexBufferMemory, 0); // 偏移量,如果这块内存需要存储多个buffer就可以使用偏移量

        // 上面是创建buffer和分配内存,现在GPU中的准备工作做完了,所以需要将数据映射到GPU准备的buffer中去
        void *data;
        device.mapMemory(vertexBufferMemory, 0, vertexBufferInfo.size, vk::MemoryMapFlags(), &data); // 第一个0是偏移值,第二个是flag
        memcpy(data, vertices.data(), vertices.size() * sizeof(Vertex)); // 将数据拷贝到GPU内存中
        device.unmapMemory(vertexBufferMemory); // 解除映射
        // 这里有可能解除映射之后不会马上将数据复制到GPU的内存中去,所以上面的那个eHostCoherent枚举就起作用了,实际啥原理也不知道,文档讲的是一点都不清楚
    }


public:
    static VulkanContext* GetInstance(){
        std::call_once(_init_flag, CreateInstance); // 线程安全的
        return _ins.get();
    }
    ~VulkanContext(){}
};