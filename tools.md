# 推荐工具清单

必做报告只需计算器、纸笔或 Markdown。下面工具用于出题人验证和参与者选做，不是交付要求。

| 优先级 | 工具 | 适用场景与证据 |
| --- | --- | --- |
| 首先使用 | 示例程序 + 优化编译器 | 正确性、重复耗时、吞吐；先确认计时范围 |
| 推荐 | 纸笔 / Mermaid / draw.io | 五点依赖图、tile 复用图、带单位的 Roofline 图 |
| CPU 选做 | perf stat | cycles、instructions、IPC、支持的缓存事件 |
| CPU 选做 | perf record/report | 区分目标 kernel 与 FP64 参考函数等热点 |
| 深入选做 | Intel VTune | 受支持 CPU 上的热点、内存带宽、NUMA 分析 |
| CUDA 移植后 | Nsight Systems | kernel、传输与同步时间线 |
| CUDA 移植后 | Nsight Compute | kernel 的 MemoryWorkloadAnalysis、SpeedOfLight 等 |

## Linux perf 的最小流程

从本目录编译后运行：

```bash
perf --version
perf list
mkdir -p results
perf stat -e cycles,instructions -o results/stencil-stat.txt -- ./build/stencil 1024 1024 20 row 5
perf stat -e cache-references,cache-misses -o results/gemm-cache.txt -- ./build/gemm 512 blocked 64 5
perf record -g -o results/gemm.perf.data -- ./build/gemm 512 blocked 64 5
perf report --stdio -i results/gemm.perf.data > results/gemm-hotspots.txt
```

对照另一实现时保存不同文件名。缓存事件不支持时记录“不支持”，不要填零；不要要求参与者提权或关闭系统限制。所有实验仅在 Linux 中运行；WSL 2 的 PMU 可用性需在实际环境中检查，缺失事件应标为不可用。

解释时注意：

- 上述 stat 是 **whole-process**，包括 FP64 参考计算、校验、初始化和打印；不能将其除以单个 kernel 的 `compute_ms`。尤其大 GEMM 参考计算可能显著影响统计。
- record/report 用于查看 `gemm_blocked`、`gemm_naive`、`stencil_step` 等实际热点；优化器可能内联，必要时查看源码/反汇编归属。若要 kernel 区域精确计数，后续需另加区域采集，当前示例没有实现。
- IPC 低不等于带宽饱和；cache-misses 不自动等于 L1 miss，也不能仅靠它推算 DRAM 字节数。
- 减少同次事件数量，关注计数器复用比例；测试工具采样的耗时与独立 benchmark 分开。

## 编译器的辅助证据

GCC 可在原编译命令上增加 `-fopt-info-vec-optimized -fopt-info-vec-missed`，观察 row/column 或 naive/blocked 是否产生不同向量化。连续访问版本更快也可能包含 SIMD 收益，不能全部归因于 DRAM。

## GPU 工具何时使用

本包没有 CUDA kernel，不能用 ncu 对现有 CPU 程序证明 GPU 瓶颈。若自行移植，先用 Systems 确认传输和启动是否主导，再用 Compute 分析目标 kernel；用相同精度、规模和时序做对照。ncu 可能重放、改变缓存状态，不能以整个采集任务耗时作为性能成绩。

## 官方参考

- [perf stat](https://man7.org/linux/man-pages/man1/perf-stat.1.html)、[perf record](https://man7.org/linux/man-pages/man1/perf-record.1.html)、[perf list](https://man7.org/linux/man-pages/man1/perf-list.1.html)
- [VTune Memory Access](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2024-0/memory-access-analysis.html)
- [Nsight Systems](https://docs.nvidia.com/nsight-systems/UserGuide/)、[Nsight Compute 的采集与重放](https://docs.nvidia.com/nsight-compute/ProfilingGuide/)
