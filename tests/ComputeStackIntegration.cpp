#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <vector>

import Kairo.SIMD;
import Kairo.Scheduler;
import Kairo.GPU;
import Kairo.ONNX;
import Kairo.ONNX.Runtime;
import Kairo.Transformers;
import Kairo.Foundation.Math.Tensor;

int main(int argc, char** argv)
{
    constexpr std::size_t count = 4096u;
    std::vector<float> first(count);
    std::vector<float> second(count);
    std::vector<float> sum(count);

    kairo::scheduler::ThreadPool pool(4u);
    kairo::scheduler::ParallelFor(pool, count, 128u,
        [&](kairo::scheduler::Range range)
        {
            for (std::size_t index = range.begin; index < range.end; ++index)
            {
                first[index] = static_cast<float>(index % 97u) * 0.01f - 0.5f;
                second[index] = static_cast<float>(index % 53u) * 0.02f - 0.25f;
            }
        });

    kairo::simd::Add<float>(sum, first, second);
    double simdError = 0.0;
    for (std::size_t index = 0u; index < count; ++index)
        simdError = std::max(simdError, std::abs(
            static_cast<double>(sum[index])
            - static_cast<double>(first[index] + second[index])));

    bool gpuChecked = false;
    double gpuError = 0.0;
    if (kairo::gpu::IsBackendCompiled(kairo::gpu::Backend::Metal))
    {
        kairo::gpu::Device device({
            .backend = kairo::gpu::Backend::Metal,
            .debugName = "kairo-compute-integration" });
        if (device.IsAvailable())
        {
            gpuChecked = true;
            std::vector<float> gpuOutput(count, 0.0f);
            const auto a = device.CreateBuffer({
                .byteSize = sum.size() * sizeof(float),
                .usage = kairo::gpu::BufferUsage::Storage,
                .debugName = "simd-output" });
            const auto b = device.CreateBuffer({
                .byteSize = second.size() * sizeof(float),
                .usage = kairo::gpu::BufferUsage::Storage,
                .debugName = "scheduler-output" });
            const auto out = device.CreateBuffer({
                .byteSize = gpuOutput.size() * sizeof(float),
                .usage = kairo::gpu::BufferUsage::Storage,
                .debugName = "gpu-output" });
            device.Upload(a, std::as_bytes(std::span(sum)));
            device.Upload(b, std::as_bytes(std::span(second)));
            device.VectorMultiplyFloat(a, b, out, count);
            device.Download(out, std::as_writable_bytes(std::span(gpuOutput)));
            for (std::size_t index = 0u; index < count; ++index)
                gpuError = std::max(gpuError, std::abs(
                    static_cast<double>(gpuOutput[index])
                    - static_cast<double>(sum[index] * second[index])));
        }
    }

    kairo::onnx::Graph graph;
    graph.inputs.push_back({ .name = "x", .shape = { 1, static_cast<std::int64_t>(count) } });
    graph.outputs.push_back({ .name = "y", .shape = { 1, static_cast<std::int64_t>(count) } });
    graph.nodes.push_back({
        .name = "relu",
        .op = kairo::onnx::OpKind::Relu,
        .inputs = { "x" },
        .outputs = { "y" } });

    kairo::foundation::math::Tensor<float> input({ 1u, count });
    for (std::size_t index = 0u; index < count; ++index) input[index] = sum[index];
    kairo::onnx::RuntimeBindings feeds;
    feeds.emplace("x", std::move(input));
    const auto inference = kairo::onnx::ExecuteGraph(graph, feeds);
    const auto& relu = std::get<kairo::foundation::math::Tensor<float>>(
        inference.outputs.at("y"));

    double onnxError = 0.0;
    for (std::size_t index = 0u; index < count; ++index)
        onnxError = std::max(onnxError, std::abs(
            static_cast<double>(relu[index])
            - static_cast<double>(std::max(0.0f, sum[index]))));

    kairo::transformers::TransformerConfig config{};
    config.vocabularySize = 256u;
    config.contextLength = 32u;
    config.modelWidth = 4u;
    config.headCount = 2u;
    config.layerCount = 1u;
    config.feedForwardWidth = 8u;
    if (!config.Valid()) return 5;

    kairo::foundation::math::Tensor<float> activation({ 2u, 4u });
    for (std::size_t index = 0u; index < activation.Size(); ++index)
        activation[index] = relu[index];
    const kairo::foundation::math::Tensor<float> scale({ 4u }, 1.0f);
    const kairo::foundation::math::Tensor<float> bias({ 4u }, 0.0f);
    const auto normalized =
        kairo::transformers::LayerNorm(activation, scale, bias);
    double transformerChecksum = 0.0;
    for (std::size_t index = 0u; index < normalized.Size(); ++index)
    {
        if (!std::isfinite(normalized[index])) return 6;
        transformerChecksum += normalized[index];
    }

    const auto stats = pool.Stats();
    const bool correct = simdError == 0.0
        && onnxError <= 1.0e-6
        && (!gpuChecked || gpuError <= 1.0e-6)
        && stats.completedTasks > 0u;

    const std::string json =
        "{\n"
        "  \"schema\": \"kairo.compute.integration.v1\",\n"
        "  \"elements\": " + std::to_string(count) + ",\n"
        "  \"scheduler_completed_tasks\": "
            + std::to_string(stats.completedTasks) + ",\n"
        "  \"simd_backend\": \""
            + std::string(kairo::simd::FeatureName(kairo::simd::DetectedFeature()))
            + "\",\n"
        "  \"simd_max_abs_error\": " + std::to_string(simdError) + ",\n"
        "  \"gpu_checked\": " + std::string(gpuChecked ? "true" : "false") + ",\n"
        "  \"gpu_max_abs_error\": " + std::to_string(gpuError) + ",\n"
        "  \"onnx_max_abs_error\": " + std::to_string(onnxError) + ",\n"
        "  \"transformer_layernorm_checksum\": "
            + std::to_string(transformerChecksum) + ",\n"
        "  \"correct\": " + std::string(correct ? "true" : "false") + "\n"
        "}\n";

    std::cout << json;
    if (argc > 1)
    {
        std::ofstream output(argv[1], std::ios::binary | std::ios::trunc);
        if (!output) return 2;
        output << json;
        if (!output) return 3;
    }
    return correct ? 0 : 1;
}
