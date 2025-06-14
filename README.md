# Asynchronous Software Tessellation

> **Master’s Thesis Project — ITMO University**

A research prototype for **Asynchronous Software Tessellation**, developed as part of a Master’s thesis at [ITMO University](https://itmo.ru/). It demonstrates a GPU-based procedural tessellation algorithm integrated with [asynchronous compute](https://developer.nvidia.com/blog/advanced-api-performance-async-compute-and-overlap/) to overlap tessellation workloads with other rendering tasks (shadow mapping, post-processing, etc.) in DirectX 12.

[Watch Demo Video](https://youtu.be/GGQwWtlLwLY)

<p align="center">
  <img src="https://github.com/ArtemVetik/Async-Compute-Tessellation-DX12/blob/main/docs/terrain-high-tessellation.png" width="49%">
  <img src="https://github.com/ArtemVetik/Async-Compute-Tessellation-DX12/blob/main/docs/terrain-high-tessellation-color.png" width="49%">
</p>

<p align="center">
  <img src="https://github.com/ArtemVetik/Async-Compute-Tessellation-DX12/blob/main/docs/teapot-wireframe.png" width="49%">
  <img src="https://github.com/ArtemVetik/Async-Compute-Tessellation-DX12/blob/main/docs/teapot-color.png" width="49%">
</p>

---

## ✨ Features

- **Compute‐Shader Tessellation**  
  - Procedural subdivision via binary‐rule keys stored in GPU buffers.  
  - Both uniform and view‐dependent adaptive LOD.
  - Frustrum culling
- **Asynchronous Compute**  
  - Overlaps tessellation compute with shadow‐map and post‐processing passes.  
  - Camera‐motion prediction to drive next‐frame tessellation.  
- **Graphics Techniques**
  - Deferred rendering.
  - Shadow mapping (Cascaded Shadow Maps).  
  - Post‐processing: Motion Blur, Bloom, Chromatic Aberration, Tone Mapping.  
- **Interactive UI**  
  - ImGui overlay to tweak LOD, toggle async/sync, enable/disable effects.  
  - Frame-time display and live profiling markers.

---

## 📊 Workflow and Sequence Diagram

This section illustrates the overall application workflow and the detailed rendering sequence for a single frame utilizing asynchronous compute.

<p align="center">
  <img src="https://github.com/ArtemVetik/Async-Compute-Tessellation-DX12/blob/main/docs/app-workflow.png" width="69%">
  <img src="https://github.com/ArtemVetik/Async-Compute-Tessellation-DX12/blob/main/docs/async-compute-with-postprocess-diagram.png" width="30%">
</p>

## 📈 Results

The implementation was evaluated across eight GPU configurations (GTX 1050 Ti, GTX 1080 Ti, RTX 2080 Ti, RTX 3060 Laptop, RTX 3080, RTX 3090, RTX 4070 Ti, RTX 4090) and five application scenarios:

* Config 1: High-detail procedural terrain.
* Config 2: High-detail 3D model.
* Config 3: Low-detail 3D model.
* Config 4: Low-detail procedural terrain.
* Config 5: High-detail procedural terrain with simplified pixel shader.

Each scenario included cascaded shadow maps, multiple colored point lights, full post‑processing effects, and a rotating camera.

### 

### Config 1 Results (RTX 4070 Ti)

Asynchronous tessellation modes yielded a 10–30% reduction in frame time, with AsyncAll often the top performer. Parallelizing tessellation alongside shadow‐map generation maximized GPU utilization and reduced idle periods.

![result-01](https://github.com/ArtemVetik/Async-Compute-Tessellation-DX12/blob/main/docs/result-01.png)

The chart below, captured in NVIDIA Nsight Graphics, illustrates the distribution of GPU and memory resources for Config 1 in Direct mode (A) versus AsyncShadowMap mode (B).

![async-sync-comp](https://github.com/ArtemVetik/Async-Compute-Tessellation-DX12/blob/main/docs/async-sync-comp.png)

### Config 2 Results (RTX 1050 Ti)

AsyncAll and AsyncShadowMap modes were most stable, delivering 5–20% gains despite a heavy 2.5 million‑triangle load. AsyncPostProcess underperformed due to high VRAM pressure from simultaneous texture sampling.

![result-02](https://github.com/ArtemVetik/Async-Compute-Tessellation-DX12/blob/main/docs/result-02.png)

### Config 3 Results (RTX 3060 Laptop)

With only ~300 k vertices (low tessellation), overheads from queue synchronization offset compute gains, yet AsyncAll still led marginal improvements.

![result-03](https://github.com/ArtemVetik/Async-Compute-Tessellation-DX12/blob/main/docs/result-03.png)

### Config 4 Results (RTX 3090)

While AsyncPostProcess saw minimal speedup, running tessellation alongside shadow rendering proved most effective. Overall, AsyncAll averaged a 2–10% frame time reduction due to fewer synchronization barriers.

![result-04](https://github.com/ArtemVetik/Async-Compute-Tessellation-DX12/blob/main/docs/result-04.png)

### Config 5 Results (RTX 3080)

Simplifying the pixel shader reduced G‑buffer costs, shifting the bottleneck to compute. As a result, asynchronous gains varied but AsyncAll remained the safest choice for consistent improvements (20–60% gains).

![result-05](https://github.com/ArtemVetik/Async-Compute-Tessellation-DX12/blob/main/docs/result-05.png)

---

## ❓ FAQ / Questions & Answers

**Q: Why were mesh shaders not considered in this work and what impact could they have on the proposed approach?**  
**A:** I did not consider mesh shaders because my main hypothesis was focused on using asynchronous compute. I aimed to optimize software tessellation algorithms executed in compute shaders. If mesh shaders were used, tessellation would have to run in the mesh stage (which executes in the graphics queue, not the compute queue), invalidating my hypothesis about asynchronous compute. Additionally, there is already an implementation of software tessellation in three variants—using compute shaders, using mesh shaders, and using hardware tessellation—allowing a performance comparison of these three approaches.

**Q: Why were GPU-Driven Pipeline techniques and the reasons for their adoption not examined and analyzed in this work? I would also like to evaluate these techniques in the context of a GPU-Driven Pipeline.**  
**A:** I did not detail GPU-Driven Pipeline techniques in the thesis (though I indirectly mentioned them in the document and presentation) because the tessellation algorithm itself inherently uses GPU-Driven Pipeline methods: for example, frustum culling runs entirely on the GPU without CPU interaction, and I use Indirect Draw, which also offloads rendering tasks from the CPU. Overall, the entire algorithm can run independently on the GPU. The only data transferred from CPU to GPU is the camera’s position; otherwise, the algorithm uses GPU-Driven Pipeline methods. It is also important that when testing the algorithm, I measured GPU time using special GPU counters (as shown in the charts), so time spent synchronizing with the CPU was not included and would not affect the results.

**Q: Why was DirectX 12 chosen instead of Vulkan?**  
**A:** I chose DirectX 12 because Windows is currently the most popular gaming OS, and DirectX 12 is the best graphics API for Windows.

**Q: How are the GPU blocks loaded when running the tessellation algorithm in synchronous and asynchronous modes?**  
**A:** On NVIDIA Turing architecture, a pixel shader runs on an SM and requests SRV/UAV resources via the L1Tex (texture cache and texture pipeline). A miss in L1 sends the request to L2, and then to VRAM if needed. Finally, the pixel shader writes color using the CROP (Color Raster Operation) block. In my tessellation implementation, mathematical operations heavily utilize SM cores, so it makes sense to overlap tessellation with work that mainly uses rasterization modules (CROP, PROP, RASTER, etc.). In asynchronous mode, VRAM load also increases due to cache misses, which is a potential area for improvement. Thus, the main goal in asynchronous mode was to reduce the number of idle warps and balance load across GPU modules without conflicts. Here’s a great talk on optimizing GPU workloads: [https://www.gdcvault.com/play/1026202/Optimizing-DX12-DXR-GPU-Workloads](https://www.gdcvault.com/play/1026202/Optimizing-DX12-DXR-GPU-Workloads)

## 🔧 Prerequisites & Build

- **Windows 10/11**  
- **Visual Studio 2022** (or later) with **Desktop Development with C++** workload  
- **DirectX 12 SDK** (installed via Windows SDK)

## 🛠 How to Build

All dependencies are included. Just:

1. Open `AsyncComputeTessellation.sln` in **Visual Studio 2022**.
2. Select x64 and Debug/Release configuration.
3. Build (`Build` > `Build Solution`).
4. Run (`Debug` > `Start Without Debugging` or `Ctrl+F5`).

## Third-party libraries

- [Assimp 5.4.3](http://www.assimp.org)
- [ImGui](https://github.com/ocornut/imgui)
- [DirectXShaderCompiler (DXC)](https://github.com/microsoft/DirectXShaderCompiler)

## 🔗 Some Important Links
* [Direct3D12 User-Mode Heap Synchronization (Microsoft)](https://learn.microsoft.com/en-us/windows/win32/direct3d12/user-mode-heap-synchronization)
* [Advanced API Performance: Async Compute and Overlap (NVIDIA)](https://developer.nvidia.com/blog/advanced-api-performance-async-compute-and-overlap/)
* [Khoury J. et al. Adaptive GPU tessellation with compute shaders](https://www.onrendering.com/data/papers/isubd/isubd.pdf)
* [Khoury, J. GPU Tessellation with Compute Shaders: Master Thesis. EPFL, 2018. 57 pp.](https://jadkhoury.github.io/files/MasterThesisFinal.pdf)
* [Optimizing DX12/DXR GPU Workloads (GDC)](https://www.gdcvault.com/play/1026202/Optimizing-DX12-DXR-GPU-Workloads)
* [AMD Dives Deep on Asynchronous Shading (AnandTech)](https://www.anandtech.com/show/9124/amd-dives-deep-on-asynchronous-shading)
* [Life in the Triangle: NVIDIA’s Logical Pipeline](https://developer.nvidia.com/content/life-triangle-nvidias-logical-pipeline)
* [Concurrent Execution & Asynchronous Queues (GPUOpen)](https://gpuopen.com/learn/concurrent-execution-asynchronous-queues/)
* [Harness Powerful Shader Insights Using Shader Debug Info with NVIDIA Nsight Graphics](https://developer.nvidia.com/blog/harness-powerful-shader-insights-using-shader-debug-info-with-nvidia-nsight-graphics)
