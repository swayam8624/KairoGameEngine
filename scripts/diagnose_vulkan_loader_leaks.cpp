// Minimal driver-only reproducer: intentionally no Kairo or GLFW code.
#include <vulkan/vulkan.h>
#include <cstdio>
#include <cstdint>
#include <vector>

int main()
{
    VkApplicationInfo app{};
    app.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName="Kairo Vulkan loader diagnostic";
    app.apiVersion=VK_API_VERSION_1_1;
    VkInstanceCreateInfo info{};
    info.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo=&app;
    VkInstance instance=VK_NULL_HANDLE;
    if(vkCreateInstance(&info,nullptr,&instance)!=VK_SUCCESS)return 2;
    std::uint32_t count=0;
    VkResult result=vkEnumeratePhysicalDevices(instance,&count,nullptr);
    if(result==VK_SUCCESS&&count)
    {
        std::vector<VkPhysicalDevice> devices(count);
        result=vkEnumeratePhysicalDevices(instance,&count,devices.data());
        if(result==VK_SUCCESS)
            for(std::uint32_t i=0;i<count;++i)
            {
                VkPhysicalDeviceProperties properties{};
                vkGetPhysicalDeviceProperties(devices[i],&properties);
                std::printf("Device: %s\n",properties.deviceName);
            }
    }
    vkDestroyInstance(instance,nullptr);
    std::printf("Destroyed instance; enumeration result=%d, devices=%u\n",int(result),count);
    return result==VK_SUCCESS&&count?0:3;
}
