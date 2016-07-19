/*
 * Copyright 2015 Rockchip Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __MPP_BUFFER_H__
#define __MPP_BUFFER_H__

#include "rk_type.h"
#include "mpp_err.h"

/*
 * because buffer usage maybe unknown when decoder is not started
 * buffer group may need to set a default group size limit
 */
#define SZ_1K           (1024)
#define SZ_2K           (SZ_1K*2)
#define SZ_4K           (SZ_1K*4)
#define SZ_8K           (SZ_1K*8)
#define SZ_16K          (SZ_1K*16)
#define SZ_32K          (SZ_1K*32)
#define SZ_64K          (SZ_1K*64)
#define SZ_128K         (SZ_1K*128)
#define SZ_256K         (SZ_1K*256)
#define SZ_512K         (SZ_1K*512)
#define SZ_1M           (SZ_1K*SZ_1K)
#define SZ_2M           (SZ_1M*2)
#define SZ_4M           (SZ_1M*4)
#define SZ_8M           (SZ_1M*8)
#define SZ_16M          (SZ_1M*16)
#define SZ_32M          (SZ_1M*32)
#define SZ_64M          (SZ_1M*64)
#define SZ_80M          (SZ_1M*80)
#define SZ_128M         (SZ_1M*128)

/*
 * MppBuffer module has several functions:
 *
 * 1. buffer get / put / reference management / external commit / get info.
 *    this part is the basic user interface for MppBuffer.
 *
 *    function:
 *
 *    mpp_buffer_get
 *    mpp_buffer_put
 *    mpp_buffer_inc_ref
 *    mpp_buffer_commit
 *    mpp_buffer_info_get
 *
 * 2. user buffer working flow control abstraction.
 *    buffer should attach to certain group, and buffer mode control the buffer usage flow.
 *    this part is also a part of user interface.
 *
 *    function:
 *
 *    mpp_buffer_group_get
 *    mpp_buffer_group_normal_get
 *    mpp_buffer_group_limit_get
 *    mpp_buffer_group_put
 *    mpp_buffer_group_limit_config
 *
 * 3. buffer allocator management
 *    this part is for allocator on different os, it does not have user interface
 *    it will support normal buffer, Android ion buffer, Linux v4l2 vb2 buffer
 *    user can only use MppBufferType to choose.
 *
 */
typedef void* MppBuffer;
typedef void* MppBufferGroup;

/*
 * mpp buffer group support two work flow mode:
 *
 * normal flow: all buffer are generated by MPP
 *              under this mode, buffer pool is maintained internally
 *
 *              typical call flow:
 *
 *              mpp_buffer_group_get()          return A
 *              mpp_buffer_get(A)               return a    ref +1 -> used
 *              mpp_buffer_inc_ref(a)                       ref +1
 *              mpp_buffer_put(a)                           ref -1
 *              mpp_buffer_put(a)                           ref -1 -> unused
 *              mpp_buffer_group_put(A)
 *
 * commit flow: all buffer are commited out of MPP
 *              under this mode, buffers is commit by external api.
 *              normally MPP only use it but not generate it.
 *
 *              typical call flow:
 *
 *              ==== external allocator ====
 *              mpp_buffer_group_get()          return A
 *              mpp_buffer_commit(A, x)
 *              mpp_buffer_commit(A, y)
 *
 *              ======= internal user ======
 *              mpp_buffer_get(A)               return a
 *              mpp_buffer_get(A)               return b
 *              mpp_buffer_put(a)
 *              mpp_buffer_put(b)
 *
 *              ==== external allocator ====
 *              mpp_buffer_group_put(A)
 *
 *              NOTE: commit interface required group handle to record group information
 */

/*
 * mpp buffer group has two buffer limit mode: normal and limit
 *
 * normal mode: allows any buffer size and always general new buffer is no unused buffer
 *              is available.
 *              This mode normally use with normal flow and is used for table / stream buffer
 *
 * limit mode : restrict the buffer's size and count in the buffer group. if try to calloc
 *              buffer with different size or extra count it will fail.
 *              This mode normally use with commit flow and is used for frame buffer
 */

/*
 * NOTE: normal mode is recommanded to work with normal flow, working with limit  mode is not.
 *       limit  mode is recommanded to work with commit flow, working with normal mode is not.
 */
typedef enum {
    MPP_BUFFER_INTERNAL,
    MPP_BUFFER_EXTERNAL,
    MPP_BUFFER_MODE_BUTT,
} MppBufferMode;

/*
 * mpp buffer has two types:
 *
 * normal   : normal malloc buffer for unit test or hardware simulation
 * ion      : use ion device under Android/Linux, MppBuffer will encapsulte ion file handle
 */
typedef enum {
    MPP_BUFFER_TYPE_NORMAL,
    MPP_BUFFER_TYPE_ION,
    MPP_BUFFER_TYPE_V4L2,
    MPP_BUFFER_TYPE_DRM,
    MPP_BUFFER_TYPE_BUTT,
} MppBufferType;

/*
 * MppBufferInfo variable's meaning is different in different MppBufferType
 *
 * MPP_BUFFER_TYPE_NORMAL
 *
 * ptr  - virtual address of normal malloced buffer
 * fd   - unused and set to -1
 *
 * MPP_BUFFER_TYPE_ION
 *
 * ptr  - virtual address of ion buffer in user space
 * hnd  - ion handle in user space
 * fd   - ion buffer file handle for map / unmap
 *
 * MPP_BUFFER_TYPE_V4L2
 *
 * TODO: to be implemented.
 */
