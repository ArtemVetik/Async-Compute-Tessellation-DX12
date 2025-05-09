# Asynchronous Software Tessellation

> **Master’s Thesis Project — ITMO University**
> 
> [📄 Read the full thesis (PDF)]() *(will be available soon)*

A research prototype for **Asynchronous Software Tessellation**, developed as part of a Master’s thesis at [ITMO University](https://itmo.ru/). It demonstrates a GPU-based procedural tessellation algorithm integrated with asynchronous compute to overlap tessellation workloads with other rendering tasks (shadow mapping, post-processing, etc.) in DirectX 12.

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
