/*******************************************************************************
* Copyright 2020-2025 Intel Corporation
* Copyright 2020-2022 Codeplay Software Limited
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

#include "common/compiler_workarounds.hpp"

#include "gpu/amd/miopen_reorder.hpp"
#include "gpu/amd/stream.hpp"
#include "gpu/amd/sycl_hip_scoped_context.hpp"

#include "xpu/sycl/memory_storage_helper.hpp"

namespace dnnl {
namespace impl {
namespace gpu {
namespace amd {

status_t miopen_reorder_t::execute(const exec_ctx_t &ctx) const {
    memory_desc_wrapper src_mdw(pd()->src_md());
    if (src_mdw.size() == 0) { return status::success; }

    amd::stream_t *hip_stream = utils::downcast<amd::stream_t *>(ctx.stream());
    return hip_stream->interop_task([&](::sycl::handler &cgh) {
        auto arg_src = CTX_IN_SYCL_MEMORY(DNNL_ARG_SRC);
        auto arg_dst = CTX_OUT_SYCL_MEMORY(DNNL_ARG_DST);
        auto arg_src_scale
                = CTX_IN_SYCL_MEMORY(DNNL_ARG_ATTR_SCALES | DNNL_ARG_SRC);
        auto arg_dst_scale
                = CTX_IN_SYCL_MEMORY(DNNL_ARG_ATTR_SCALES | DNNL_ARG_DST);

        compat::host_task(cgh,
                [= WA_THIS_COPY_CAPTURE](const compat::interop_handle &ih) {
                    auto &sycl_engine = *utils::downcast<amd::engine_t *>(
                            hip_stream->engine());
                    auto sc = hip_sycl_scoped_context_handler_t(sycl_engine);
                    auto handle = hip_stream->get_miopen_handle();

                    void *src_ = arg_src.get_native_pointer(ih);
                    void *dst_ = arg_dst.get_native_pointer(ih);

                    auto a = static_cast<uint8_t *>(src_)
                            + pd()->reorder_->src_offset_in_bytes();
                    auto b = static_cast<uint8_t *>(dst_)
                            + pd()->reorder_->dst_offset_in_bytes();

                    void *src_sc = arg_src_scale.get_native_pointer(ih);
                    void *dst_sc = arg_dst_scale.get_native_pointer(ih);

                    pd()->reorder_->execute(handle, a, b, src_sc, dst_sc);
                });
    });
}

} // namespace amd
} // namespace gpu
} // namespace impl
} // namespace dnnl
