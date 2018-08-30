/*******************************************************************************
 *
 * MIT License
 *
 * Copyright (c) 2018 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *******************************************************************************/
#ifndef MIOPEN_FUSION_HPP_
#define MIOPEN_FUSION_HPP_

#include <miopen/common.hpp>
#include <miopen/errors.hpp>
#include <miopen/handle.hpp>
#include <miopen/miopen.h>
#include <miopen/tensor.hpp>
#include <miopen/activ.hpp>
#include <miopen/convolution.hpp>
#include <miopen/op_kernel_args.hpp>
#include <miopen/fusion_ops.hpp>

#include <set>
#include <vector>
#include <unordered_map>

namespace miopen {

enum FusionKernelSourceType
{
    OpenclText,
    AsmText,
    Binary,
};

using any_t = OpKernelArg;
struct OperatorArgs : miopenOperatorArgs
{
    OperatorArgs();
    void ins_arg(std::string name, any_t v);
    friend std::ostream& operator<<(std::ostream& stream, const OperatorArgs& x);
    std::vector<any_t> args_vec;
    std::unordered_map<std::string, any_t> args_map;
};

struct FusionOpDescriptor : miopenFusionOpDescriptor
{
    virtual ~FusionOpDescriptor()                 = default;
    FusionOpDescriptor(const FusionOpDescriptor&) = delete;
    FusionOpDescriptor()                          = default;
    FusionOpDescriptor& operator=(const FusionOpDescriptor&) = delete;
    void SetIdx(int _id) { plan_idx = _id; };
    int GetIdx() const { return plan_idx; };
    virtual FusionMDGraph_Edge_Map MDGraphKey() const
    {
        return {{"weight", EdgeOp(0, true, OpAny)}};
    };
    virtual miopenStatus_t GetOutputDesc(TensorDescriptor& output_desc) = 0;
    virtual miopenStatus_t GetNetworkConfig(std::string& network_config, Handle& handle);
    virtual miopenStatus_t
    GetCompileParms(std::string& compile_config, Handle& handle, FusionKernelSourceType source);
    friend std::ostream& operator<<(std::ostream& stream, const FusionOpDescriptor& x);
    virtual miopenFusionOp_t kind() const            = 0;
    virtual std::vector<std::string> GetArgs() const = 0;
    virtual std::vector<size_t> GetLocalWGSz(Handle& handle, std::string algorithm_name);
    virtual std::vector<size_t> GetGlobalWGSz(Handle& handle, std::string algorithm_name);
    void SetInputDesc(TensorDescriptor i_desc) { input_desc = i_desc; };
    TensorDescriptor input_desc;

    private:
    int plan_idx                       = 0;
    std::shared_ptr<OperatorArgs> args = nullptr;
};

struct ActivFusionOpDescriptor : FusionOpDescriptor
{
    ActivFusionOpDescriptor(miopenActivationMode_t mode) : activMode(mode){};
    miopenStatus_t GetOutputDesc(TensorDescriptor& output_desc) override;
    miopenStatus_t GetNetworkConfig(std::string& network_config, Handle& handle) override;
    miopenStatus_t GetCompileParms(std::string& compile_config,
                                   Handle& handle,
                                   FusionKernelSourceType source) override;
    miopenStatus_t SetArgs(OperatorArgs& args,
                           const void* alpha,
                           const void* beta,
                           double activAlpha,
                           double activBeta,
                           double activGamma);
    std::vector<std::string> GetArgs() const override;
    miopenFusionOp_t kind() const override { return miopenFusionOpActivForward; };
    FusionMDGraph_Edge_Map MDGraphKey() const override;
    static FusionMDGraph_Edge_Map MDGraphKey(miopenActivationMode_t mode);
    std::vector<size_t> GetLocalWGSz(Handle& handle, std::string algorithm_name) override;
    std::vector<size_t> GetGlobalWGSz(Handle& handle, std::string algorithm_name) override;
    miopenActivationMode_t activMode;
};

struct BatchNormInferenceFusionOpDescriptor : FusionOpDescriptor
{
    BatchNormInferenceFusionOpDescriptor(miopenBatchNormMode_t bn_mode, TensorDescriptor& desc)
        : mode(bn_mode), base_desc(desc){};
    miopenStatus_t GetOutputDesc(TensorDescriptor& output_desc) override;
    miopenStatus_t GetNetworkConfig(std::string& network_config, Handle& handle) override;
    miopenStatus_t GetCompileParms(std::string& compile_config,
                                   Handle& handle,
                                   FusionKernelSourceType source) override;
    miopenStatus_t SetArgs(OperatorArgs& args,
                           const void* alpha,
                           const void* beta,
                           ConstData_t bnScale,
                           ConstData_t bnBias,
                           ConstData_t estimatedMean,
                           ConstData_t estimatedVariance,
                           double epsilon);
    std::vector<std::string> GetArgs() const override;
    miopenFusionOp_t kind() const override { return miopenFusionOpBatchNormInference; };
    FusionMDGraph_Edge_Map MDGraphKey() const override;
    static FusionMDGraph_Edge_Map MDGraphKey(miopenBatchNormMode_t bn_mode);
    std::vector<size_t> GetLocalWGSz(Handle& handle, std::string algorithm_name) override;
    std::vector<size_t> GetGlobalWGSz(Handle& handle, std::string algorithm_name) override;

    miopenBatchNormMode_t mode;
    TensorDescriptor& base_desc;
};

} // namespace miopen
MIOPEN_DEFINE_OBJECT(miopenFusionOpDescriptor, miopen::FusionOpDescriptor);
MIOPEN_DEFINE_OBJECT(miopenOperatorArgs, miopen::OperatorArgs);

#endif // _MIOPEN_FUSION_HPP_