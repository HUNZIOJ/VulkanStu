#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

// 这个glm是VulkanSDK自带的库，用来进行线性代数的计算，这个glm是直接复制整个项目的源代码到编译器目录下面
// 所以以后只要使用这个mingw编译器并且用到了这个glm库，就不需要下载glm复制到项目中
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <memory>
#include <iostream>

class HelloTriangleApplication {
    public:
        GLFWwindow* window = nullptr;
        void run() {
            initVulkan();
            mainLoop();
            cleanup();
        }
    
    private:
        VkInstance instance;

        void initVulkan(int width = 800, int height = 600) {
            // 创建Vulkan实例
            createInstance();

            // 初始化glfw
            glfwInit();
            // 设置窗口属性，历史原因glfw最初是用来创建OpenGL上下文的，为了兼容Vulkan，这里需要告诉glfw不要创建OpenGL上下文
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);   // 禁止修改窗口大小
            // 根据上面的属性来创建窗口
            window = glfwCreateWindow(width, height, "Vulkan window", nullptr, nullptr);
             // 第四个参数表示是否是全屏，第五个参数是共享指定窗口的上下文，最后一个参数只和OpenGL相关
        
            uint32_t extensionCount = 0;
            vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
            std::vector<VkExtensionProperties> extensions(extensionCount);
            vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
            std::cout << "available extensions:\n";

            for (const auto& extension : extensions) {
                std::cout << '\t' << extension.extensionName << '\n';
            }
        }

        void createInstance() {
            VkApplicationInfo appInfo{};
            appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            appInfo.pApplicationName = "Hello Triangle";
            appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
            appInfo.pEngineName = "No Engine";
            appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
            appInfo.apiVersion = VK_API_VERSION_1_0;

            VkInstanceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            createInfo.pApplicationInfo = &appInfo;
            uint32_t glfwExtensionCount = 0;
            const char** glfwExtensions;
            glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
            createInfo.enabledExtensionCount = glfwExtensionCount;
            createInfo.ppEnabledExtensionNames = glfwExtensions;
            createInfo.enabledLayerCount = 0;

            VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
        }
    
        void mainLoop() {
            while(!glfwWindowShouldClose(window)) {
                glfwPollEvents();
            }
        }
    
        void cleanup() {
            // 清理创建的窗口资源
            glfwDestroyWindow(window);
            // 关闭窗口
            glfwTerminate();
        }
};

int main() {

    HelloTriangleApplication app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}