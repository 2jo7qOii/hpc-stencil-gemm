# HPC 第二关：stencil 与 GEMM 的性能瓶颈判断与优化决策

判断两个经典 HPC 内核（五点 stencil、矩阵乘法）的性能瓶颈，通过**成对对照实验**定位限制因素并给出优化决策。

## 结论速览

| 实验 | 对照 | 实测差距 | 瓶颈定位 |
|---|---|---|---|
| stencil | `row` vs `column` | **20.70×**（1024²，steps=20） | `column` **受限于访存**（读流量放大 16 倍）|
| gemm | `naive` vs `blocked` | **1.70× → 3.86×**（N=256 → 1024） | `blocked` **受限于标量算力**（带宽仅用 5%）|

**三个关键发现**

1. **程序的"有效带宽"指标对 `column` 失效**。它输出的 `0.285 GB/s` 比机械硬盘还慢，物理上不可能——因为分子 `8 × updates` 是基于"理想空间复用"的常量，与遍历顺序无关。用修正模型还原后，两者真实带宽为 5.91 与 2.42 GB/s，**同一量级**。
2. **分块把瓶颈从内存推到了算力**。`gemm blocked` 搬运量降到 naive 的 1/42，但加速只有 3.86 倍——因其 GFLOP/s（约 3.0）已达基频下标量指令上限的 96%。
3. **`tile` 调参已饱和**。实测拟合 `T ≈ 84.9 + 428.5/tile`（毫秒，偏差 < 4%），其中 `84.9 ms` 是与 `tile` 无关的计算下限。

## 文件结构

```
.
├── stencil.cpp / gemm.cpp / common.hpp   # 被测程序源码
├── tools.md                              # 附带的工具说明
├── results/                              # 22 个原始测量输出（对照实验证据）
└── report/
    ├── report.pdf                        # ⭐ 实验报告（最终交付物）
    ├── report.md                         # 报告源文件（Markdown）
    ├── full.html                         # 打印优化版 HTML
    ├── 打印优化.css                       # 打印样式（字号/边距/分页规则）
    └── mkfull.mjs                        # Markdown → HTML 生成脚本
```

## 报告包含

公式推导、成对实验数据、瓶颈判断、优化决策，以及六项要求指标：

- 正确性结果（`check=PASS`，成对实现的 `checksum` 逐位一致）
- 平均时间 / 中位数 / **总体**标准差
- GFLOP/s
- 有效带宽（含"模型值 vs 真实值"的辨析）

另附三份 SIMD 证据：编译器向量化诊断、`objdump` 的 `ymm` 计数（**0**）、`-mno-avx2` 消融实验。

## 复现

```bash
g++ -std=c++17 -O2 -march=native -o stencil stencil.cpp
g++ -std=c++17 -O2 -march=native -o gemm    gemm.cpp

./stencil 1024 1024 20 row    5
./stencil 1024 1024 20 column 5
./gemm    512  naive   64 5
./gemm    512  blocked 64 5
```

**环境**：Intel Core i5-8250U / g++ 13.3.0 / Ubuntu 24.04

**注意**：报告中的耗时数据受移动平台热降频影响，同一二进制的 `stencil row` 中位数在不同批次间相差约 21%，故结论基于**同批次配对数据**，不跨批次比较绝对耗时。

## 重新生成报告

```bash
cd report
npm install jsdom          # 首次
node mkfull.mjs            # 生成 full.html
# 浏览器打开 full.html → Ctrl+P → 另存为 PDF
```
