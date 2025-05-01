#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <memory>
#include <iostream>
#include <mutex>

class VulkanContext final {
private:
    // 使用inline内联初始化
    static inline std::once_flag _init_flag;
    static inline std::unique_ptr<VulkanContext> _ins = nullptr;

    VkInstance* vkIns;
    GLFWwindow* window = nullptr;

    VulkanContext(int width = 800, int height = 600){
        // 在构造函数中来实现Vulkan的整体流程
        // vkInstanceCreateInfo是C语言的写法，然后C++的写法就是vk::InstanceCreateInfo{}，但是这种写法是使用原生的vulkan头文件，我这边使用的是glfw框架来实现跨平台的操作
        // vk::InstanceCreateInfo createInfo{}; 

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
            // vulkan中有很多layer，如果忘记了layer叫啥名字，可以通过这样来遍历所有的layer
            // VkLayerProperties *pProperties;
            // uint32_t* layerCount;
            // vkEnumerateInstanceLayerProperties(layerCount, pProperties);
            // for (uint32_t i = 0; i < *layerCount; i++) {
            //     std::cout << pProperties[i].layerName << std::endl;
            // }
            const char* validationLayers[] = { "VK_LAYER_KHRONOS_validation" };
            // 启用validation layer
            createInfo.ppEnabledLayerNames = validationLayers;
        #endif

        // 有创建就有销毁
        vkCreateInstance(&createInfo, nullptr, vkIns);

        // 初始化glfw
        glfwInit();
        // 设置窗口属性，历史原因glfw最初是用来创建OpenGL上下文的，为了兼容Vulkan，这里需要告诉glfw不要创建OpenGL上下文
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);   // 禁止修改窗口大小
        // 根据上面的属性来创建窗口
        window = glfwCreateWindow(width, height, "Hello Vulkan", nullptr, nullptr);
         // 第四个参数表示是否是全屏，第五个参数是共享指定窗口的上下文，最后一个参数只和OpenGL相关
    
        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

        Init();

        while(!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            Update();
        }

        Destroy();
    }
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;
    static void CreateInstance(){
        _ins.reset(new VulkanContext);
    }
public:
    static VulkanContext* GetInstance(){
        std::call_once(_init_flag, CreateInstance); // 线程安全的
        return _ins.get();
    }
    ~VulkanContext(){
        vkDestroyInstance(*vkIns, nullptr);

        // 清理创建的窗口资源
        glfwDestroyWindow(window);
        // 关闭窗口
        glfwTerminate();
    }

    // 定义一些基本的生命周期函数
    void Init(){

    }
    void Update(){

    }
    void Destroy(){

    }
};