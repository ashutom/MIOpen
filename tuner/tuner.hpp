/*******************************************************************************
 *
 * MIT License
 *
 * Copyright (c) 2019 Advanced Micro Devices, Inc.
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
#ifndef GUARD_MIOPEN_TUNER_HPP
#define GUARD_MIOPEN_TUNER_HPP

#include "half.hpp"
//#include "random.hpp"
#include "miopen/bfloat16.hpp"

using half_float::half;
typedef half float16;

#include "InputFlags.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cfloat>
#include <memory>
#include <miopen/miopen.h>
#include <miopen/handle.hpp>
#include <numeric>
#include <vector>

#if MIOPEN_BACKEND_OPENCL

#if defined(__APPLE__) || defined(__MACOSX)
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#elif MIOPEN_BACKEND_HIP
#include <hip/hip_runtime_api.h>

#define printf(...) fprintf(stdout, __VA_ARGS__)

#endif

#define UNPACK_VEC4(v) (v[0]), (v[1]), (v[2]), (v[3])


struct GPUMem
{

#if MIOPEN_BACKEND_OPENCL
    GPUMem(){};
    GPUMem(cl_context& ctx, size_t psz, size_t pdata_sz) : sz(psz), data_sz(pdata_sz)
    {
        buf = clCreateBuffer(ctx, CL_MEM_READ_WRITE, data_sz * sz, nullptr, nullptr);
    }

    int ToGPU(cl_command_queue& q, void* p)
    {
        return clEnqueueWriteBuffer(q, buf, CL_TRUE, 0, data_sz * sz, p, 0, nullptr, nullptr);
    }
    int FromGPU(cl_command_queue& q, void* p)
    {
        return clEnqueueReadBuffer(q, buf, CL_TRUE, 0, data_sz * sz, p, 0, nullptr, nullptr);
    }

    cl_mem GetMem() { return buf; }
    size_t GetSize() { return sz * data_sz; }

    ~GPUMem() { clReleaseMemObject(buf); }

    cl_mem buf;
    size_t sz;
    size_t data_sz;

#elif MIOPEN_BACKEND_HIP

    GPUMem(){};
    GPUMem(uint32_t ctx, size_t psz, size_t pdata_sz) : _ctx(ctx), sz(psz), data_sz(pdata_sz)
    {
        hipMalloc(static_cast<void**>(&buf), data_sz * sz);
    }

    int ToGPU(hipStream_t q, void* p)
    {
        _q = q;
        return static_cast<int>(hipMemcpy(buf, p, data_sz * sz, hipMemcpyHostToDevice));
    }
    int FromGPU(hipStream_t q, void* p)
    {
        hipDeviceSynchronize();
        _q = q;
        return static_cast<int>(hipMemcpy(p, buf, data_sz * sz, hipMemcpyDeviceToHost));
    }

    void* GetMem() { return buf; }
    size_t GetSize() { return sz * data_sz; }

    ~GPUMem() { hipFree(buf); }
    hipStream_t _q; // Place holder for opencl context
    uint32_t _ctx;
    void* buf;
    size_t sz;
    size_t data_sz;
#endif
};

class Driver
{
    public:
    Driver()
    {
        data_type = miopenFloat;
#if MIOPEN_BACKEND_OPENCL
        //handle = miopen::Handle();
#elif MIOPEN_BACKEND_HIP
        hipStream_t s;
        hipStreamCreate(&s);
        handle = miopen::Handle(s);
#endif

        q = handle.GetStream();
    }

    miopen::Handle& GetHandle() { return handle; }
    miopenDataType_t GetDataType() { return data_type; }

#if MIOPEN_BACKEND_OPENCL
    cl_command_queue& GetStream() { return q; }
#elif MIOPEN_BACKEND_HIP
    hipStream_t& GetStream() { return q; }
#endif
    //virtual ~Driver() { miopenDestroy(handle); }

    // TODO: add timing APIs
    virtual int AddCmdLineArgs() = 0;
    virtual int ParseCmdLineArgs(int argc, char* argv[]) = 0;
    virtual InputFlags& GetInputFlags()  = 0;
    virtual int GetandSetData()          = 0;
    virtual int AllocateBuffersAndCopy() = 0;
    virtual int RunForwardGPU()          = 0;
    //virtual int VerifyForward()          = 0;
    virtual int RunBackwardGPU()         = 0;
    //virtual int VerifyBackward()         = 0;

    protected:
    template <typename Tgpu>
    void InitDataType();
    miopen::Handle handle;
    miopenDataType_t data_type;

#if MIOPEN_BACKEND_OPENCL
    cl_command_queue q;
#elif MIOPEN_BACKEND_HIP
    hipStream_t q;
#endif
};

template <>
void Driver::InitDataType<int8_t>()
{
    data_type = miopenInt8;
}
template <>
void Driver::InitDataType<float>()
{
    data_type = miopenFloat;
}
template <>
void Driver::InitDataType<float16>()
{
    data_type = miopenHalf;
}
template <>
void Driver::InitDataType<bfloat16>()
{
    data_type = miopenBFloat16;
}
// "std::is_same<Tgpu, float>{}" used to avoid "static_assert" compilation error,
// which occurs when the condition does not depend in any way on the template parameters.
template <typename Tgpu>
void Driver::InitDataType()
{
    static_assert(std::is_same<Tgpu, float>{}, "unsupported Tgpu");
}

void PadBufferSize(size_t& sz, int datatype_sz)
{
    size_t page_sz = (2 * 1024 * 1024) / datatype_sz;
    if(sz % page_sz != 0)
    {
        sz = ((sz + page_sz) / page_sz) * page_sz;
    }
}

[[gnu::noreturn]] void Usage()
{
    printf("Usage: ./MIOpenTuner *base_arg* *other_args*\n");
    printf(
        "Supported Base Arguments: conv[fp16]\n");
    exit(0);
}

std::string ParseBaseArg(int argc, char* argv[])
{
    if(argc < 2)
    {
        printf("Invalid Number of Input Arguments\n");
        Usage();
    }

    std::string arg = argv[1];

    if(arg != "conv")
    {
        printf("Invalid Base Input Argument\n");
        Usage();
    }
    else if(arg == "-h" || arg == "--help" || arg == "-?")
        Usage();
    else
        return arg;
}

class Tuner : public Driver
{
    public:
    Tuner() : Driver()
    {
        data_type = miopenFloat;
#if MIOPEN_BACKEND_OPENCL
        //handle = miopen::Handle();
#elif MIOPEN_BACKEND_HIP
        hipStream_t s;
        hipStreamCreate(&s);
        handle = miopen::Handle(s);
#endif

        q = handle.GetStream();
    }

    miopen::Handle& GetHandle() { return handle; }
    miopenDataType_t GetDataType() { return data_type; }

#if MIOPEN_BACKEND_OPENCL
    cl_command_queue& GetStream() { return q; }
#elif MIOPEN_BACKEND_HIP
    hipStream_t& GetStream() { return q; }
#endif
    //virtual ~Tuner() { miopenDestroy(handle); }

    // TODO: add timing APIs
    virtual int AddCmdLineArgs() = 0;
    virtual int ParseCmdLineArgs(int argc, char* argv[]) = 0;
    virtual InputFlags& GetInputFlags()  = 0;
    virtual int GetandSetData()          = 0;
    virtual int AllocateBuffersAndCopy() = 0;
    virtual int RunForwardGPU()          = 0;
    virtual int RunBackwardGPU()         = 0;

    protected:
    miopen::Handle handle;
    miopenDataType_t data_type;

#if MIOPEN_BACKEND_OPENCL
    cl_command_queue q;
#elif MIOPEN_BACKEND_HIP
    hipStream_t q;
#endif
};

template <typename T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& vs)
{
    os << "{ size: " << vs.size() << ", entries: ";
    for(auto& v : vs)
        os << v << " ";
    os << "}";
    return os;
}


std::vector<int> GetTensorLengths(miopen::TensorDescriptor& tensor)
{

    std::vector<int> tensor_len;
    tensor_len.resize(tensor.GetSize());
    std::copy(tensor.GetLengths().begin(),
              tensor.GetLengths().end(),
                  tensor_len.data());

    return tensor_len;
}

size_t GetTensorSize(miopen::TensorDescriptor& tensor)
{
    std::vector<int> len = GetTensorLengths(tensor);
    size_t sz            = std::accumulate(len.begin(), len.end(), 1, std::multiplies<int>());

    return sz;
}

#endif // GUARD_MIOPEN_TUNER_HPP

