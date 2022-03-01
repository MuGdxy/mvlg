#include <vulkan/vulkan.h>

PFN_vkCreateDebugUtilsMessengerEXT  pfnVkCreateDebugUtilsMessengerEXT;
PFN_vkDestroyDebugUtilsMessengerEXT pfnVkDestroyDebugUtilsMessengerEXT;

#define SetFunPtr(x,ins,name) (x) = (decltype(x))vkGetInstanceProcAddr(ins,name);


template<typename Func, typename Addr>
void Set(Func& func, Addr addr) {func = (Func)addr;}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugUtilsMessengerEXT(
    VkInstance                                  instance,
    const VkDebugUtilsMessengerCreateInfoEXT*   pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkDebugUtilsMessengerEXT*                   pMessenger)
{
    static PFN_vkCreateDebugUtilsMessengerEXT  pfnVkCreateDebugUtilsMessengerEXT = nullptr;
    static VkInstance                          mInstance = nullptr;
    if (!pfnVkCreateDebugUtilsMessengerEXT || mInstance != instance)
    {
        Set(pfnVkCreateDebugUtilsMessengerEXT, 
            vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
        if (pfnVkCreateDebugUtilsMessengerEXT) mInstance = instance;
    }
    return pfnVkCreateDebugUtilsMessengerEXT(instance, pCreateInfo, pAllocator, pMessenger);
}

VKAPI_ATTR void VKAPI_CALL vkDestroyDebugUtilsMessengerEXT(VkInstance                    instance,
    VkDebugUtilsMessengerEXT      messenger,
    VkAllocationCallbacks const* pAllocator)
{
    static PFN_vkDestroyDebugUtilsMessengerEXT pfnVkDestroyDebugUtilsMessengerEXT = nullptr;
    static VkInstance                          mInstance = nullptr;
    if (!pfnVkCreateDebugUtilsMessengerEXT || mInstance != instance)
    {
        Set(pfnVkDestroyDebugUtilsMessengerEXT,
            vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (pfnVkDestroyDebugUtilsMessengerEXT) mInstance = instance;
    }
    return pfnVkDestroyDebugUtilsMessengerEXT(instance, messenger, pAllocator);
}