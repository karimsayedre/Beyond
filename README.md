# Beyond Engine  

**Beyond Engine** is a modern, high-performance graphics engine built using Vulkan, designed with flexibility, scalability, and cutting-edge rendering techniques in mind.  

Initially developed as a fork of the Hazel Engine, Beyond Engine has undergone extensive optimizations and feature additions, including state-of-the-art technologies such as DLSS and ray tracing. It is the culmination of a relentless pursuit of rendering excellence.  

## Acknowledgments
Special thanks to the Studio Cherno for laying the foundation and everyone who contributed to the development and success of Hazel Engine which served as the starting point for Beyond Engine.


# Demo Images
![Beyond Engine Demo Image 1](https://karimsayedre.github.io/images/Pathtracing/0.jpg?raw=true)

![Beyond Engine Demo Image 2](https://karimsayedre.github.io/images/Pathtracing/1.jpg?raw=true)

![Beyond Engine Demo Image 3](https://karimsayedre.github.io/images/Pathtracing/21.jpg?raw=true)
Checkout my [website](https://karimsayedre.github.io) for more images.

---

## Features  

### 🚀 Performance Optimizations  
- **Ray Tracing**: Hardware-accelerated ray tracing with advanced synchronization techniques.  
- **DLSS Integration**: Super-sampling with NVIDIA's Deep Learning-based solution for superior frame rates.  
- **CPU-side Optimizations**: Significant performance improvements for multi-threaded workloads.  
- **Data-oriented Design**: Memory usage and cache efficiency are optimized for modern GPUs.  


### 🌟 Cutting-edge Rendering  
- **Path Tracing**: Fully integrated path tracer with realistic global illumination.    
- **Importance Sampling**: GGX importance sampling for reflections and refractions.  
- **DLSS**: Deep Learning-based Super-Sampling for improved image quality and sharpness.
- **Tonemapping**: HDR to LDR tonemapping using AGX, PBR Neutral, and ACES.  
- **Bindless Descriptor Sets**: Efficient and scalable descriptor set management.  
- **glTF 2.0 Support**: Almost full glTF 2.0 material support with path tracing.  

### 🛠️ Tooling and Workflow  
- **Premake Build System**: Streamlined project setup and maintenance.  
- **Hot Reloading**: Live++ integration for seamless development cycles (Debug builds only).  
- **Debugging**: Enhanced diagnostics using C++23 stack trace functionality.  

---

## Getting Started  

### Prerequisites  
- **Graphics API**:
- Visual Studio 2022 (Windows)
- Vulkan SDK.  
- Git
- Git-LFS
- Python 3

### Build Instructions  
1. Clone the repository:  
   ```bash  
   git clone https://github.com/karimsayedre/Beyond.git --recursive  
   ```

2. Run the Setup script:  
   ```bash  
    cd Beyond/scripts  
    ./Setup.bat  
   ```
3. Open the Beyond.sln file in Visual Studio 2022.

4. Build the solution.


## License
This project is licensed under the Apache 2.0 License. See the LICENSE file for details.
It does not include commit history of Hazel Engine, which is not open-source.