typedef struct MppBufferInfo_t {
    MppBufferType   type;
    size_t          size;
    void            *ptr;
    void            *hnd;
    int             fd;
} MppBufferInfo;

#define BUFFER_GROUP_SIZE_DEFAULT           (SZ_1M*80)

/*
 * mpp_buffer_import_with_tag(MppBufferGroup group, MppBufferInfo *info, MppBuffer *buffer)
 *
 * 1. group - specified the MppBuffer to be attached to.
 *    group can be NULL then this buffer will attached to default legecy group
 *    Default to NULL on mpp_buffer_import case
 *
 * 2. info  - input information for the output MppBuffer
 *    info can NOT be NULL. It must contain at least one of ptr/fd.
 *
 * 3. buffer - generated MppBuffer from MppBufferInfo.
 *    buffer can be NULL then the buffer is commit to group with unused status.
 *    Otherwise generated buffer will be directly got and ref_count increased.
 *    Default to NULL on mpp_buffer_commit case
 *
 * mpp_buffer_commit usage:
 *
 * Add a external buffer info to group. This buffer will be on unused status.
 * Typical usage is on Android. MediaPlayer gralloc Graphic buffer then commit these buffer
 * to decoder's buffer group. Then decoder will recycle these buffer and return buffer reference
 * to MediaPlayer for display.
 *
 * mpp_buffer_import usage:
 *
 * Transfer a external buffer info to MppBuffer but it is not expected to attached to certain
 * buffer group. So the group is set to NULL. Then this buffer can be used for MppFrame/MppPacket.
 * Typical usage is for image processing. Image processing normally will be a oneshot operation
 * It does not need complicated group management. But in other hand mpp still need to know the
 * imported buffer is leak or not and trace its usage inside mpp process. So we attach this kind
 * of buffer to default misc buffer group for management.
 */
#define mpp_buffer_commit(group, info, ...) \
        mpp_buffer_import_with_tag(group, info, NULL, MODULE_TAG, __FUNCTION__)

#define mpp_buffer_import(buffer, info, ...) \
        mpp_buffer_import_with_tag(NULL, info, buffer, MODULE_TAG, __FUNCTION__)

#define mpp_buffer_get(group, buffer, size, ...) \
        mpp_buffer_get_with_tag(group, buffer, size, MODULE_TAG, __FUNCTION__)

#define mpp_buffer_put(buffer) \
        mpp_buffer_put_with_caller(buffer, __FUNCTION__)

#define mpp_buffer_inc_ref(buffer) \
        mpp_buffer_inc_ref_with_caller(buffer, __FUNCTION__);

#define mpp_buffer_group_get_internal(group, type, ...) \
        mpp_buffer_group_get(group, type, MPP_BUFFER_INTERNAL, MODULE_TAG, __FUNCTION__)

#define mpp_buffer_group_get_external(group, type, ...) \
        mpp_buffer_group_get(group, type, MPP_BUFFER_EXTERNAL, MODULE_TAG, __FUNCTION__)

#ifdef __cplusplus
extern "C" {
#endif

/*
 * MppBuffer interface
 * these interface will change value of group and buffer so before calling functions
 * parameter need to be checked.
 *
 * IMPORTANT:
 * mpp_buffer_import_with_tag - compounded interface for commit and import
 *
 */
MPP_RET mpp_buffer_import_with_tag(MppBufferGroup group, MppBufferInfo *info, MppBuffer *buffer,
                                   const char *tag, const char *caller);
MPP_RET mpp_buffer_get_with_tag(MppBufferGroup group, MppBuffer *buffer, size_t size,
                                const char *tag, const char *caller);
MPP_RET mpp_buffer_put_with_caller(MppBuffer buffer, const char *caller);
MPP_RET mpp_buffer_inc_ref_with_caller(MppBuffer buffer, const char *caller);

MPP_RET mpp_buffer_info_get(MppBuffer buffer, MppBufferInfo *info);
MPP_RET mpp_buffer_read(MppBuffer buffer, size_t offset, void *data, size_t size);
MPP_RET mpp_buffer_write(MppBuffer buffer, size_t offset, void *data, size_t size);
void   *mpp_buffer_get_ptr(MppBuffer buffer);
int     mpp_buffer_get_fd(MppBuffer buffer);
size_t  mpp_buffer_get_size(MppBuffer buffer);

MPP_RET mpp_buffer_group_get(MppBufferGroup *group, MppBufferType type, MppBufferMode mode,
                             const char *tag, const char *caller);
MPP_RET mpp_buffer_group_put(MppBufferGroup group);
MPP_RET mpp_buffer_group_clear(MppBufferGroup group);
RK_S32  mpp_buffer_group_unused(MppBufferGroup group);
MppBufferMode mpp_buffer_group_mode(MppBufferGroup group);
MppBufferType mpp_buffer_group_type(MppBufferGroup group);

/*
 * size  : 0 - no limit, other - max buffer size
 * count : 0 - no limit, other - max buffer count
 */
MPP_RET mpp_buffer_group_limit_config(MppBufferGroup group, size_t size, RK_S32 count);

#ifdef __cplusplus
}
#endif

#endif /*__MPP_BUFFER_H__*/
