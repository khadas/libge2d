/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include "kernel-headers/linux/ge2d.h"
#include "include/ge2d_port.h"
#include "include/ge2d_com.h"
#include "include/dmabuf.h"

#define   FILE_NAME_GE2D        "/dev/ge2d"

#define GE2D_BPP_32  32
#define GE2D_BPP_24  24
#define GE2D_BPP_16  16
#define GE2D_BPP_12  12
#define GE2D_BPP_8   8

int has_canvas = 0;

int CANVAS_ALIGNED(int x)
{
    if (has_canvas)
        return (x + 31) & ~31;
    else
        return (x + 7) & ~7;
}

int YV12_Y_ALIGNED(int x)
{
    if (has_canvas)
        return (x + 63) & ~63;
    else
        return (x + 15) & ~15;
}

static int  pixel_to_ge2d_format(int *img_format, int *pge2d_format,int *p_bpp)
{
    int is_one_plane = -1;
    unsigned int mask = ~(MATRIX_CUSTOM | STRIDE_CUSTOM | FORMAT_FULL_RANGE | EXT_FUN_MASK);

    switch (*img_format & mask) {
        case PIXEL_FORMAT_RGBA_8888:
        case PIXEL_FORMAT_RGBX_8888:
        *pge2d_format = GE2D_FORMAT_S32_ABGR;
        *p_bpp = GE2D_BPP_32;
        is_one_plane = 1;
        break;
        case PIXEL_FORMAT_ARGB_8888:
        *pge2d_format = GE2D_FORMAT_S32_BGRA;
        *p_bpp = GE2D_BPP_32;
        is_one_plane = 1;
        break;
        case PIXEL_FORMAT_ABGR_8888:
        *pge2d_format = GE2D_FORMAT_S32_RGBA;
        *p_bpp = GE2D_BPP_32;
        is_one_plane = 1;
        break;
        case PIXEL_FORMAT_BGRA_8888:
        *pge2d_format = GE2D_FORMAT_S32_ARGB;
        *p_bpp = GE2D_BPP_32;
        is_one_plane = 1;
        break;
        case PIXEL_FORMAT_RGB_888:
        *pge2d_format = GE2D_FORMAT_S24_BGR;
        *p_bpp = GE2D_BPP_24;
        is_one_plane = 1;
        break;
        case PIXEL_FORMAT_RGB_565:
        *pge2d_format = GE2D_FORMAT_S16_RGB_565;
        *p_bpp = GE2D_BPP_16;
        is_one_plane = 1;
        break;
        case PIXEL_FORMAT_ARGB_4444:
        *pge2d_format = GE2D_FORMAT_S16_ARGB_4444;
        *p_bpp = GE2D_BPP_16;
        is_one_plane = 1;
        break;
        case PIXEL_FORMAT_RGBA_4444:
        *pge2d_format = GE2D_FORMAT_S16_RGBA_4444;
        *p_bpp = GE2D_BPP_16;
        is_one_plane = 1;
        break;
        case PIXEL_FORMAT_ARGB_1555:
        *pge2d_format = GE2D_FORMAT_S16_ARGB_1555;
        *p_bpp = GE2D_BPP_16;
        is_one_plane = 1;
        break;
        case PIXEL_FORMAT_YCbCr_422_UYVY:
        *pge2d_format = GE2D_FORMAT_S16_YUV422;
        *p_bpp = GE2D_BPP_16;
        is_one_plane = 1;
        break;
        case PIXEL_FORMAT_YCrCb_420_SP:
        *pge2d_format = GE2D_FORMAT_M24_NV21;
        *p_bpp = GE2D_BPP_8;
        is_one_plane = 0;
        break;
        case PIXEL_FORMAT_YCbCr_420_SP_NV12:
        *pge2d_format = GE2D_FORMAT_M24_NV12;
        *p_bpp = GE2D_BPP_8;
        is_one_plane = 0;
        break;
        case PIXEL_FORMAT_YV12:
        case PIXEL_FORMAT_YU12:
        *pge2d_format = GE2D_FORMAT_M24_YUV420;
        *p_bpp = GE2D_BPP_8;
        is_one_plane = 0;
        break;
        case  PIXEL_FORMAT_Y8:
        *pge2d_format = GE2D_FORMAT_S8_Y;
        *p_bpp = GE2D_BPP_8;
        is_one_plane = 1;
        break;
        case PIXEL_FORMAT_BGR_888:
        *pge2d_format = GE2D_FORMAT_S24_RGB;
        *p_bpp = GE2D_BPP_24;
        is_one_plane = 1;
        break;
        case PIXEL_FORMAT_YCbCr_422_SP:
        *pge2d_format = GE2D_FORMAT_M24_YUV422SP;
        *p_bpp = GE2D_BPP_8;
        is_one_plane = 0;
        break;
        case PIXEL_FORMAT_CLUT8:
        *pge2d_format = GE2D_FORMAT_S8_LUT;
        *p_bpp = GE2D_BPP_8;
        is_one_plane = 1;
        break;
        case  PIXEL_FORMAT_ALPHA8:
        *pge2d_format = GE2D_FORMAT_S8_A;
        *p_bpp = GE2D_BPP_8;
        is_one_plane = 1;
        break;
        default:
        E_GE2D("Image format %d not supported!", *img_format);
        *pge2d_format = 0xffffffff;
        *p_bpp = GE2D_BPP_32;
        break;
    }

    if (*img_format & MATRIX_CUSTOM)
        *pge2d_format |= GE2D_MATRIX_CUSTOM;
    if (*img_format & STRIDE_CUSTOM)
        *pge2d_format |= GE2D_STRIDE_CUSTOM;
    if (*img_format & FORMAT_FULL_RANGE)
        *pge2d_format |= GE2D_FORMAT_FULL_RANGE;
    if ((*img_format & EXT_FUN_MASK) == DST_REPEAT_2)
        *pge2d_format |= GE2D_DST_REPEAT_2;
    if ((*img_format & EXT_FUN_MASK) == DST_REPEAT_4)
        *pge2d_format |= GE2D_DST_REPEAT_4;
    if ((*img_format & EXT_FUN_MASK) == DST_REPEAT_8)
        *pge2d_format |= GE2D_DST_REPEAT_8;
    if ((*img_format & EXT_FUN_MASK) == DST_SIGN_MDOE)
        *pge2d_format |= GE2D_DST_SIGN_MDOE;

    *img_format &= mask;

    return is_one_plane;
}

static void ge2d_set_canvas(int bpp, int w,int h, int *canvas_w, int *canvas_h)
{
   *canvas_w = (CANVAS_ALIGNED(w * bpp >> 3))/(bpp >> 3);
   *canvas_h = h;
}

static inline unsigned blendop(unsigned color_blending_mode,
        unsigned color_blending_src_factor,
        unsigned color_blending_dst_factor,
        unsigned alpha_blending_mode,
        unsigned alpha_blending_src_factor,
        unsigned alpha_blending_dst_factor)
{
    return (color_blending_mode << 24) |
        (color_blending_src_factor << 20) |
        (color_blending_dst_factor << 16) |
        (alpha_blending_mode << 8) |
        (alpha_blending_src_factor << 4) | (alpha_blending_dst_factor << 0);
}

static int is_no_alpha(int format)
{
    if ((format == PIXEL_FORMAT_RGBX_8888) ||
        (format == PIXEL_FORMAT_RGB_565) ||
        (format == PIXEL_FORMAT_ARGB_4444) ||
        (format == PIXEL_FORMAT_RGBA_4444) ||
        (format == PIXEL_FORMAT_ARGB_1555) ||
        (format == PIXEL_FORMAT_RGB_888) ||
        (format == PIXEL_FORMAT_YCrCb_420_SP)||
        (format == PIXEL_FORMAT_YCbCr_420_SP_NV12) ||
        (format == PIXEL_FORMAT_Y8) ||
        (format == PIXEL_FORMAT_YV12) ||
        (format == PIXEL_FORMAT_YU12) ||
        (format == PIXEL_FORMAT_YCbCr_422_SP) ||
        (format == PIXEL_FORMAT_YCbCr_422_UYVY) ||
        (format == PIXEL_FORMAT_BGR_888))
        return 1;
    else
        return 0;
}

/* for example, append alpha for nv12:
 * nv12 + alpha -> rgba
 */
static int is_not_append_alpha(aml_ge2d_info_t *pge2dinfo)
{
    if (pge2dinfo->src_info[0].format == PIXEL_FORMAT_ALPHA8 ||
        pge2dinfo->src_info[1].format == PIXEL_FORMAT_ALPHA8)
        return 0;
    else
        return 1;
}

static int is_append_alpha(aml_ge2d_info_t *pge2dinfo)
{
    return !is_not_append_alpha(pge2dinfo);
}

static int is_need_swap_src2(int format,buffer_info_t *src2, buffer_info_t *dst,
                             int cap_attr)
{
    int ret = 0;
    unsigned int mask = ~(MATRIX_CUSTOM | STRIDE_CUSTOM | FORMAT_FULL_RANGE | EXT_FUN_MASK);

    format &= mask;
    /* src2 not support nv21/nv12/yv12, swap src1 and src2 */
    if ((format == PIXEL_FORMAT_YCrCb_420_SP) ||
        (format == PIXEL_FORMAT_YCbCr_420_SP_NV12) ||
        (format == PIXEL_FORMAT_YV12) ||
        (format == PIXEL_FORMAT_YU12))
        ret = 1;
    else
        ret = 0;

    if (cap_attr & SRC2_REPEAT)
        return ret;

    /* src2 scaler, swap src1 and src2 */
    if (!ret) {
        if ((src2->rect.w < dst->rect.w) ||
            (src2->rect.h < dst->rect.h))
            ret = 1;
        else
            ret = 0;
    }
    return ret;
}

static int is_rect_valid(buffer_info_t *pbuffer_info)
{
    int ret = 1;
    if (CANVAS_ALLOC == pbuffer_info->memtype)
        if (((unsigned int)pbuffer_info->rect.w > pbuffer_info->canvas_w) ||
            ((unsigned int)pbuffer_info->rect.h > pbuffer_info->canvas_h)) {
            E_GE2D("rect.w,h:[%d,%d] out of range!\n",pbuffer_info->rect.w,
                pbuffer_info->rect.h);
            ret = 0;
        }
    return ret;
}

/* dst plane number > 1 means need do more time ge2d transform */
static int get_dst_op_number(aml_ge2d_info_t *pge2dinfo)
{
    int dst_format;
    int op_number;
    unsigned int mask = ~(MATRIX_CUSTOM | STRIDE_CUSTOM | FORMAT_FULL_RANGE | EXT_FUN_MASK);

    dst_format = pge2dinfo->dst_info.format & mask;
    switch (dst_format) {
    case PIXEL_FORMAT_RGBA_8888:
    case PIXEL_FORMAT_RGBX_8888:
    case PIXEL_FORMAT_ARGB_8888:
    case PIXEL_FORMAT_ABGR_8888:
    case PIXEL_FORMAT_RGB_888:
    case PIXEL_FORMAT_RGB_565:
    case PIXEL_FORMAT_ARGB_4444:
    case PIXEL_FORMAT_RGBA_4444:
    case PIXEL_FORMAT_ARGB_1555:
    case PIXEL_FORMAT_BGRA_8888:
    case PIXEL_FORMAT_Y8:
    case PIXEL_FORMAT_CLUT8:
    case PIXEL_FORMAT_YCrCb_420_SP:
    case PIXEL_FORMAT_YCbCr_422_SP:
    case PIXEL_FORMAT_YCbCr_420_SP_NV12:
    case PIXEL_FORMAT_YCbCr_422_UYVY:
    case PIXEL_FORMAT_BGR_888:
    case PIXEL_FORMAT_ALPHA8:
        op_number = 1;
        break;
    case PIXEL_FORMAT_YV12:
    case PIXEL_FORMAT_YU12:
        op_number = 3;
        break;
    default:
        op_number = 1;
    }
    return op_number;
}

static int ge2d_fillrectangle_config_ex_ion(int fd,aml_ge2d_info_t *pge2dinfo)
{
    int ret = -1;
    struct config_para_ex_ion_s ge2d_config_ex;
    int src_format = 0xffffffff,dst_format = 0xffffffff;
    int s_canvas_w = 0;
    int s_canvas_h = 0;
    int d_canvas_w = 0;
    int d_canvas_h = 0;
    int bpp = 0;
    int is_one_plane = -1;
    buffer_info_t* input_buffer_info = &(pge2dinfo->src_info[0]);
    buffer_info_t* output_buffer_info = &(pge2dinfo->dst_info);
    unsigned int src_endian = input_buffer_info->endain ? GE2D_BIG_ENDIAN : GE2D_LITTLE_ENDIAN;
    unsigned int dst_endian = output_buffer_info->endain ? GE2D_BIG_ENDIAN : GE2D_LITTLE_ENDIAN;

    if (output_buffer_info->plane_number < 1 ||
        output_buffer_info->plane_number > GE2D_MAX_PLANE)
        output_buffer_info->plane_number = 1;
    memset(&ge2d_config_ex, 0, sizeof(struct config_para_ex_ion_s ));

    if ((CANVAS_ALLOC == output_buffer_info->memtype)) {
        is_one_plane = pixel_to_ge2d_format(&output_buffer_info->format,&dst_format,&bpp);
        dst_format |= dst_endian;
        if ((int)0xffffffff == dst_format) {
            E_GE2D("can't get proper ge2d format\n" );
            return ge2d_fail;
        }
        ge2d_set_canvas(bpp,output_buffer_info->canvas_w,output_buffer_info->canvas_h,&d_canvas_w,&d_canvas_h);

        pixel_to_ge2d_format(&input_buffer_info->format,&src_format,&bpp);
        src_format |= src_endian;
        ge2d_set_canvas(bpp,input_buffer_info->canvas_w,input_buffer_info->canvas_h,&s_canvas_w,&s_canvas_h);
    }else {
        is_one_plane = pixel_to_ge2d_format(&output_buffer_info->format,&dst_format,&bpp);
        dst_format |= dst_endian;
        ge2d_set_canvas(bpp,output_buffer_info->canvas_w,output_buffer_info->canvas_h,&d_canvas_w,&d_canvas_h);

        pixel_to_ge2d_format(&input_buffer_info->format,&src_format,&bpp);
        src_format |= src_endian;
        ge2d_set_canvas(bpp,input_buffer_info->canvas_w,input_buffer_info->canvas_h,&s_canvas_w,&s_canvas_h);
    }
    D_GE2D("ge2d_fillrectangle_config_ex,memtype=%x,src_format=%x,s_canvas_w=%d,s_canvas_h=%d,rotation=%d\n",
        input_buffer_info->memtype,src_format,s_canvas_w,s_canvas_h,input_buffer_info->rotation);

    D_GE2D("ge2d_fillrectangle_config_ex,memtype=%x,dst_format=%x,d_canvas_w=%d,d_canvas_h=%d,rotation=%d\n",
        output_buffer_info->memtype,dst_format,d_canvas_w,d_canvas_h,output_buffer_info->rotation);

    ge2d_config_ex.src_para.mem_type = CANVAS_TYPE_INVALID;
    ge2d_config_ex.src_para.format = src_format;
    ge2d_config_ex.src_para.left = 0;
    ge2d_config_ex.src_para.top = 0;
    ge2d_config_ex.src_para.width = s_canvas_w;
    ge2d_config_ex.src_para.height = s_canvas_h;

    ge2d_config_ex.dst_para.mem_type = output_buffer_info->memtype;
    ge2d_config_ex.dst_para.format = dst_format;
    ge2d_config_ex.dst_para.left = 0;
    ge2d_config_ex.dst_para.top = 0;
    ge2d_config_ex.dst_para.width = d_canvas_w;
    ge2d_config_ex.dst_para.height = d_canvas_h;

    ge2d_config_ex.src2_para.mem_type = CANVAS_TYPE_INVALID;

    switch (pge2dinfo->dst_info.rotation) {
        case GE2D_ROTATION_0:
            break;
        case GE2D_ROTATION_90:
            ge2d_config_ex.dst_xy_swap = 1;
            ge2d_config_ex.dst_para.x_rev = 1;
            break;
        case GE2D_ROTATION_180:
            ge2d_config_ex.dst_para.x_rev = 1;
            ge2d_config_ex.dst_para.y_rev = 1;
            break;
        case GE2D_ROTATION_270:
            ge2d_config_ex.dst_xy_swap = 1;
            ge2d_config_ex.dst_para.y_rev = 1;
            break;
        default:
            break;
    }

    if (CANVAS_ALLOC == output_buffer_info->memtype) {
        if (is_one_plane == 1) {
            ge2d_config_ex.dst_planes[0].addr = output_buffer_info->offset[0];
            ge2d_config_ex.dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
            ge2d_config_ex.dst_planes[0].w = d_canvas_w;
            ge2d_config_ex.dst_planes[0].h = d_canvas_h;
        } else if ((output_buffer_info->format == PIXEL_FORMAT_YCrCb_420_SP) ||
            (output_buffer_info->format == PIXEL_FORMAT_YCbCr_420_SP_NV12)) {
            if (output_buffer_info->plane_number == 1) {
                ge2d_config_ex.dst_planes[0].addr = output_buffer_info->offset[0];
                ge2d_config_ex.dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                ge2d_config_ex.dst_planes[0].w = d_canvas_w;
                ge2d_config_ex.dst_planes[0].h = d_canvas_h;
                ge2d_config_ex.dst_planes[1].addr = (d_canvas_w * d_canvas_h);
                ge2d_config_ex.dst_planes[1].shared_fd = 0;
                ge2d_config_ex.dst_planes[1].w = d_canvas_w;
                ge2d_config_ex.dst_planes[1].h = d_canvas_h/2;
            } else if (output_buffer_info->plane_number == 2) {
                ge2d_config_ex.dst_planes[0].addr = output_buffer_info->offset[0];
                ge2d_config_ex.dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                ge2d_config_ex.dst_planes[0].w = d_canvas_w;
                ge2d_config_ex.dst_planes[0].h = d_canvas_h;
                ge2d_config_ex.dst_planes[1].addr = output_buffer_info->offset[1];
                ge2d_config_ex.dst_planes[1].shared_fd = output_buffer_info->shared_fd[1];
                ge2d_config_ex.dst_planes[1].w = d_canvas_w;
                ge2d_config_ex.dst_planes[1].h = d_canvas_h/2;
            }
        }  else if (output_buffer_info->format == PIXEL_FORMAT_Y8 ||
                    output_buffer_info->format == PIXEL_FORMAT_CLUT8 ||
                    output_buffer_info->format == PIXEL_FORMAT_ALPHA8) {
            ge2d_config_ex.dst_planes[0].addr = output_buffer_info->offset[0];
            ge2d_config_ex.dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
            ge2d_config_ex.dst_planes[0].w = d_canvas_w;
            ge2d_config_ex.dst_planes[0].h = d_canvas_h;
        } else if (output_buffer_info->format == PIXEL_FORMAT_YV12 ||
                   output_buffer_info->format == PIXEL_FORMAT_YU12) {
            switch (pge2dinfo->dst_op_cnt) {
            case 0:
                ge2d_config_ex.dst_para.format = GE2D_FORMAT_S8_Y | dst_endian;
                ge2d_config_ex.dst_planes[0].addr = output_buffer_info->offset[0];
                ge2d_config_ex.dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                ge2d_config_ex.dst_planes[0].w = d_canvas_w;
                ge2d_config_ex.dst_planes[0].h = d_canvas_h;
                ge2d_config_ex.dst_para.width = d_canvas_w;
                ge2d_config_ex.dst_para.height = d_canvas_h;
                break;
            case 1:
                if (output_buffer_info->format == PIXEL_FORMAT_YV12)
                    ge2d_config_ex.dst_para.format = GE2D_FORMAT_S8_CR | dst_endian;
                else
                    ge2d_config_ex.dst_para.format = GE2D_FORMAT_S8_CB | dst_endian;
                ge2d_config_ex.dst_planes[0].w = d_canvas_w/2;
                ge2d_config_ex.dst_planes[0].h = d_canvas_h/2;
                ge2d_config_ex.dst_para.width = d_canvas_w/2;
                ge2d_config_ex.dst_para.height = d_canvas_h/2;
                if (output_buffer_info->plane_number == 1) {
                    ge2d_config_ex.dst_planes[0].addr = YV12_Y_ALIGNED(d_canvas_w) *
                        d_canvas_h;
                    ge2d_config_ex.dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                } else if (output_buffer_info->plane_number == 3) {
                    ge2d_config_ex.dst_planes[0].addr = output_buffer_info->offset[1];
                    ge2d_config_ex.dst_planes[0].shared_fd = output_buffer_info->shared_fd[1];
                }
                break;
            case 2:
                if (output_buffer_info->format == PIXEL_FORMAT_YV12)
                    ge2d_config_ex.dst_para.format = GE2D_FORMAT_S8_CB | dst_endian;
                else
                    ge2d_config_ex.dst_para.format = GE2D_FORMAT_S8_CR | dst_endian;
                ge2d_config_ex.dst_planes[0].w = d_canvas_w/2;
                ge2d_config_ex.dst_planes[0].h = d_canvas_h/2;
                ge2d_config_ex.dst_para.width = d_canvas_w/2;
                ge2d_config_ex.dst_para.height = d_canvas_h/2;
                if (output_buffer_info->plane_number == 1) {
                    ge2d_config_ex.dst_planes[0].addr = YV12_Y_ALIGNED(d_canvas_w) *
                        d_canvas_h + CANVAS_ALIGNED(d_canvas_w/2) * d_canvas_h/2;
                    ge2d_config_ex.dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                } else if (output_buffer_info->plane_number == 3) {
                    ge2d_config_ex.dst_planes[0].addr = output_buffer_info->offset[2];
                    ge2d_config_ex.dst_planes[0].shared_fd = output_buffer_info->shared_fd[2];
                }
                break;
            default:
                break;
            }
        } else if (output_buffer_info->format == PIXEL_FORMAT_YCbCr_422_SP) {
            if (output_buffer_info->plane_number == 1) {
                ge2d_config_ex.dst_planes[0].addr = output_buffer_info->offset[0];
                ge2d_config_ex.dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                ge2d_config_ex.dst_planes[0].w = d_canvas_w;
                ge2d_config_ex.dst_planes[0].h = d_canvas_h;
                ge2d_config_ex.dst_planes[1].addr = (d_canvas_w * d_canvas_h);
                ge2d_config_ex.dst_planes[1].shared_fd = 0;
                ge2d_config_ex.dst_planes[1].w = d_canvas_w;
                ge2d_config_ex.dst_planes[1].h = d_canvas_h;
            } else if (output_buffer_info->plane_number == 2) {
                ge2d_config_ex.dst_planes[0].addr = output_buffer_info->offset[0];
                ge2d_config_ex.dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                ge2d_config_ex.dst_planes[0].w = d_canvas_w;
                ge2d_config_ex.dst_planes[0].h = d_canvas_h;
                ge2d_config_ex.dst_planes[1].addr = output_buffer_info->offset[1];
                ge2d_config_ex.dst_planes[1].shared_fd = output_buffer_info->shared_fd[1];
                ge2d_config_ex.dst_planes[1].w = d_canvas_w;
                ge2d_config_ex.dst_planes[1].h = d_canvas_h;

            }
        } else {
            E_GE2D("format is not match, should config dst_planes correct.\n");
            return ge2d_fail;
        }
    }
    ge2d_config_ex.alu_const_color = pge2dinfo->const_color;
    ge2d_config_ex.src1_gb_alpha = input_buffer_info->plane_alpha & 0xff;

    if (pge2dinfo->cap_attr == -1)
        ret = ioctl(fd, GE2D_CONFIG_EX_ION_EN, &ge2d_config_ex);
    else
        ret = ioctl(fd, GE2D_CONFIG_EX_ION, &ge2d_config_ex);
    if (ret < 0) {
        E_GE2D("ge2d config ex ion failed. \n");
        return ge2d_fail;
    }
    return ge2d_success;
}

static int ge2d_blit_config_ex_ion(int fd,aml_ge2d_info_t *pge2dinfo)
{
    int ret = -1;
    struct config_para_ex_ion_s ge2d_config_ex;
    int src_format = 0xffffffff,dst_format = 0xffffffff;
    int s_canvas_w = 0;
    int s_canvas_h = 0;
    int d_canvas_w = 0;
    int d_canvas_h = 0;
    int bpp = 0;
    int is_one_plane_input = -1;
    int is_one_plane_output = -1;
    buffer_info_t* input_buffer_info = &(pge2dinfo->src_info[0]);
    buffer_info_t* output_buffer_info = &(pge2dinfo->dst_info);
    unsigned int src_endian = input_buffer_info->endain ? GE2D_BIG_ENDIAN : GE2D_LITTLE_ENDIAN;
    unsigned int dst_endian = output_buffer_info->endain ? GE2D_BIG_ENDIAN : GE2D_LITTLE_ENDIAN;

    if (input_buffer_info->plane_number < 1 ||
        input_buffer_info->plane_number > GE2D_MAX_PLANE)
        input_buffer_info->plane_number = 1;
    if (output_buffer_info->plane_number < 1 ||
        output_buffer_info->plane_number > GE2D_MAX_PLANE)
        output_buffer_info->plane_number = 1;
    memset(&ge2d_config_ex, 0, sizeof(struct config_para_ex_ion_s ));

    if ((CANVAS_ALLOC == input_buffer_info->memtype)) {
        is_one_plane_input = pixel_to_ge2d_format(&input_buffer_info->format,&src_format,&bpp);
        src_format |= src_endian;
        if ((int)0xffffffff == src_format) {
            E_GE2D("can't get proper ge2d format\n" );
            return ge2d_fail;
        }
        ge2d_set_canvas(bpp,input_buffer_info->canvas_w,input_buffer_info->canvas_h,&s_canvas_w,&s_canvas_h);
    }

    if ((CANVAS_ALLOC == output_buffer_info->memtype)) {
        is_one_plane_output = pixel_to_ge2d_format(&output_buffer_info->format,&dst_format,&bpp);
        dst_format |= dst_endian;
        if ((int)0xffffffff == dst_format) {
            E_GE2D("can't get proper ge2d format\n" );
            return ge2d_fail;
        }
        ge2d_set_canvas(bpp,output_buffer_info->canvas_w,output_buffer_info->canvas_h,&d_canvas_w,&d_canvas_h);
    }
    D_GE2D("ge2d_blit_config_ex,memtype=%x,src_format=%x,s_canvas_w=%d,s_canvas_h=%d,rotation=%d\n",
        input_buffer_info->memtype,src_format,s_canvas_w,s_canvas_h,input_buffer_info->rotation);

    D_GE2D("ge2d_blit_config_ex,memtype=%x,dst_format=%x,d_canvas_w=%d,d_canvas_h=%d,rotation=%d\n",
        output_buffer_info->memtype,dst_format,d_canvas_w,d_canvas_h,output_buffer_info->rotation);

    ge2d_config_ex.src_para.mem_type = input_buffer_info->memtype;
    ge2d_config_ex.src_para.format = src_format;
    ge2d_config_ex.src_para.left = input_buffer_info->rect.x;
    ge2d_config_ex.src_para.top = input_buffer_info->rect.y;
    ge2d_config_ex.src_para.width = input_buffer_info->rect.w;
    ge2d_config_ex.src_para.height = input_buffer_info->rect.h;

    ge2d_config_ex.src2_para.mem_type = CANVAS_TYPE_INVALID;

    ge2d_config_ex.dst_para.mem_type = output_buffer_info->memtype;
    ge2d_config_ex.dst_para.format = dst_format;
    ge2d_config_ex.dst_para.left = 0;
    ge2d_config_ex.dst_para.top = 0;
    ge2d_config_ex.dst_para.width = d_canvas_w;
    ge2d_config_ex.dst_para.height = d_canvas_h;
    if (input_buffer_info->layer_mode == LAYER_MODE_PREMULTIPLIED)
        ge2d_config_ex.src1_cmult_asel = 2;
    else if (input_buffer_info->layer_mode == LAYER_MODE_COVERAGE)
        ge2d_config_ex.src1_cmult_asel = 0;
    else if (input_buffer_info->layer_mode == LAYER_MODE_NON)
        ge2d_config_ex.src1_cmult_asel = 0;

    switch (pge2dinfo->src_info[0].rotation) {
        case GE2D_ROTATION_0:
            break;
        case GE2D_ROTATION_90:
            ge2d_config_ex.dst_xy_swap = 1;
            ge2d_config_ex.src_para.y_rev = 1;
            break;
        case GE2D_ROTATION_180:
            ge2d_config_ex.src_para.x_rev = 1;
            ge2d_config_ex.src_para.y_rev = 1;
            break;
        case GE2D_ROTATION_270:
            ge2d_config_ex.dst_xy_swap = 1;
            ge2d_config_ex.src_para.x_rev = 1;
            break;
        default:
            break;
    }

    switch (pge2dinfo->dst_info.rotation) {
        case GE2D_ROTATION_0:
            break;
        case GE2D_ROTATION_90:
            ge2d_config_ex.dst_xy_swap = 1;
            ge2d_config_ex.dst_para.x_rev = 1;
            break;
        case GE2D_ROTATION_180:
            ge2d_config_ex.dst_para.x_rev = 1;
            ge2d_config_ex.dst_para.y_rev = 1;
            break;
        case GE2D_ROTATION_270:
            ge2d_config_ex.dst_xy_swap = 1;
            ge2d_config_ex.dst_para.y_rev = 1;
            break;
        default:
            break;
    }

    if (CANVAS_ALLOC == input_buffer_info->memtype) {
        if (is_one_plane_input) {
            ge2d_config_ex.src_planes[0].addr = input_buffer_info->offset[0];
            ge2d_config_ex.src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
            ge2d_config_ex.src_planes[0].w = s_canvas_w;
            ge2d_config_ex.src_planes[0].h = s_canvas_h;
        } else if ((input_buffer_info->format == PIXEL_FORMAT_YCrCb_420_SP) ||
            (input_buffer_info->format == PIXEL_FORMAT_YCbCr_420_SP_NV12)) {
            if (input_buffer_info->plane_number == 1) {
                ge2d_config_ex.src_planes[0].addr = input_buffer_info->offset[0];
                ge2d_config_ex.src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
                ge2d_config_ex.src_planes[0].w = s_canvas_w;
                ge2d_config_ex.src_planes[0].h = s_canvas_h;
                ge2d_config_ex.src_planes[1].addr = (s_canvas_w * s_canvas_h);
                ge2d_config_ex.src_planes[1].shared_fd = 0;
                ge2d_config_ex.src_planes[1].w = s_canvas_w;
                ge2d_config_ex.src_planes[1].h = s_canvas_h/2;
            } else if (input_buffer_info->plane_number == 2) {
                ge2d_config_ex.src_planes[0].addr = input_buffer_info->offset[0];
                ge2d_config_ex.src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
                ge2d_config_ex.src_planes[0].w = s_canvas_w;
                ge2d_config_ex.src_planes[0].h = s_canvas_h;
                ge2d_config_ex.src_planes[1].addr = input_buffer_info->offset[1];
                ge2d_config_ex.src_planes[1].shared_fd = input_buffer_info->shared_fd[1];
                ge2d_config_ex.src_planes[1].w = s_canvas_w;
                ge2d_config_ex.src_planes[1].h = s_canvas_h/2;
            }
        } else if (input_buffer_info->format == PIXEL_FORMAT_Y8 ||
                   input_buffer_info->format == PIXEL_FORMAT_CLUT8 ||
                   input_buffer_info->format == PIXEL_FORMAT_ALPHA8) {
            ge2d_config_ex.src_planes[0].addr = input_buffer_info->offset[0];
            ge2d_config_ex.src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
            ge2d_config_ex.src_planes[0].w = s_canvas_w;
            ge2d_config_ex.src_planes[0].h = s_canvas_h;
        } else if (input_buffer_info->format == PIXEL_FORMAT_YV12) {
            if (input_buffer_info->plane_number == 1) {
            ge2d_config_ex.src_planes[0].addr = input_buffer_info->offset[0];
            ge2d_config_ex.src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
            ge2d_config_ex.src_planes[0].w = s_canvas_w;
            ge2d_config_ex.src_planes[0].h = s_canvas_h;
            /* android is ycrcb,kernel is ycbcr,swap the addr */
            ge2d_config_ex.src_planes[1].addr = YV12_Y_ALIGNED(s_canvas_w) *
                s_canvas_h + CANVAS_ALIGNED(s_canvas_w/2) * s_canvas_h/2;
            ge2d_config_ex.src_planes[1].shared_fd = 0;
            ge2d_config_ex.src_planes[1].w = CANVAS_ALIGNED(s_canvas_w/2);
            ge2d_config_ex.src_planes[1].h = s_canvas_h/2;
            ge2d_config_ex.src_planes[2].addr = YV12_Y_ALIGNED(s_canvas_w) * s_canvas_h;
            ge2d_config_ex.src_planes[2].shared_fd = 0;
            ge2d_config_ex.src_planes[2].w = CANVAS_ALIGNED(s_canvas_w/2);
            ge2d_config_ex.src_planes[2].h = s_canvas_h/2;
            if ( !(d_canvas_w % 64) || !(d_canvas_w % 32))
                D_GE2D("yv12 src canvas_w not alligned\n ");
            } else if (input_buffer_info->plane_number == 3) {
                ge2d_config_ex.src_planes[0].addr = input_buffer_info->offset[0];
                ge2d_config_ex.src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
                ge2d_config_ex.src_planes[0].w = s_canvas_w;
                ge2d_config_ex.src_planes[0].h = s_canvas_h;
                /* android is ycrcb,kernel is ycbcr,swap the addr */
                ge2d_config_ex.src_planes[1].addr = input_buffer_info->offset[1];
                ge2d_config_ex.src_planes[1].shared_fd = input_buffer_info->shared_fd[2];
                ge2d_config_ex.src_planes[1].w = CANVAS_ALIGNED(s_canvas_w/2);
                ge2d_config_ex.src_planes[1].h = s_canvas_h/2;
                ge2d_config_ex.src_planes[2].addr = input_buffer_info->offset[2];
                ge2d_config_ex.src_planes[2].shared_fd = input_buffer_info->shared_fd[1];
                ge2d_config_ex.src_planes[2].w = CANVAS_ALIGNED(s_canvas_w/2);
                ge2d_config_ex.src_planes[2].h = s_canvas_h/2;
                if ( !(d_canvas_w % 64) || !(d_canvas_w % 32))
                    D_GE2D("yv12 src canvas_w not alligned\n ");
            }
        } else if (input_buffer_info->format == PIXEL_FORMAT_YU12) {
            if (input_buffer_info->plane_number == 1) {
            ge2d_config_ex.src_planes[0].addr = input_buffer_info->offset[0];
            ge2d_config_ex.src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
            ge2d_config_ex.src_planes[0].w = s_canvas_w;
            ge2d_config_ex.src_planes[0].h = s_canvas_h;
            ge2d_config_ex.src_planes[1].addr = YV12_Y_ALIGNED(s_canvas_w) * s_canvas_h;
            ge2d_config_ex.src_planes[1].shared_fd = 0;
            ge2d_config_ex.src_planes[1].w = CANVAS_ALIGNED(s_canvas_w/2);
            ge2d_config_ex.src_planes[1].h = s_canvas_h/2;
            ge2d_config_ex.src_planes[2].addr = YV12_Y_ALIGNED(s_canvas_w) *
                s_canvas_h + CANVAS_ALIGNED(s_canvas_w/2) * s_canvas_h/2;
            ge2d_config_ex.src_planes[2].shared_fd = 0;
            ge2d_config_ex.src_planes[2].w = CANVAS_ALIGNED(s_canvas_w/2);
            ge2d_config_ex.src_planes[2].h = s_canvas_h/2;
            if ( !(d_canvas_w % 64) || !(d_canvas_w % 32))
                D_GE2D("yu12 src canvas_w not alligned\n ");
            } else if (input_buffer_info->plane_number == 3) {
                ge2d_config_ex.src_planes[0].addr = input_buffer_info->offset[0];
                ge2d_config_ex.src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
                ge2d_config_ex.src_planes[0].w = s_canvas_w;
                ge2d_config_ex.src_planes[0].h = s_canvas_h;
                ge2d_config_ex.src_planes[1].addr = input_buffer_info->offset[1];
                ge2d_config_ex.src_planes[1].shared_fd = input_buffer_info->shared_fd[1];
                ge2d_config_ex.src_planes[1].w = CANVAS_ALIGNED(s_canvas_w/2);
                ge2d_config_ex.src_planes[1].h = s_canvas_h/2;
                ge2d_config_ex.src_planes[2].addr = input_buffer_info->offset[2];
                ge2d_config_ex.src_planes[2].shared_fd = input_buffer_info->shared_fd[2];
                ge2d_config_ex.src_planes[2].w = CANVAS_ALIGNED(s_canvas_w/2);
                ge2d_config_ex.src_planes[2].h = s_canvas_h/2;
                if ( !(d_canvas_w % 64) || !(d_canvas_w % 32))
                    D_GE2D("yu12 src canvas_w not alligned\n ");
            }
        } else if (input_buffer_info->format == PIXEL_FORMAT_YCbCr_422_SP) {
            if (input_buffer_info->plane_number == 1) {
                ge2d_config_ex.src_planes[0].addr = input_buffer_info->offset[0];
                ge2d_config_ex.src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
                ge2d_config_ex.src_planes[0].w = s_canvas_w;
                ge2d_config_ex.src_planes[0].h = s_canvas_h;
                ge2d_config_ex.src_planes[1].addr = (s_canvas_w * s_canvas_h);
                ge2d_config_ex.src_planes[1].shared_fd = 0;
                ge2d_config_ex.src_planes[1].w = s_canvas_w;
                ge2d_config_ex.src_planes[1].h = s_canvas_h;
            } else if (input_buffer_info->plane_number == 2) {
                ge2d_config_ex.src_planes[0].addr = input_buffer_info->offset[0];
                ge2d_config_ex.src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
                ge2d_config_ex.src_planes[0].w = s_canvas_w;
                ge2d_config_ex.src_planes[0].h = s_canvas_h;
                ge2d_config_ex.src_planes[1].addr = input_buffer_info->offset[1];
                ge2d_config_ex.src_planes[1].shared_fd = input_buffer_info->shared_fd[1];
                ge2d_config_ex.src_planes[1].w = s_canvas_w;
                ge2d_config_ex.src_planes[1].h = s_canvas_h;

            }
        } else {
            E_GE2D("format is not match, should config src_planes correct.\n");
            return ge2d_fail;
        }
    }


    if (CANVAS_ALLOC == output_buffer_info->memtype) {
        switch (output_buffer_info->format) {
        case PIXEL_FORMAT_RGBA_8888:
        case PIXEL_FORMAT_RGBX_8888:
        case PIXEL_FORMAT_ARGB_8888:
        case PIXEL_FORMAT_ABGR_8888:
        case PIXEL_FORMAT_RGB_888:
        case PIXEL_FORMAT_BGR_888:
        case PIXEL_FORMAT_RGB_565:
        case PIXEL_FORMAT_ARGB_4444:
        case PIXEL_FORMAT_RGBA_4444:
        case PIXEL_FORMAT_ARGB_1555:
        case PIXEL_FORMAT_YCbCr_422_UYVY:
        case PIXEL_FORMAT_BGRA_8888:
        case PIXEL_FORMAT_Y8:
        case PIXEL_FORMAT_CLUT8:
        case PIXEL_FORMAT_ALPHA8:
            if (pge2dinfo->dst_op_cnt == 0) {
                ge2d_config_ex.dst_planes[0].addr = output_buffer_info->offset[0];
                ge2d_config_ex.dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                ge2d_config_ex.dst_planes[0].w = d_canvas_w;
                ge2d_config_ex.dst_planes[0].h = d_canvas_h;
            }
            break;
        case PIXEL_FORMAT_YV12:
        case PIXEL_FORMAT_YU12:
            switch (pge2dinfo->dst_op_cnt) {
            case 0:
                ge2d_config_ex.dst_para.format = GE2D_FORMAT_S8_Y | dst_endian;
                ge2d_config_ex.dst_planes[0].addr = output_buffer_info->offset[0];
                ge2d_config_ex.dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                ge2d_config_ex.dst_planes[0].w = d_canvas_w;
                ge2d_config_ex.dst_planes[0].h = d_canvas_h;
                ge2d_config_ex.dst_para.width = d_canvas_w;
                ge2d_config_ex.dst_para.height = d_canvas_h;
                break;
            case 1:
                if (output_buffer_info->format == PIXEL_FORMAT_YV12)
                    ge2d_config_ex.dst_para.format = GE2D_FORMAT_S8_CR | dst_endian;
                else {
                    ge2d_config_ex.dst_para.format = GE2D_FORMAT_S8_CB | dst_endian;
                }
                ge2d_config_ex.dst_planes[0].w = d_canvas_w/2;
                ge2d_config_ex.dst_planes[0].h = d_canvas_h/2;
                ge2d_config_ex.dst_para.width = d_canvas_w/2;
                ge2d_config_ex.dst_para.height = d_canvas_h/2;
                if (output_buffer_info->plane_number == 1) {
                    ge2d_config_ex.dst_planes[0].addr = YV12_Y_ALIGNED(d_canvas_w) *
                        d_canvas_h;
                    ge2d_config_ex.dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                } else if (output_buffer_info->plane_number == 3) {
                    ge2d_config_ex.dst_planes[0].addr = output_buffer_info->offset[1];
                    ge2d_config_ex.dst_planes[0].shared_fd = output_buffer_info->shared_fd[1];
                }
                break;
            case 2:
                if (output_buffer_info->format == PIXEL_FORMAT_YV12)
                    ge2d_config_ex.dst_para.format = GE2D_FORMAT_S8_CB | dst_endian;
                else {
                    ge2d_config_ex.dst_para.format = GE2D_FORMAT_S8_CR | dst_endian;
                }
                ge2d_config_ex.dst_planes[0].w = d_canvas_w/2;
                ge2d_config_ex.dst_planes[0].h = d_canvas_h/2;
                ge2d_config_ex.dst_para.width = d_canvas_w/2;
                ge2d_config_ex.dst_para.height = d_canvas_h/2;
                if (output_buffer_info->plane_number == 1) {
                    ge2d_config_ex.dst_planes[0].addr = YV12_Y_ALIGNED(d_canvas_w) *
                        d_canvas_h + CANVAS_ALIGNED(d_canvas_w/2) * d_canvas_h/2;
                    ge2d_config_ex.dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                } else if (output_buffer_info->plane_number == 3) {
                    ge2d_config_ex.dst_planes[0].addr = output_buffer_info->offset[2];
                    ge2d_config_ex.dst_planes[0].shared_fd = output_buffer_info->shared_fd[2];
                }
                break;
            default:
                break;
            }
            break;
        case PIXEL_FORMAT_YCbCr_422_SP:
            if (output_buffer_info->plane_number == 1) {
                ge2d_config_ex.dst_planes[0].addr = output_buffer_info->offset[0];
                ge2d_config_ex.dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                ge2d_config_ex.dst_planes[0].w = d_canvas_w;
                ge2d_config_ex.dst_planes[0].h = d_canvas_h;
                ge2d_config_ex.dst_planes[1].addr = (d_canvas_w * d_canvas_h);
                ge2d_config_ex.dst_planes[1].shared_fd = 0;
                ge2d_config_ex.dst_planes[1].w = d_canvas_w;
                ge2d_config_ex.dst_planes[1].h = d_canvas_h;
            } else if (output_buffer_info->plane_number == 2) {
                ge2d_config_ex.dst_planes[0].addr = output_buffer_info->offset[0];
                ge2d_config_ex.dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                ge2d_config_ex.dst_planes[0].w = d_canvas_w;
                ge2d_config_ex.dst_planes[0].h = d_canvas_h;
                ge2d_config_ex.dst_planes[1].addr = output_buffer_info->offset[1];
                ge2d_config_ex.dst_planes[1].shared_fd = output_buffer_info->shared_fd[1];
                ge2d_config_ex.dst_planes[1].w = d_canvas_w;
                ge2d_config_ex.dst_planes[1].h = d_canvas_h;
            }
            break;
        case PIXEL_FORMAT_YCrCb_420_SP:
        case PIXEL_FORMAT_YCbCr_420_SP_NV12:
            if (output_buffer_info->plane_number == 1) {
                ge2d_config_ex.dst_planes[0].addr = output_buffer_info->offset[0];
                ge2d_config_ex.dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                ge2d_config_ex.dst_planes[0].w = d_canvas_w;
                ge2d_config_ex.dst_planes[0].h = d_canvas_h;
                ge2d_config_ex.dst_planes[1].addr = (d_canvas_w * d_canvas_h);
                ge2d_config_ex.dst_planes[1].shared_fd = 0;
                ge2d_config_ex.dst_planes[1].w = d_canvas_w;
                ge2d_config_ex.dst_planes[1].h = d_canvas_h/2;
            } else if (output_buffer_info->plane_number == 2) {
                ge2d_config_ex.dst_planes[0].addr = output_buffer_info->offset[0];
                ge2d_config_ex.dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                ge2d_config_ex.dst_planes[0].w = d_canvas_w;
                ge2d_config_ex.dst_planes[0].h = d_canvas_h;
                ge2d_config_ex.dst_planes[1].addr = output_buffer_info->offset[1];
                ge2d_config_ex.dst_planes[1].shared_fd = output_buffer_info->shared_fd[1];
                ge2d_config_ex.dst_planes[1].w = d_canvas_w;
                ge2d_config_ex.dst_planes[1].h = d_canvas_h/2;
            }
            break;
        }
    }
    ge2d_config_ex.alu_const_color = pge2dinfo->const_color;
    ge2d_config_ex.src1_gb_alpha = input_buffer_info->plane_alpha & 0xff;
    ge2d_config_ex.src1_gb_alpha_en = 1;
    D_GE2D("bilt:src1_cmult_asel=%d,src2_cmult_ase2=%d,gl_alpha=%x,src1_gb_alpha_en=%d\n",
        ge2d_config_ex.src1_cmult_asel,
        ge2d_config_ex.src2_cmult_asel,
        ge2d_config_ex.src1_gb_alpha,
        ge2d_config_ex.src1_gb_alpha_en);

    if (pge2dinfo->cap_attr == -1)
        ret = ioctl(fd, GE2D_CONFIG_EX_ION_EN, &ge2d_config_ex);
    else
        ret = ioctl(fd, GE2D_CONFIG_EX_ION, &ge2d_config_ex);
    if (ret < 0) {
        E_GE2D("ge2d config ex ion failed. \n");
        return ge2d_fail;
    }

    return ge2d_success;
}


static int ge2d_blend_config_ex_ion(int fd,aml_ge2d_info_t *pge2dinfo)
{
    int ret = -1;
    struct config_para_ex_ion_s ge2d_config_ex;
    int src_format = 0xffffffff,src2_format = 0xffffffff,dst_format = 0xffffffff;
    int s_canvas_w = 0;
    int s_canvas_h = 0;
    int s2_canvas_w = 0;
    int s2_canvas_h = 0;
    int d_canvas_w = 0;
    int d_canvas_h = 0;
    int is_one_plane_input = -1;
    int is_one_plane_input2 = -1;
    int is_one_plane_output = -1;
    int bpp = 0;
    buffer_info_t* input_buffer_info = &pge2dinfo->src_info[0];
    buffer_info_t* input2_buffer_info = &pge2dinfo->src_info[1];
    buffer_info_t* output_buffer_info = &pge2dinfo->dst_info;
    unsigned int src1_endian = input_buffer_info->endain ? GE2D_BIG_ENDIAN : GE2D_LITTLE_ENDIAN;
    unsigned int src2_endian = input2_buffer_info->endain ? GE2D_BIG_ENDIAN : GE2D_LITTLE_ENDIAN;
    unsigned int dst_endian = output_buffer_info->endain ? GE2D_BIG_ENDIAN : GE2D_LITTLE_ENDIAN;

    /* src2 not support nv21/nv12/yv12, swap src1 and src2 */
    if (is_need_swap_src2(input2_buffer_info->format, input2_buffer_info,
                          output_buffer_info, pge2dinfo->cap_attr)) {
        input_buffer_info = &pge2dinfo->src_info[1];
        input2_buffer_info = &pge2dinfo->src_info[0];
        pge2dinfo->b_src_swap = 1;
        D_GE2D("NOTE:src2 not support nv21/nv12, swap src1 and src2!\n");
    }
    else if (!(pge2dinfo->cap_attr & SRC2_ALPHA) &&
        (input_buffer_info->plane_alpha == 0xff) &&
        (input2_buffer_info->plane_alpha != 0xff) &&
        (input_buffer_info->layer_mode != LAYER_MODE_NON) &&
        (input2_buffer_info->layer_mode == LAYER_MODE_PREMULTIPLIED)) {
        /* swap src1 and src2 buffer to process plane alpha*/
        /* TODO: need check the src1 rect and src2 rect */
        input_buffer_info = &pge2dinfo->src_info[1];
        input2_buffer_info = &pge2dinfo->src_info[0];
        //pge2dinfo->gl_alpha &= SRC1_GB_ALPHA_ENABLE;
        //pge2dinfo->gl_alpha |= (input_buffer_info->plane_alpha & 0xff);
        pge2dinfo->b_src_swap = 1;
    }
    else
        pge2dinfo->b_src_swap = 0;
    D_GE2D("b_src_swap=%d\n",pge2dinfo->b_src_swap);
    if (input_buffer_info->plane_number < 1 ||
        input_buffer_info->plane_number > GE2D_MAX_PLANE)
        input_buffer_info->plane_number = 1;
    if (input2_buffer_info->plane_number < 1 ||
        input2_buffer_info->plane_number > GE2D_MAX_PLANE)
        input2_buffer_info->plane_number = 1;
    if (output_buffer_info->plane_number < 1 ||
        output_buffer_info->plane_number > GE2D_MAX_PLANE)
        output_buffer_info->plane_number = 1;
    memset(&ge2d_config_ex, 0, sizeof(struct config_para_ex_ion_s ));

    if (CANVAS_ALLOC == input_buffer_info->memtype) {
        is_one_plane_input = pixel_to_ge2d_format(&input_buffer_info->format,&src_format,&bpp);
        src_format |= src1_endian;
        if ((int)0xffffffff == src_format) {
            E_GE2D("can't get proper ge2d format\n" );
            return ge2d_fail;
        }
        ge2d_set_canvas(bpp,input_buffer_info->canvas_w,input_buffer_info->canvas_h,&s_canvas_w,&s_canvas_h);
    }
    if ((CANVAS_ALLOC == input2_buffer_info->memtype)) {
        is_one_plane_input2 = pixel_to_ge2d_format(&input2_buffer_info->format,&src2_format,&bpp);
        src2_format |= src2_endian;
        if ((int)0xffffffff == src2_format) {
            E_GE2D("can't get proper ge2d format\n" );
            return ge2d_fail;
        }
        ge2d_set_canvas(bpp,input2_buffer_info->canvas_w,input2_buffer_info->canvas_h,&s2_canvas_w,&s2_canvas_h);
    }

    if ((CANVAS_ALLOC == output_buffer_info->memtype)) {
        is_one_plane_output = pixel_to_ge2d_format(&output_buffer_info->format,&dst_format,&bpp);
        dst_format |= dst_endian;
        if ((int)0xffffffff == dst_format) {
            E_GE2D("can't get proper ge2d format\n" );
            return ge2d_fail;
        }
        ge2d_set_canvas(bpp,output_buffer_info->canvas_w,output_buffer_info->canvas_h,&d_canvas_w,&d_canvas_h);

    }
    D_GE2D("ge2d_blit_config_ex,memtype=%x,src_format=%x,s_canvas_w=%d,s_canvas_h=%d,rotation=%d\n",
        input_buffer_info->memtype,src_format,s_canvas_w,s_canvas_h,input_buffer_info->rotation);

    D_GE2D("ge2d_blit_config_ex,memtype=%x,src2_format=%x,s2_canvas_w=%d,s2_canvas_h=%d,rotation=%d\n",
        input2_buffer_info->memtype,src2_format,s2_canvas_w,s2_canvas_h,input2_buffer_info->rotation);

    D_GE2D("ge2d_blit_config_ex,memtype=%x,dst_format=%x,d_canvas_w=%d,d_canvas_h=%d,rotation=%d\n",
        output_buffer_info->memtype,dst_format,d_canvas_w,d_canvas_h,output_buffer_info->rotation);

    ge2d_config_ex.src_para.mem_type = input_buffer_info->memtype;
    ge2d_config_ex.src_para.format = src_format;
    ge2d_config_ex.src_para.left = input_buffer_info->rect.x;
    ge2d_config_ex.src_para.top = input_buffer_info->rect.y;
    ge2d_config_ex.src_para.width = input_buffer_info->rect.w;
    ge2d_config_ex.src_para.height = input_buffer_info->rect.h;
    ge2d_config_ex.src_para.color = input_buffer_info->def_color;
    ge2d_config_ex.src_para.fill_color_en = input_buffer_info->fill_color_en;

    /* top layer factor, use premult/coverage/none
     * depends on top layer mode
     */
    if (pge2dinfo->src_info[0].layer_mode == LAYER_MODE_PREMULTIPLIED)
        pge2dinfo->blend_mode = BLEND_MODE_PREMULTIPLIED;
    else if (pge2dinfo->src_info[0].layer_mode == LAYER_MODE_COVERAGE)
        pge2dinfo->blend_mode = BLEND_MODE_COVERAGE;
    else
        pge2dinfo->blend_mode = BLEND_MODE_NONE;

    if (pge2dinfo->src_info[0].layer_mode == LAYER_MODE_PREMULTIPLIED)
        ge2d_config_ex.src1_cmult_asel = 2;
    else if (pge2dinfo->src_info[0].layer_mode == LAYER_MODE_COVERAGE)
        ge2d_config_ex.src1_cmult_asel = 0;
    else if (pge2dinfo->src_info[0].layer_mode == LAYER_MODE_NON)
        ge2d_config_ex.src1_cmult_asel = 0;

    ge2d_config_ex.src2_para.mem_type = input2_buffer_info->memtype;
    ge2d_config_ex.src2_para.format = src2_format;
    ge2d_config_ex.src2_para.left = input2_buffer_info->rect.x;
    ge2d_config_ex.src2_para.top = input2_buffer_info->rect.y;
    ge2d_config_ex.src2_para.width = input2_buffer_info->rect.w;
    ge2d_config_ex.src2_para.height = input2_buffer_info->rect.h;
    ge2d_config_ex.src2_para.color = input2_buffer_info->def_color;
    ge2d_config_ex.src2_para.fill_color_en = input2_buffer_info->fill_color_en;
    if (pge2dinfo->cap_attr & SRC2_ALPHA) {
        if (pge2dinfo->src_info[1].layer_mode == LAYER_MODE_PREMULTIPLIED)
            ge2d_config_ex.src2_cmult_asel = 2;
        else if (pge2dinfo->src_info[1].layer_mode == LAYER_MODE_COVERAGE)
            ge2d_config_ex.src2_cmult_asel = 1;
        else if (pge2dinfo->src_info[1].layer_mode == LAYER_MODE_NON)
            ge2d_config_ex.src2_cmult_asel= 0;
        if ((pge2dinfo->src_info[0].layer_mode == LAYER_MODE_PREMULTIPLIED) && (pge2dinfo->src_info[1].layer_mode == LAYER_MODE_PREMULTIPLIED))
            ge2d_config_ex.src2_cmult_ad = 0;
        else if ((pge2dinfo->src_info[0].layer_mode == LAYER_MODE_COVERAGE) && (pge2dinfo->src_info[1].layer_mode == LAYER_MODE_PREMULTIPLIED))
            ge2d_config_ex.src2_cmult_ad = 0;
        else if ((pge2dinfo->src_info[0].layer_mode == LAYER_MODE_PREMULTIPLIED) && (pge2dinfo->src_info[1].layer_mode == LAYER_MODE_COVERAGE))
            ge2d_config_ex.src2_cmult_ad = 1;
        else if ((pge2dinfo->src_info[0].layer_mode == LAYER_MODE_COVERAGE) && (pge2dinfo->src_info[1].layer_mode == LAYER_MODE_COVERAGE))
            ge2d_config_ex.src2_cmult_ad = 1;
        else if ((pge2dinfo->src_info[0].layer_mode == LAYER_MODE_COVERAGE) && (pge2dinfo->src_info[1].layer_mode == LAYER_MODE_NON))
            ge2d_config_ex.src2_cmult_ad = 3;
        else
            ge2d_config_ex.src2_cmult_ad = 0;
    } else
        ge2d_config_ex.src2_cmult_asel= 0;
    ge2d_config_ex.dst_para.mem_type = output_buffer_info->memtype;
    ge2d_config_ex.dst_para.format = dst_format;
    ge2d_config_ex.dst_para.left = 0;
    ge2d_config_ex.dst_para.top = 0;
    ge2d_config_ex.dst_para.width = d_canvas_w;
    ge2d_config_ex.dst_para.height = d_canvas_h;
    switch (pge2dinfo->src_info[0].rotation) {
        case GE2D_ROTATION_0:
            break;
        case GE2D_ROTATION_90:
            ge2d_config_ex.dst_xy_swap = 1;
            ge2d_config_ex.src_para.y_rev = 1;
            break;
        case GE2D_ROTATION_180:
            ge2d_config_ex.src_para.x_rev = 1;
            ge2d_config_ex.src_para.y_rev = 1;
            break;
        case GE2D_ROTATION_270:
            ge2d_config_ex.dst_xy_swap = 1;
            ge2d_config_ex.src_para.x_rev = 1;
            break;
        default:
            break;
    }

    switch (pge2dinfo->src_info[1].rotation) {
        case GE2D_ROTATION_0:
            break;
        case GE2D_ROTATION_90:
            ge2d_config_ex.dst_xy_swap = 1;
            ge2d_config_ex.src2_para.y_rev = 1;
            break;
        case GE2D_ROTATION_180:
            ge2d_config_ex.src2_para.x_rev = 1;
            ge2d_config_ex.src2_para.y_rev = 1;
            break;
        case GE2D_ROTATION_270:
            ge2d_config_ex.dst_xy_swap = 1;
            ge2d_config_ex.src2_para.x_rev = 1;
            break;
        default:
            break;
    }

    switch (pge2dinfo->dst_info.rotation) {
         case GE2D_ROTATION_0:
             break;
         case GE2D_ROTATION_90:
             ge2d_config_ex.dst_xy_swap = 1;
             ge2d_config_ex.dst_para.x_rev = 1;
             break;
         case GE2D_ROTATION_180:
             ge2d_config_ex.dst_para.x_rev = 1;
             ge2d_config_ex.dst_para.y_rev = 1;
             break;
         case GE2D_ROTATION_270:
             ge2d_config_ex.dst_xy_swap = 1;
             ge2d_config_ex.dst_para.y_rev = 1;
             break;
         default:
             break;
    }

    if (CANVAS_ALLOC == input_buffer_info->memtype) {
        if (is_one_plane_input) {
            ge2d_config_ex.src_planes[0].addr = input_buffer_info->offset[0];
            ge2d_config_ex.src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
            ge2d_config_ex.src_planes[0].w = s_canvas_w;
            ge2d_config_ex.src_planes[0].h = s_canvas_h;
        } else if ((input_buffer_info->format == PIXEL_FORMAT_YCrCb_420_SP) ||
            (input_buffer_info->format ==PIXEL_FORMAT_YCbCr_420_SP_NV12)) {
            if (input_buffer_info->plane_number == 1) {
                ge2d_config_ex.src_planes[0].addr = input_buffer_info->offset[0];
                ge2d_config_ex.src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
                ge2d_config_ex.src_planes[0].w = s_canvas_w;
                ge2d_config_ex.src_planes[0].h = s_canvas_h;
                ge2d_config_ex.src_planes[1].addr = (s_canvas_w * s_canvas_h);
                ge2d_config_ex.src_planes[1].shared_fd = 0;
                ge2d_config_ex.src_planes[1].w = s_canvas_w;
                ge2d_config_ex.src_planes[1].h = s_canvas_h/2;
            } else if (input_buffer_info->plane_number == 2) {
                ge2d_config_ex.src_planes[0].addr = input_buffer_info->offset[0];
                ge2d_config_ex.src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
                ge2d_config_ex.src_planes[0].w = s_canvas_w;
                ge2d_config_ex.src_planes[0].h = s_canvas_h;
                ge2d_config_ex.src_planes[1].addr = input_buffer_info->offset[1];
                ge2d_config_ex.src_planes[1].shared_fd = input_buffer_info->shared_fd[1];
                ge2d_config_ex.src_planes[1].w = s_canvas_w;
                ge2d_config_ex.src_planes[1].h = s_canvas_h/2;
            }
        } else if (input_buffer_info->format == PIXEL_FORMAT_Y8 ||
                   input_buffer_info->format == PIXEL_FORMAT_CLUT8 ||
                   input_buffer_info->format == PIXEL_FORMAT_ALPHA8) {
            ge2d_config_ex.src_planes[0].addr = input_buffer_info->offset[0];
            ge2d_config_ex.src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
            ge2d_config_ex.src_planes[0].w = s_canvas_w;
            ge2d_config_ex.src_planes[0].h = s_canvas_h;
        } else if (input_buffer_info->format == PIXEL_FORMAT_YV12) {
            if (input_buffer_info->plane_number == 1) {
                ge2d_config_ex.src_planes[0].addr = input_buffer_info->offset[0];
                ge2d_config_ex.src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
                ge2d_config_ex.src_planes[0].w = s_canvas_w;
                ge2d_config_ex.src_planes[0].h = s_canvas_h;
                /* android is ycrcb,kernel is ycbcr,swap the addr */
                ge2d_config_ex.src_planes[1].addr = YV12_Y_ALIGNED(s_canvas_w) *
                    s_canvas_h + CANVAS_ALIGNED(s_canvas_w/2 ) * s_canvas_h/2;
                ge2d_config_ex.src_planes[1].shared_fd = 0;
                ge2d_config_ex.src_planes[1].w = CANVAS_ALIGNED(s_canvas_w/2);
                ge2d_config_ex.src_planes[1].h = s_canvas_h/2;
                ge2d_config_ex.src_planes[2].addr = YV12_Y_ALIGNED(s_canvas_w) * s_canvas_h;
                ge2d_config_ex.src_planes[2].shared_fd = 0;
                ge2d_config_ex.src_planes[2].w = CANVAS_ALIGNED(s_canvas_w/2);
                ge2d_config_ex.src_planes[2].h = s_canvas_h/2;
                if ( !(d_canvas_w % 64) || !(d_canvas_w % 32))
                    D_GE2D("yv12 src canvas_w not alligned\n ");
            } else if (input_buffer_info->plane_number == 3) {
                ge2d_config_ex.src_planes[0].addr = input_buffer_info->offset[0];
                ge2d_config_ex.src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
                ge2d_config_ex.src_planes[0].w = s_canvas_w;
                ge2d_config_ex.src_planes[0].h = s_canvas_h;
                /* android is ycrcb,kernel is ycbcr,swap the addr */
                ge2d_config_ex.src_planes[1].addr = input_buffer_info->offset[1];
                ge2d_config_ex.src_planes[1].shared_fd = input_buffer_info->shared_fd[2];
                ge2d_config_ex.src_planes[1].w = CANVAS_ALIGNED(s_canvas_w/2);
                ge2d_config_ex.src_planes[1].h = s_canvas_h/2;
                ge2d_config_ex.src_planes[2].addr = input_buffer_info->offset[2];
                ge2d_config_ex.src_planes[2].shared_fd = input_buffer_info->shared_fd[1];
                ge2d_config_ex.src_planes[2].w = CANVAS_ALIGNED(s_canvas_w/2);
                ge2d_config_ex.src_planes[2].h = s_canvas_h/2;
                if ( !(d_canvas_w % 64) || !(d_canvas_w % 32))
                    D_GE2D("yv12 src canvas_w not alligned\n ");
            }
        } else if (input_buffer_info->format == PIXEL_FORMAT_YU12) {
            if (input_buffer_info->plane_number == 1) {
                ge2d_config_ex.src_planes[0].addr = input_buffer_info->offset[0];
                ge2d_config_ex.src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
                ge2d_config_ex.src_planes[0].w = s_canvas_w;
                ge2d_config_ex.src_planes[0].h = s_canvas_h;
                ge2d_config_ex.src_planes[1].addr = YV12_Y_ALIGNED(s_canvas_w) * s_canvas_h;
                ge2d_config_ex.src_planes[1].shared_fd = 0;
                ge2d_config_ex.src_planes[1].w = CANVAS_ALIGNED(s_canvas_w/2);
                ge2d_config_ex.src_planes[1].h = s_canvas_h/2;
                ge2d_config_ex.src_planes[2].addr = YV12_Y_ALIGNED(s_canvas_w) *
                    s_canvas_h + CANVAS_ALIGNED(s_canvas_w/2 ) * s_canvas_h/2;
                ge2d_config_ex.src_planes[2].shared_fd = 0;
                ge2d_config_ex.src_planes[2].w = CANVAS_ALIGNED(s_canvas_w/2);
                ge2d_config_ex.src_planes[2].h = s_canvas_h/2;
                if ( !(d_canvas_w % 64) || !(d_canvas_w % 32))
                    D_GE2D("yu12 src canvas_w not alligned\n ");
            } else if (input_buffer_info->plane_number == 3) {
                ge2d_config_ex.src_planes[0].addr = input_buffer_info->offset[0];
                ge2d_config_ex.src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
                ge2d_config_ex.src_planes[0].w = s_canvas_w;
                ge2d_config_ex.src_planes[0].h = s_canvas_h;
                ge2d_config_ex.src_planes[1].addr = input_buffer_info->offset[1];
                ge2d_config_ex.src_planes[1].shared_fd = input_buffer_info->shared_fd[1];
                ge2d_config_ex.src_planes[1].w = CANVAS_ALIGNED(s_canvas_w/2);
                ge2d_config_ex.src_planes[1].h = s_canvas_h/2;
                ge2d_config_ex.src_planes[2].addr = input_buffer_info->offset[2];
                ge2d_config_ex.src_planes[2].shared_fd = input_buffer_info->shared_fd[2];
                ge2d_config_ex.src_planes[2].w = CANVAS_ALIGNED(s_canvas_w/2);
                ge2d_config_ex.src_planes[2].h = s_canvas_h/2;
                if ( !(d_canvas_w % 64) || !(d_canvas_w % 32))
                    D_GE2D("yu12 src canvas_w not alligned\n ");
            }
        } else if (input_buffer_info->format == PIXEL_FORMAT_YCbCr_422_SP) {
            if (input_buffer_info->plane_number == 1) {
                ge2d_config_ex.src_planes[0].addr = input_buffer_info->offset[0];
                ge2d_config_ex.src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
                ge2d_config_ex.src_planes[0].w = s_canvas_w;
                ge2d_config_ex.src_planes[0].h = s_canvas_h;
                ge2d_config_ex.src_planes[1].addr = (s_canvas_w * s_canvas_h);
                ge2d_config_ex.src_planes[1].shared_fd = 0;
                ge2d_config_ex.src_planes[1].w = s_canvas_w;
                ge2d_config_ex.src_planes[1].h = s_canvas_h;
            } else if (input_buffer_info->plane_number == 2) {
                ge2d_config_ex.src_planes[0].addr = input_buffer_info->offset[0];
                ge2d_config_ex.src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
                ge2d_config_ex.src_planes[0].w = s_canvas_w;
                ge2d_config_ex.src_planes[0].h = s_canvas_h;
                ge2d_config_ex.src_planes[1].addr = input_buffer_info->offset[1];
                ge2d_config_ex.src_planes[1].shared_fd = input_buffer_info->shared_fd[1];
                ge2d_config_ex.src_planes[1].w = s_canvas_w;
                ge2d_config_ex.src_planes[1].h = s_canvas_h;
            }
        } else {
            E_GE2D("format is not match, should config src_planes correct.\n");
            return ge2d_fail;
        }
    }


    if (CANVAS_ALLOC == input2_buffer_info->memtype) {
        if (is_one_plane_input2) {
            ge2d_config_ex.src2_planes[0].addr = input2_buffer_info->offset[0];
            ge2d_config_ex.src2_planes[0].shared_fd = input2_buffer_info->shared_fd[0];
            ge2d_config_ex.src2_planes[0].w = s2_canvas_w;
            ge2d_config_ex.src2_planes[0].h = s2_canvas_h;
        } else if ((input2_buffer_info->format == PIXEL_FORMAT_YCrCb_420_SP) ||
            (input2_buffer_info->format == PIXEL_FORMAT_YCbCr_420_SP_NV12)) {
            if (input2_buffer_info->plane_number == 1) {
                ge2d_config_ex.src2_planes[0].addr = input2_buffer_info->offset[0];
                ge2d_config_ex.src2_planes[0].shared_fd = input2_buffer_info->shared_fd[0];
                ge2d_config_ex.src2_planes[0].w = s2_canvas_w;
                ge2d_config_ex.src2_planes[0].h = s2_canvas_h;
                ge2d_config_ex.src2_planes[1].addr = (s2_canvas_w * s2_canvas_h);
                ge2d_config_ex.src2_planes[1].shared_fd = 0;
                ge2d_config_ex.src2_planes[1].w = s2_canvas_w;
                ge2d_config_ex.src2_planes[1].h = s2_canvas_h/2;
            } else if (input2_buffer_info->plane_number == 2) {
                ge2d_config_ex.src2_planes[0].addr = input2_buffer_info->offset[0];
                ge2d_config_ex.src2_planes[0].shared_fd = input2_buffer_info->shared_fd[0];
                ge2d_config_ex.src2_planes[0].w = s2_canvas_w;
                ge2d_config_ex.src2_planes[0].h = s2_canvas_h;
                ge2d_config_ex.src2_planes[1].addr = input2_buffer_info->offset[1];
                ge2d_config_ex.src2_planes[1].shared_fd = input2_buffer_info->shared_fd[1];
                ge2d_config_ex.src2_planes[1].w = s2_canvas_w;
                ge2d_config_ex.src2_planes[1].h = s2_canvas_h/2;
            }
        } else if (input2_buffer_info->format == PIXEL_FORMAT_Y8 ||
                   input2_buffer_info->format == PIXEL_FORMAT_CLUT8 ||
                   input2_buffer_info->format == PIXEL_FORMAT_ALPHA8) {
            ge2d_config_ex.src2_planes[0].addr = input2_buffer_info->offset[0];
            ge2d_config_ex.src2_planes[0].shared_fd = input2_buffer_info->shared_fd[0];
            ge2d_config_ex.src2_planes[0].w = s2_canvas_w;
            ge2d_config_ex.src2_planes[0].h = s2_canvas_h;
        } else if (input2_buffer_info->format == PIXEL_FORMAT_YV12) {
            if (input2_buffer_info->plane_number == 1) {
                ge2d_config_ex.src2_planes[0].addr = input2_buffer_info->offset[0];
                ge2d_config_ex.src2_planes[0].shared_fd = input2_buffer_info->shared_fd[0];
                ge2d_config_ex.src2_planes[0].w = s2_canvas_w;
                ge2d_config_ex.src2_planes[0].h = s2_canvas_h;
                /* android is ycrcb,kernel is ycbcr,swap the addr */
                ge2d_config_ex.src2_planes[1].addr = YV12_Y_ALIGNED(s2_canvas_w) *
                    s2_canvas_h + CANVAS_ALIGNED(s2_canvas_w/2)* s2_canvas_h/2;
                ge2d_config_ex.src2_planes[1].shared_fd = 0;
                ge2d_config_ex.src2_planes[1].w = CANVAS_ALIGNED(s2_canvas_w/2);
                ge2d_config_ex.src2_planes[1].h = s2_canvas_h/2;
                ge2d_config_ex.src2_planes[2].addr = YV12_Y_ALIGNED(s2_canvas_w) * s2_canvas_h;
                ge2d_config_ex.src2_planes[2].shared_fd = 0;
                ge2d_config_ex.src2_planes[2].w = CANVAS_ALIGNED(s2_canvas_w/2);
                ge2d_config_ex.src2_planes[2].h = s2_canvas_h/2;
                if ( !(d_canvas_w % 64) || !(d_canvas_w % 32))
                    D_GE2D("yv12 src2 canvas_w not alligned\n ");
            } else if (input2_buffer_info->plane_number == 3) {
                ge2d_config_ex.src2_planes[0].addr = input2_buffer_info->offset[0];
                ge2d_config_ex.src2_planes[0].shared_fd = input2_buffer_info->shared_fd[0];
                ge2d_config_ex.src2_planes[0].w = s2_canvas_w;
                ge2d_config_ex.src2_planes[0].h = s2_canvas_h;
                /* android is ycrcb,kernel is ycbcr,swap the addr */
                ge2d_config_ex.src2_planes[1].addr = input2_buffer_info->offset[1];
                ge2d_config_ex.src2_planes[1].shared_fd = input2_buffer_info->shared_fd[2];
                ge2d_config_ex.src2_planes[1].w = CANVAS_ALIGNED(s2_canvas_w/2);
                ge2d_config_ex.src2_planes[1].h = s2_canvas_h/2;
                ge2d_config_ex.src2_planes[2].addr = input2_buffer_info->offset[2];
                ge2d_config_ex.src2_planes[2].shared_fd = input2_buffer_info->shared_fd[1];
                ge2d_config_ex.src2_planes[2].w = CANVAS_ALIGNED(s2_canvas_w/2);
                ge2d_config_ex.src2_planes[2].h = s2_canvas_h/2;
                if ( !(d_canvas_w % 64) || !(d_canvas_w % 32))
                    D_GE2D("yv12 src2 canvas_w not alligned\n ");
            }
        } else if (input2_buffer_info->format == PIXEL_FORMAT_YU12) {
            if (input2_buffer_info->plane_number == 1) {
                ge2d_config_ex.src2_planes[0].addr = input2_buffer_info->offset[0];
                ge2d_config_ex.src2_planes[0].shared_fd = input2_buffer_info->shared_fd[0];
                ge2d_config_ex.src2_planes[0].w = s2_canvas_w;
                ge2d_config_ex.src2_planes[0].h = s2_canvas_h;
                ge2d_config_ex.src2_planes[1].addr = YV12_Y_ALIGNED(s2_canvas_w) * s2_canvas_h;
                ge2d_config_ex.src2_planes[1].shared_fd = 0;
                ge2d_config_ex.src2_planes[1].w = CANVAS_ALIGNED(s2_canvas_w/2);
                ge2d_config_ex.src2_planes[1].h = s2_canvas_h/2;
                ge2d_config_ex.src2_planes[2].addr = YV12_Y_ALIGNED(s2_canvas_w) *
                    s2_canvas_h + CANVAS_ALIGNED(s2_canvas_w/2)* s2_canvas_h/2;
                ge2d_config_ex.src2_planes[2].shared_fd = 0;
                ge2d_config_ex.src2_planes[2].w = CANVAS_ALIGNED(s2_canvas_w/2);
                ge2d_config_ex.src2_planes[2].h = s2_canvas_h/2;
                if ( !(d_canvas_w % 64) || !(d_canvas_w % 32))
                    D_GE2D("yu12 src2 canvas_w not alligned\n ");
            } else if (input2_buffer_info->plane_number == 3) {
                ge2d_config_ex.src2_planes[0].addr = input2_buffer_info->offset[0];
                ge2d_config_ex.src2_planes[0].shared_fd = input2_buffer_info->shared_fd[0];
                ge2d_config_ex.src2_planes[0].w = s2_canvas_w;
                ge2d_config_ex.src2_planes[0].h = s2_canvas_h;
                ge2d_config_ex.src2_planes[1].addr = input2_buffer_info->offset[1];
                ge2d_config_ex.src2_planes[1].shared_fd = input2_buffer_info->shared_fd[1];
                ge2d_config_ex.src2_planes[1].w = CANVAS_ALIGNED(s2_canvas_w/2);
                ge2d_config_ex.src2_planes[1].h = s2_canvas_h/2;
                ge2d_config_ex.src2_planes[2].addr = input2_buffer_info->offset[2];
                ge2d_config_ex.src2_planes[2].shared_fd = input2_buffer_info->shared_fd[2];
                ge2d_config_ex.src2_planes[2].w = CANVAS_ALIGNED(s2_canvas_w/2);
                ge2d_config_ex.src2_planes[2].h = s2_canvas_h/2;
                if ( !(d_canvas_w % 64) || !(d_canvas_w % 32))
                    D_GE2D("yu12 src2 canvas_w not alligned\n ");
            }
        } else if (input2_buffer_info->format == PIXEL_FORMAT_YCbCr_422_SP) {
            if (input2_buffer_info->plane_number == 1) {
                ge2d_config_ex.src2_planes[0].addr = input2_buffer_info->offset[0];
                ge2d_config_ex.src2_planes[0].shared_fd = input2_buffer_info->shared_fd[0];
                ge2d_config_ex.src2_planes[0].w = s2_canvas_w;
                ge2d_config_ex.src2_planes[0].h = s2_canvas_h;
                ge2d_config_ex.src2_planes[1].addr = (s2_canvas_w * s2_canvas_h);
                ge2d_config_ex.src2_planes[1].shared_fd = 0;
                ge2d_config_ex.src2_planes[1].w = s2_canvas_w;
                ge2d_config_ex.src2_planes[1].h = s2_canvas_h;
            } else if (input2_buffer_info->plane_number == 2) {
                ge2d_config_ex.src2_planes[0].addr = input2_buffer_info->offset[0];
                ge2d_config_ex.src2_planes[0].shared_fd = input2_buffer_info->shared_fd[0];
                ge2d_config_ex.src2_planes[0].w = s2_canvas_w;
                ge2d_config_ex.src2_planes[0].h = s2_canvas_h;
                ge2d_config_ex.src2_planes[1].addr = input2_buffer_info->offset[1];
                ge2d_config_ex.src2_planes[1].shared_fd = input2_buffer_info->shared_fd[1];
                ge2d_config_ex.src2_planes[1].w = s2_canvas_w;
                ge2d_config_ex.src2_planes[1].h = s2_canvas_h;
            }
        } else {
            E_GE2D("format is not match, should config src2_planes correct.\n");
            return ge2d_fail;
        }
    }

    if (CANVAS_ALLOC == output_buffer_info->memtype) {
        if (is_one_plane_output) {
            ge2d_config_ex.dst_planes[0].addr = output_buffer_info->offset[0];
            ge2d_config_ex.dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
            ge2d_config_ex.dst_planes[0].w = d_canvas_w;
            ge2d_config_ex.dst_planes[0].h = d_canvas_h;
        } else if ((output_buffer_info->format == PIXEL_FORMAT_YCrCb_420_SP) ||
            (output_buffer_info->format ==PIXEL_FORMAT_YCbCr_420_SP_NV12)) {
            if (output_buffer_info->plane_number == 1) {
                ge2d_config_ex.dst_planes[0].addr = output_buffer_info->offset[0];
                ge2d_config_ex.dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                ge2d_config_ex.dst_planes[0].w = d_canvas_w;
                ge2d_config_ex.dst_planes[0].h = d_canvas_h;
                ge2d_config_ex.dst_planes[1].addr = (d_canvas_w * d_canvas_h);
                ge2d_config_ex.dst_planes[1].shared_fd = 0;
                ge2d_config_ex.dst_planes[1].w = d_canvas_w;
                ge2d_config_ex.dst_planes[1].h = d_canvas_h/2;
            } else if (output_buffer_info->plane_number == 2) {
                ge2d_config_ex.dst_planes[0].addr = output_buffer_info->offset[0];
                ge2d_config_ex.dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                ge2d_config_ex.dst_planes[0].w = d_canvas_w;
                ge2d_config_ex.dst_planes[0].h = d_canvas_h;
                ge2d_config_ex.dst_planes[1].addr = output_buffer_info->offset[1];
                ge2d_config_ex.dst_planes[1].shared_fd = output_buffer_info->shared_fd[1];
                ge2d_config_ex.dst_planes[1].w = d_canvas_w;
                ge2d_config_ex.dst_planes[1].h = d_canvas_h/2;
            }
        }  else if (output_buffer_info->format == PIXEL_FORMAT_Y8 ||
                    output_buffer_info->format == PIXEL_FORMAT_CLUT8 ||
                    output_buffer_info->format == PIXEL_FORMAT_ALPHA8) {
            ge2d_config_ex.dst_planes[0].addr = output_buffer_info->offset[0];
            ge2d_config_ex.dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
            ge2d_config_ex.dst_planes[0].w = d_canvas_w;
            ge2d_config_ex.dst_planes[0].h = d_canvas_h;
        } else if (output_buffer_info->format == PIXEL_FORMAT_YV12 ||
                   output_buffer_info->format == PIXEL_FORMAT_YU12) {
            switch (pge2dinfo->dst_op_cnt) {
            case 0:
                ge2d_config_ex.dst_para.format = GE2D_FORMAT_S8_Y | dst_endian;
                ge2d_config_ex.dst_planes[0].addr = output_buffer_info->offset[0];
                ge2d_config_ex.dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                ge2d_config_ex.dst_planes[0].w = d_canvas_w;
                ge2d_config_ex.dst_planes[0].h = d_canvas_h;
                ge2d_config_ex.dst_para.width = d_canvas_w;
                ge2d_config_ex.dst_para.height = d_canvas_h;
                break;
            case 1:
                if (output_buffer_info->format == PIXEL_FORMAT_YV12)
                    ge2d_config_ex.dst_para.format = GE2D_FORMAT_S8_CR | dst_endian;
                else
                    ge2d_config_ex.dst_para.format = GE2D_FORMAT_S8_CB | dst_endian;
                ge2d_config_ex.dst_planes[0].w = d_canvas_w/2;
                ge2d_config_ex.dst_planes[0].h = d_canvas_h/2;
                ge2d_config_ex.dst_para.width = d_canvas_w/2;
                ge2d_config_ex.dst_para.height = d_canvas_h/2;
                if (output_buffer_info->plane_number == 1) {
                    ge2d_config_ex.dst_planes[0].addr = YV12_Y_ALIGNED(d_canvas_w) *
                        d_canvas_h;
                    ge2d_config_ex.dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                } else if (output_buffer_info->plane_number == 3) {
                    ge2d_config_ex.dst_planes[0].addr = output_buffer_info->offset[1];
                    ge2d_config_ex.dst_planes[0].shared_fd = output_buffer_info->shared_fd[1];
                }
                break;
            case 2:
                if (output_buffer_info->format == PIXEL_FORMAT_YV12)
                    ge2d_config_ex.dst_para.format = GE2D_FORMAT_S8_CB | dst_endian;
                else
                    ge2d_config_ex.dst_para.format = GE2D_FORMAT_S8_CR | dst_endian;
                ge2d_config_ex.dst_planes[0].w = d_canvas_w/2;
                ge2d_config_ex.dst_planes[0].h = d_canvas_h/2;
                ge2d_config_ex.dst_para.width = d_canvas_w/2;
                ge2d_config_ex.dst_para.height = d_canvas_h/2;
                if (output_buffer_info->plane_number == 1) {
                    ge2d_config_ex.dst_planes[0].addr = YV12_Y_ALIGNED(d_canvas_w) *
                        d_canvas_h + CANVAS_ALIGNED(d_canvas_w/2) * d_canvas_h/2;
                    ge2d_config_ex.dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                } else if (output_buffer_info->plane_number == 3) {
                    ge2d_config_ex.dst_planes[0].addr = output_buffer_info->offset[2];
                    ge2d_config_ex.dst_planes[0].shared_fd = output_buffer_info->shared_fd[2];
                }
                break;
            default:
                break;
            }
        } else if (output_buffer_info->format == PIXEL_FORMAT_YCbCr_422_SP) {
            if (output_buffer_info->plane_number == 1) {
                ge2d_config_ex.dst_planes[0].addr = output_buffer_info->offset[0];
                ge2d_config_ex.dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                ge2d_config_ex.dst_planes[0].w = d_canvas_w;
                ge2d_config_ex.dst_planes[0].h = d_canvas_h;
                ge2d_config_ex.dst_planes[1].addr = (d_canvas_w * d_canvas_h);
                ge2d_config_ex.dst_planes[1].shared_fd = 0;
                ge2d_config_ex.dst_planes[1].w = d_canvas_w;
                ge2d_config_ex.dst_planes[1].h = d_canvas_h;
            } else if (output_buffer_info->plane_number == 2) {
                ge2d_config_ex.dst_planes[0].addr = output_buffer_info->offset[0];
                ge2d_config_ex.dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                ge2d_config_ex.dst_planes[0].w = d_canvas_w;
                ge2d_config_ex.dst_planes[0].h = d_canvas_h;
                ge2d_config_ex.dst_planes[1].addr = output_buffer_info->offset[1];
                ge2d_config_ex.dst_planes[1].shared_fd = output_buffer_info->shared_fd[1];
                ge2d_config_ex.dst_planes[1].w = d_canvas_w;
                ge2d_config_ex.dst_planes[1].h = d_canvas_h;
            }
        } else {
            E_GE2D("format is not match, should config dst_planes correct.\n");
            return ge2d_fail;
        }
    }
    ge2d_config_ex.alu_const_color = pge2dinfo->const_color;
    ge2d_config_ex.src1_gb_alpha = input_buffer_info->plane_alpha & 0xff;
    ge2d_config_ex.src1_gb_alpha_en = 1;
    if (pge2dinfo->cap_attr & SRC2_ALPHA) {
        ge2d_config_ex.src2_gb_alpha = input2_buffer_info->plane_alpha & 0xff;
        ge2d_config_ex.src2_gb_alpha_en = 1;
    }
    D_GE2D("blend:src1_cmult_asel=%d,src2_cmult_ase2=%d,gl_alpha=%x,src1_gb_alpha_en=%d, src2_gb_alpha=%x, src2_gb_alpha_en=%d, src2_cmult_ad=%d\n",
        ge2d_config_ex.src1_cmult_asel,
        ge2d_config_ex.src2_cmult_asel,
        ge2d_config_ex.src1_gb_alpha,
        ge2d_config_ex.src1_gb_alpha_en,
        ge2d_config_ex.src2_gb_alpha,
         ge2d_config_ex.src2_gb_alpha_en,
        ge2d_config_ex.src2_cmult_ad);
    if (pge2dinfo->cap_attr == -1)
        ret = ioctl(fd, GE2D_CONFIG_EX_ION_EN, &ge2d_config_ex);
    else
        ret = ioctl(fd, GE2D_CONFIG_EX_ION, &ge2d_config_ex);
    if (ret < 0) {
        E_GE2D("ge2d config ex ion failed. \n");
        return ge2d_fail;
    }
    return ge2d_success;
}

static int ge2d_fillrectangle_config_ex(int fd,aml_ge2d_info_t *pge2dinfo)
{
    int ret = -1;
    struct config_ge2d_para_ex_s ge2d_config_para_ex;
    struct config_para_ex_memtype_s *ge2d_config_mem_ex;
    struct config_para_ex_ion_s *ge2d_config_ex;
    int src_format = 0xffffffff,dst_format = 0xffffffff;
    int s_canvas_w = 0;
    int s_canvas_h = 0;
    int d_canvas_w = 0;
    int d_canvas_h = 0;
    int bpp = 0;
    int is_one_plane = -1;
    buffer_info_t* input_buffer_info = &(pge2dinfo->src_info[0]);
    buffer_info_t* output_buffer_info = &(pge2dinfo->dst_info);
    unsigned int src_endian = input_buffer_info->endain ? GE2D_BIG_ENDIAN : GE2D_LITTLE_ENDIAN;
    unsigned int dst_endian = output_buffer_info->endain ? GE2D_BIG_ENDIAN : GE2D_LITTLE_ENDIAN;

    if (output_buffer_info->plane_number < 1 ||
        output_buffer_info->plane_number > GE2D_MAX_PLANE)
        output_buffer_info->plane_number = 1;
    memset(&ge2d_config_para_ex, 0, sizeof(struct config_ge2d_para_ex_s));
    ge2d_config_mem_ex = &(ge2d_config_para_ex.para_config_memtype);
    ge2d_config_mem_ex->ge2d_magic = sizeof(struct config_para_ex_memtype_s);
    ge2d_config_mem_ex->src1_mem_alloc_type = AML_GE2D_MEM_INVALID;
    ge2d_config_mem_ex->src2_mem_alloc_type = AML_GE2D_MEM_INVALID;
    ge2d_config_mem_ex->dst_mem_alloc_type = output_buffer_info->mem_alloc_type;
    memcpy(&ge2d_config_mem_ex->matrix_custom, &pge2dinfo->matrix_custom,
           sizeof(struct ge2d_matrix_s));
    memcpy(&ge2d_config_mem_ex->stride_custom, &pge2dinfo->stride_custom,
           sizeof(struct ge2d_stride_s));

    ge2d_config_ex = &(ge2d_config_mem_ex->_ge2d_config_ex);

    if ((CANVAS_ALLOC == output_buffer_info->memtype)) {
        is_one_plane = pixel_to_ge2d_format(&output_buffer_info->format,&dst_format,&bpp);
        dst_format |= dst_endian;
        if ((int)0xffffffff == dst_format) {
            E_GE2D("can't get proper ge2d format\n" );
            return ge2d_fail;
        }
        ge2d_set_canvas(bpp,output_buffer_info->canvas_w,output_buffer_info->canvas_h,&d_canvas_w,&d_canvas_h);

        pixel_to_ge2d_format(&input_buffer_info->format,&src_format,&bpp);
        src_format |= src_endian;
        ge2d_set_canvas(bpp,input_buffer_info->canvas_w,input_buffer_info->canvas_h,&s_canvas_w,&s_canvas_h);
    }else {
        is_one_plane = pixel_to_ge2d_format(&output_buffer_info->format,&dst_format,&bpp);
        dst_format |= dst_endian;
        ge2d_set_canvas(bpp,output_buffer_info->canvas_w,output_buffer_info->canvas_h,&d_canvas_w,&d_canvas_h);

        pixel_to_ge2d_format(&input_buffer_info->format,&src_format,&bpp);
        src_format |= src_endian;
        ge2d_set_canvas(bpp,input_buffer_info->canvas_w,input_buffer_info->canvas_h,&s_canvas_w,&s_canvas_h);
    }
    D_GE2D("ge2d_fillrectangle_config_ex,memtype=%x,src_format=%x,s_canvas_w=%d,s_canvas_h=%d,rotation=%d\n",
        input_buffer_info->memtype,src_format,s_canvas_w,s_canvas_h,input_buffer_info->rotation);

    D_GE2D("ge2d_fillrectangle_config_ex,memtype=%x,dst_format=%x,d_canvas_w=%d,d_canvas_h=%d,rotation=%d\n",
        output_buffer_info->memtype,dst_format,d_canvas_w,d_canvas_h,output_buffer_info->rotation);

    ge2d_config_ex->src_para.mem_type = CANVAS_TYPE_INVALID;
    ge2d_config_ex->src_para.format = src_format;
    ge2d_config_ex->src_para.left = 0;
    ge2d_config_ex->src_para.top = 0;
    ge2d_config_ex->src_para.width = s_canvas_w;
    ge2d_config_ex->src_para.height = s_canvas_h;

    ge2d_config_ex->dst_para.mem_type = output_buffer_info->memtype;
    ge2d_config_ex->dst_para.format = dst_format;
    ge2d_config_ex->dst_para.left = 0;
    ge2d_config_ex->dst_para.top = 0;
    ge2d_config_ex->dst_para.width = d_canvas_w;
    ge2d_config_ex->dst_para.height = d_canvas_h;

    ge2d_config_ex->src2_para.mem_type = CANVAS_TYPE_INVALID;

    switch (pge2dinfo->dst_info.rotation) {
        case GE2D_ROTATION_0:
            break;
        case GE2D_ROTATION_90:
            ge2d_config_ex->dst_xy_swap = 1;
            ge2d_config_ex->dst_para.x_rev = 1;
            break;
        case GE2D_ROTATION_180:
            ge2d_config_ex->dst_para.x_rev = 1;
            ge2d_config_ex->dst_para.y_rev = 1;
            break;
        case GE2D_ROTATION_270:
            ge2d_config_ex->dst_xy_swap = 1;
            ge2d_config_ex->dst_para.y_rev = 1;
            break;
        case GE2D_MIRROR_X:
            ge2d_config_ex->src_para.x_rev = 1;
            break;
        case GE2D_MIRROR_Y:
            ge2d_config_ex->src_para.y_rev = 1;
            break;
        default:
            break;
    }

    if (CANVAS_ALLOC == output_buffer_info->memtype) {
        if (is_one_plane == 1) {
            ge2d_config_ex->dst_planes[0].addr = output_buffer_info->offset[0];
            ge2d_config_ex->dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
            ge2d_config_ex->dst_planes[0].w = d_canvas_w;
            ge2d_config_ex->dst_planes[0].h = d_canvas_h;
        } else if ((output_buffer_info->format == PIXEL_FORMAT_YCrCb_420_SP) ||
            (output_buffer_info->format == PIXEL_FORMAT_YCbCr_420_SP_NV12)) {
            if (output_buffer_info->plane_number == 1) {
                ge2d_config_ex->dst_planes[0].addr = output_buffer_info->offset[0];
                ge2d_config_ex->dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                ge2d_config_ex->dst_planes[0].w = d_canvas_w;
                ge2d_config_ex->dst_planes[0].h = d_canvas_h;
                ge2d_config_ex->dst_planes[1].addr = (d_canvas_w * d_canvas_h);
                ge2d_config_ex->dst_planes[1].shared_fd = 0;
                ge2d_config_ex->dst_planes[1].w = d_canvas_w;
                ge2d_config_ex->dst_planes[1].h = d_canvas_h/2;
            } else if (output_buffer_info->plane_number == 2) {
                ge2d_config_ex->dst_planes[0].addr = output_buffer_info->offset[0];
                ge2d_config_ex->dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                ge2d_config_ex->dst_planes[0].w = d_canvas_w;
                ge2d_config_ex->dst_planes[0].h = d_canvas_h;
                ge2d_config_ex->dst_planes[1].addr = output_buffer_info->offset[1];
                ge2d_config_ex->dst_planes[1].shared_fd = output_buffer_info->shared_fd[1];
                ge2d_config_ex->dst_planes[1].w = d_canvas_w;
                ge2d_config_ex->dst_planes[1].h = d_canvas_h/2;
            }
        }  else if (output_buffer_info->format == PIXEL_FORMAT_Y8 ||
                    output_buffer_info->format == PIXEL_FORMAT_CLUT8 ||
                    output_buffer_info->format == PIXEL_FORMAT_ALPHA8) {
            ge2d_config_ex->dst_planes[0].addr = output_buffer_info->offset[0];
            ge2d_config_ex->dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
            ge2d_config_ex->dst_planes[0].w = d_canvas_w;
            ge2d_config_ex->dst_planes[0].h = d_canvas_h;
        } else if (output_buffer_info->format == PIXEL_FORMAT_YV12 ||
                   output_buffer_info->format == PIXEL_FORMAT_YU12) {
            switch (pge2dinfo->dst_op_cnt) {
            case 0:
                ge2d_config_ex->dst_para.format = GE2D_FORMAT_S8_Y | dst_endian;
                ge2d_config_ex->dst_planes[0].addr = output_buffer_info->offset[0];
                ge2d_config_ex->dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                ge2d_config_ex->dst_planes[0].w = d_canvas_w;
                ge2d_config_ex->dst_planes[0].h = d_canvas_h;
                ge2d_config_ex->dst_para.width = d_canvas_w;
                ge2d_config_ex->dst_para.height = d_canvas_h;
                break;
            case 1:
                if (output_buffer_info->format == PIXEL_FORMAT_YV12)
                    ge2d_config_ex->dst_para.format = GE2D_FORMAT_S8_CR | dst_endian;
                else
                    ge2d_config_ex->dst_para.format = GE2D_FORMAT_S8_CB | dst_endian;
                ge2d_config_ex->dst_planes[0].w = d_canvas_w/2;
                ge2d_config_ex->dst_planes[0].h = d_canvas_h/2;
                ge2d_config_ex->dst_para.width = d_canvas_w/2;
                ge2d_config_ex->dst_para.height = d_canvas_h/2;
                if (output_buffer_info->plane_number == 1) {
                    ge2d_config_ex->dst_planes[0].addr = YV12_Y_ALIGNED(d_canvas_w) *
                        d_canvas_h;
                    ge2d_config_ex->dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                } else if (output_buffer_info->plane_number == 3) {
                    ge2d_config_ex->dst_planes[0].addr = output_buffer_info->offset[1];
                    ge2d_config_ex->dst_planes[0].shared_fd = output_buffer_info->shared_fd[1];
                }
                break;
            case 2:
                if (output_buffer_info->format == PIXEL_FORMAT_YV12)
                    ge2d_config_ex->dst_para.format = GE2D_FORMAT_S8_CB | dst_endian;
                else
                    ge2d_config_ex->dst_para.format = GE2D_FORMAT_S8_CR | dst_endian;
                ge2d_config_ex->dst_planes[0].w = d_canvas_w/2;
                ge2d_config_ex->dst_planes[0].h = d_canvas_h/2;
                ge2d_config_ex->dst_para.width = d_canvas_w/2;
                ge2d_config_ex->dst_para.height = d_canvas_h/2;
                if (output_buffer_info->plane_number == 1) {
                    ge2d_config_ex->dst_planes[0].addr = YV12_Y_ALIGNED(d_canvas_w) *
                        d_canvas_h + CANVAS_ALIGNED(d_canvas_w/2) * d_canvas_h/2;
                    ge2d_config_ex->dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                } else if (output_buffer_info->plane_number == 3) {
                    ge2d_config_ex->dst_planes[0].addr = output_buffer_info->offset[2];
                    ge2d_config_ex->dst_planes[0].shared_fd = output_buffer_info->shared_fd[2];
                }
                break;
            default:
                break;
            }
        } else if (output_buffer_info->format == PIXEL_FORMAT_YCbCr_422_SP) {
            if (output_buffer_info->plane_number == 1) {
                ge2d_config_ex->dst_planes[0].addr = output_buffer_info->offset[0];
                ge2d_config_ex->dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                ge2d_config_ex->dst_planes[0].w = d_canvas_w;
                ge2d_config_ex->dst_planes[0].h = d_canvas_h;
                ge2d_config_ex->dst_planes[1].addr = (d_canvas_w * d_canvas_h);
                ge2d_config_ex->dst_planes[1].shared_fd = 0;
                ge2d_config_ex->dst_planes[1].w = d_canvas_w;
                ge2d_config_ex->dst_planes[1].h = d_canvas_h;
            } else {
                ge2d_config_ex->dst_planes[0].addr = output_buffer_info->offset[0];
                ge2d_config_ex->dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                ge2d_config_ex->dst_planes[0].w = d_canvas_w;
                ge2d_config_ex->dst_planes[0].h = d_canvas_h;
                ge2d_config_ex->dst_planes[1].addr = output_buffer_info->offset[1];
                ge2d_config_ex->dst_planes[1].shared_fd = output_buffer_info->shared_fd[1];
                ge2d_config_ex->dst_planes[1].w = d_canvas_w;
                ge2d_config_ex->dst_planes[1].h = d_canvas_h;
            }
        } else {
            E_GE2D("format is not match, should config dst_planes correct.\n");
            return ge2d_fail;
        }
    }
    ge2d_config_ex->alu_const_color = pge2dinfo->const_color;
    ge2d_config_ex->src1_gb_alpha = input_buffer_info->plane_alpha & 0xff;

    ret = ioctl(fd, GE2D_CONFIG_EX_MEM, &ge2d_config_para_ex);
    if (ret < 0) {
        E_GE2D("ge2d config ex dma failed.\n");
        return ge2d_fail;
    }
    return ge2d_success;
}

static int ge2d_blit_config_ex(int fd,aml_ge2d_info_t *pge2dinfo)
{
    int ret = -1;
    struct config_ge2d_para_ex_s ge2d_config_para_ex;
    struct config_para_ex_memtype_s *ge2d_config_mem_ex;
    struct config_para_ex_ion_s *ge2d_config_ex;
    int src_format = 0xffffffff,dst_format = 0xffffffff;
    int s_canvas_w = 0;
    int s_canvas_h = 0;
    int d_canvas_w = 0;
    int d_canvas_h = 0;
    int bpp = 0;
    int is_one_plane_input = -1;
    int is_one_plane_output = -1;
    buffer_info_t* input_buffer_info = &(pge2dinfo->src_info[0]);
    buffer_info_t* output_buffer_info = &(pge2dinfo->dst_info);
    unsigned int src_endian = input_buffer_info->endain ? GE2D_BIG_ENDIAN : GE2D_LITTLE_ENDIAN;
    unsigned int dst_endian = output_buffer_info->endain ? GE2D_BIG_ENDIAN : GE2D_LITTLE_ENDIAN;

    if (input_buffer_info->plane_number < 1 ||
        input_buffer_info->plane_number > GE2D_MAX_PLANE)
        input_buffer_info->plane_number = 1;
    if (output_buffer_info->plane_number < 1 ||
        output_buffer_info->plane_number > GE2D_MAX_PLANE)
        output_buffer_info->plane_number = 1;
    memset(&ge2d_config_para_ex, 0, sizeof(struct config_ge2d_para_ex_s));
    ge2d_config_mem_ex = &(ge2d_config_para_ex.para_config_memtype);
    ge2d_config_mem_ex->ge2d_magic = sizeof(struct config_para_ex_memtype_s);
    ge2d_config_mem_ex->src1_mem_alloc_type = input_buffer_info->mem_alloc_type;
    ge2d_config_mem_ex->src2_mem_alloc_type = AML_GE2D_MEM_INVALID;
    ge2d_config_mem_ex->dst_mem_alloc_type = output_buffer_info->mem_alloc_type;
    memcpy(&ge2d_config_mem_ex->matrix_custom, &pge2dinfo->matrix_custom,
           sizeof(struct ge2d_matrix_s));
    memcpy(&ge2d_config_mem_ex->stride_custom, &pge2dinfo->stride_custom,
           sizeof(struct ge2d_stride_s));

    ge2d_config_ex = &(ge2d_config_mem_ex->_ge2d_config_ex);

    if ((CANVAS_ALLOC == input_buffer_info->memtype)) {
        is_one_plane_input = pixel_to_ge2d_format(&input_buffer_info->format,&src_format,&bpp);
        src_format |= src_endian;
        if ((int)0xffffffff == src_format) {
            E_GE2D("can't get proper ge2d format\n" );
            return ge2d_fail;
        }
        ge2d_set_canvas(bpp,input_buffer_info->canvas_w,input_buffer_info->canvas_h,&s_canvas_w,&s_canvas_h);
    }

    if ((CANVAS_ALLOC == output_buffer_info->memtype)) {
        is_one_plane_output = pixel_to_ge2d_format(&output_buffer_info->format,&dst_format,&bpp);
        dst_format |= dst_endian;
        if ((int)0xffffffff == dst_format) {
            E_GE2D("can't get proper ge2d format\n" );
            return ge2d_fail;
        }
        ge2d_set_canvas(bpp,output_buffer_info->canvas_w,output_buffer_info->canvas_h,&d_canvas_w,&d_canvas_h);
    }
    D_GE2D("ge2d_blit_config_ex,memtype=%x,src_format=%x,s_canvas_w=%d,s_canvas_h=%d,rotation=%d\n",
        input_buffer_info->memtype,src_format,s_canvas_w,s_canvas_h,input_buffer_info->rotation);

    D_GE2D("ge2d_blit_config_ex,memtype=%x,dst_format=%x,d_canvas_w=%d,d_canvas_h=%d,rotation=%d\n",
        output_buffer_info->memtype,dst_format,d_canvas_w,d_canvas_h,output_buffer_info->rotation);

    ge2d_config_ex->src_para.mem_type = input_buffer_info->memtype;
    ge2d_config_ex->src_para.format = src_format;
    ge2d_config_ex->src_para.left = input_buffer_info->rect.x;
    ge2d_config_ex->src_para.top = input_buffer_info->rect.y;
    ge2d_config_ex->src_para.width = input_buffer_info->rect.w;
    ge2d_config_ex->src_para.height = input_buffer_info->rect.h;

    ge2d_config_ex->src2_para.mem_type = CANVAS_TYPE_INVALID;

    ge2d_config_ex->dst_para.mem_type = output_buffer_info->memtype;
    ge2d_config_ex->dst_para.format = dst_format;
    ge2d_config_ex->dst_para.left = 0;
    ge2d_config_ex->dst_para.top = 0;
    ge2d_config_ex->dst_para.width = d_canvas_w;
    ge2d_config_ex->dst_para.height = d_canvas_h;
    if (input_buffer_info->layer_mode == LAYER_MODE_PREMULTIPLIED)
        ge2d_config_ex->src1_cmult_asel = 2;
    else if (input_buffer_info->layer_mode == LAYER_MODE_COVERAGE)
        ge2d_config_ex->src1_cmult_asel = 0;
    else if (input_buffer_info->layer_mode == LAYER_MODE_NON)
        ge2d_config_ex->src1_cmult_asel = 0;

    switch (pge2dinfo->src_info[0].rotation) {
        case GE2D_ROTATION_0:
            break;
        case GE2D_ROTATION_90:
            ge2d_config_ex->dst_xy_swap = 1;
            ge2d_config_ex->src_para.y_rev = 1;
            break;
        case GE2D_ROTATION_180:
            ge2d_config_ex->src_para.x_rev = 1;
            ge2d_config_ex->src_para.y_rev = 1;
            break;
        case GE2D_ROTATION_270:
            ge2d_config_ex->dst_xy_swap = 1;
            ge2d_config_ex->src_para.x_rev = 1;
            break;
        default:
            break;
    }

    switch (pge2dinfo->dst_info.rotation) {
        case GE2D_ROTATION_0:
            break;
        case GE2D_ROTATION_90:
            ge2d_config_ex->dst_xy_swap = 1;
            ge2d_config_ex->dst_para.x_rev = 1;
            break;
        case GE2D_ROTATION_180:
            ge2d_config_ex->dst_para.x_rev = 1;
            ge2d_config_ex->dst_para.y_rev = 1;
            break;
        case GE2D_ROTATION_270:
            ge2d_config_ex->dst_xy_swap = 1;
            ge2d_config_ex->dst_para.y_rev = 1;
            break;
        case GE2D_MIRROR_X:
            ge2d_config_ex->src_para.x_rev = 1;
            break;
        case GE2D_MIRROR_Y:
            ge2d_config_ex->src_para.y_rev = 1;
            break;
        default:
            break;
    }

    if (CANVAS_ALLOC == input_buffer_info->memtype) {
        if (is_one_plane_input) {
            ge2d_config_ex->src_planes[0].addr = input_buffer_info->offset[0];
            ge2d_config_ex->src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
            ge2d_config_ex->src_planes[0].w = s_canvas_w;
            ge2d_config_ex->src_planes[0].h = s_canvas_h;
        } else if ((input_buffer_info->format == PIXEL_FORMAT_YCrCb_420_SP) ||
            (input_buffer_info->format == PIXEL_FORMAT_YCbCr_420_SP_NV12)) {
            if (input_buffer_info->plane_number == 1) {
                ge2d_config_ex->src_planes[0].addr = input_buffer_info->offset[0];
                ge2d_config_ex->src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
                ge2d_config_ex->src_planes[0].w = s_canvas_w;
                ge2d_config_ex->src_planes[0].h = s_canvas_h;
                ge2d_config_ex->src_planes[1].addr = (s_canvas_w * s_canvas_h);
                ge2d_config_ex->src_planes[1].shared_fd = 0;
                ge2d_config_ex->src_planes[1].w = s_canvas_w;
                ge2d_config_ex->src_planes[1].h = s_canvas_h/2;
            } else if (input_buffer_info->plane_number == 2) {
                ge2d_config_ex->src_planes[0].addr = input_buffer_info->offset[0];
                ge2d_config_ex->src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
                ge2d_config_ex->src_planes[0].w = s_canvas_w;
                ge2d_config_ex->src_planes[0].h = s_canvas_h;
                ge2d_config_ex->src_planes[1].addr = input_buffer_info->offset[1];
                ge2d_config_ex->src_planes[1].shared_fd = input_buffer_info->shared_fd[1];
                ge2d_config_ex->src_planes[1].w = s_canvas_w;
                ge2d_config_ex->src_planes[1].h = s_canvas_h/2;
            }
        } else if (input_buffer_info->format == PIXEL_FORMAT_Y8 ||
                   input_buffer_info->format == PIXEL_FORMAT_CLUT8 ||
                   input_buffer_info->format == PIXEL_FORMAT_ALPHA8) {
            ge2d_config_ex->src_planes[0].addr = input_buffer_info->offset[0];
            ge2d_config_ex->src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
            ge2d_config_ex->src_planes[0].w = s_canvas_w;
            ge2d_config_ex->src_planes[0].h = s_canvas_h;
        } else if (input_buffer_info->format == PIXEL_FORMAT_YV12) {
            if (input_buffer_info->plane_number == 1) {
                ge2d_config_ex->src_planes[0].addr = input_buffer_info->offset[0];
                ge2d_config_ex->src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
                ge2d_config_ex->src_planes[0].w = s_canvas_w;
                ge2d_config_ex->src_planes[0].h = s_canvas_h;
                /* android is ycrcb,kernel is ycbcr,swap the addr */
                ge2d_config_ex->src_planes[1].addr = YV12_Y_ALIGNED(s_canvas_w) *
                    s_canvas_h + CANVAS_ALIGNED(s_canvas_w/2) * s_canvas_h/2;
                ge2d_config_ex->src_planes[1].shared_fd = 0;
                ge2d_config_ex->src_planes[1].w = CANVAS_ALIGNED(s_canvas_w/2);
                ge2d_config_ex->src_planes[1].h = s_canvas_h/2;
                ge2d_config_ex->src_planes[2].addr = YV12_Y_ALIGNED(s_canvas_w) * s_canvas_h;
                ge2d_config_ex->src_planes[2].shared_fd = 0;
                ge2d_config_ex->src_planes[2].w = CANVAS_ALIGNED(s_canvas_w/2);
                ge2d_config_ex->src_planes[2].h = s_canvas_h/2;
                if ( !(d_canvas_w % 64) || !(d_canvas_w % 32))
                    D_GE2D("yv12 src canvas_w not alligned\n ");
            } else if (input_buffer_info->plane_number == 3) {
                ge2d_config_ex->src_planes[0].addr = input_buffer_info->offset[0];
                ge2d_config_ex->src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
                ge2d_config_ex->src_planes[0].w = s_canvas_w;
                ge2d_config_ex->src_planes[0].h = s_canvas_h;
                /* android is ycrcb,kernel is ycbcr,swap the addr */
                ge2d_config_ex->src_planes[1].addr = input_buffer_info->offset[1];
                ge2d_config_ex->src_planes[1].shared_fd = input_buffer_info->shared_fd[2];
                ge2d_config_ex->src_planes[1].w = CANVAS_ALIGNED(s_canvas_w/2);
                ge2d_config_ex->src_planes[1].h = s_canvas_h/2;
                ge2d_config_ex->src_planes[2].addr = input_buffer_info->offset[2];
                ge2d_config_ex->src_planes[2].shared_fd = input_buffer_info->shared_fd[1];
                ge2d_config_ex->src_planes[2].w = CANVAS_ALIGNED(s_canvas_w/2);
                ge2d_config_ex->src_planes[2].h = s_canvas_h/2;
                if ( !(d_canvas_w % 64) || !(d_canvas_w % 32))
                    D_GE2D("yv12 src canvas_w not alligned\n ");
            }
        } else if (input_buffer_info->format == PIXEL_FORMAT_YU12) {
            if (input_buffer_info->plane_number == 1) {
                ge2d_config_ex->src_planes[0].addr = input_buffer_info->offset[0];
                ge2d_config_ex->src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
                ge2d_config_ex->src_planes[0].w = s_canvas_w;
                ge2d_config_ex->src_planes[0].h = s_canvas_h;
                ge2d_config_ex->src_planes[1].addr = YV12_Y_ALIGNED(s_canvas_w) * s_canvas_h;
                ge2d_config_ex->src_planes[1].shared_fd = 0;
                ge2d_config_ex->src_planes[1].w = CANVAS_ALIGNED(s_canvas_w/2);
                ge2d_config_ex->src_planes[1].h = s_canvas_h/2;
                ge2d_config_ex->src_planes[2].addr = YV12_Y_ALIGNED(s_canvas_w) *
                    s_canvas_h + CANVAS_ALIGNED(s_canvas_w/2) * s_canvas_h/2;
                ge2d_config_ex->src_planes[2].shared_fd = 0;
                ge2d_config_ex->src_planes[2].w = CANVAS_ALIGNED(s_canvas_w/2);
                ge2d_config_ex->src_planes[2].h = s_canvas_h/2;
                if ( !(d_canvas_w % 64) || !(d_canvas_w % 32))
                    D_GE2D("yu12 src canvas_w not alligned\n ");
            } else if (input_buffer_info->plane_number == 3) {
                ge2d_config_ex->src_planes[0].addr = input_buffer_info->offset[0];
                ge2d_config_ex->src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
                ge2d_config_ex->src_planes[0].w = s_canvas_w;
                ge2d_config_ex->src_planes[0].h = s_canvas_h;
                ge2d_config_ex->src_planes[1].addr = input_buffer_info->offset[1];
                ge2d_config_ex->src_planes[1].shared_fd = input_buffer_info->shared_fd[1];
                ge2d_config_ex->src_planes[1].w = CANVAS_ALIGNED(s_canvas_w/2);
                ge2d_config_ex->src_planes[1].h = s_canvas_h/2;
                ge2d_config_ex->src_planes[2].addr = input_buffer_info->offset[2];
                ge2d_config_ex->src_planes[2].shared_fd = input_buffer_info->shared_fd[2];
                ge2d_config_ex->src_planes[2].w = CANVAS_ALIGNED(s_canvas_w/2);
                ge2d_config_ex->src_planes[2].h = s_canvas_h/2;
                if ( !(d_canvas_w % 64) || !(d_canvas_w % 32))
                    D_GE2D("yu12 src canvas_w not alligned\n ");
            }
        } else if (input_buffer_info->format == PIXEL_FORMAT_YCbCr_422_SP) {
            if (input_buffer_info->plane_number == 1) {
                ge2d_config_ex->src_planes[0].addr = input_buffer_info->offset[0];
                ge2d_config_ex->src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
                ge2d_config_ex->src_planes[0].w = s_canvas_w;
                ge2d_config_ex->src_planes[0].h = s_canvas_h;
                ge2d_config_ex->src_planes[1].addr = (s_canvas_w * s_canvas_h);
                ge2d_config_ex->src_planes[1].shared_fd = 0;
                ge2d_config_ex->src_planes[1].w = s_canvas_w;
                ge2d_config_ex->src_planes[1].h = s_canvas_h;
            } else if (input_buffer_info->plane_number == 2) {
                ge2d_config_ex->src_planes[0].addr = input_buffer_info->offset[0];
                ge2d_config_ex->src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
                ge2d_config_ex->src_planes[0].w = s_canvas_w;
                ge2d_config_ex->src_planes[0].h = s_canvas_h;
                ge2d_config_ex->src_planes[1].addr = input_buffer_info->offset[1];
                ge2d_config_ex->src_planes[1].shared_fd = input_buffer_info->shared_fd[1];
                ge2d_config_ex->src_planes[1].w = s_canvas_w;
                ge2d_config_ex->src_planes[1].h = s_canvas_h;
            }
        } else {
            E_GE2D("format is not match, should config src_planes correct.\n");
            return ge2d_fail;
        }
    }


    if (CANVAS_ALLOC == output_buffer_info->memtype) {
        switch (output_buffer_info->format) {
        case PIXEL_FORMAT_RGBA_8888:
        case PIXEL_FORMAT_RGBX_8888:
        case PIXEL_FORMAT_ARGB_8888:
        case PIXEL_FORMAT_ABGR_8888:
        case PIXEL_FORMAT_RGB_888:
        case PIXEL_FORMAT_BGR_888:
        case PIXEL_FORMAT_RGB_565:
        case PIXEL_FORMAT_ARGB_4444:
        case PIXEL_FORMAT_RGBA_4444:
        case PIXEL_FORMAT_ARGB_1555:
        case PIXEL_FORMAT_YCbCr_422_UYVY:
        case PIXEL_FORMAT_BGRA_8888:
        case PIXEL_FORMAT_Y8:
        case PIXEL_FORMAT_CLUT8:
        case PIXEL_FORMAT_ALPHA8:
            if (pge2dinfo->dst_op_cnt == 0) {
                ge2d_config_ex->dst_planes[0].addr = output_buffer_info->offset[0];
                ge2d_config_ex->dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                ge2d_config_ex->dst_planes[0].w = d_canvas_w;
                ge2d_config_ex->dst_planes[0].h = d_canvas_h;
            }
            break;
        case PIXEL_FORMAT_YV12:
        case PIXEL_FORMAT_YU12:
            switch (pge2dinfo->dst_op_cnt) {
            case 0:
                ge2d_config_ex->dst_para.format = GE2D_FORMAT_S8_Y | dst_endian;
                ge2d_config_ex->dst_planes[0].addr = output_buffer_info->offset[0];
                ge2d_config_ex->dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                ge2d_config_ex->dst_planes[0].w = d_canvas_w;
                ge2d_config_ex->dst_planes[0].h = d_canvas_h;
                ge2d_config_ex->dst_para.width = d_canvas_w;
                ge2d_config_ex->dst_para.height = d_canvas_h;
                break;
            case 1:
                if (output_buffer_info->format == PIXEL_FORMAT_YV12)
                    ge2d_config_ex->dst_para.format = GE2D_FORMAT_S8_CR | dst_endian;
                else
                    ge2d_config_ex->dst_para.format = GE2D_FORMAT_S8_CB | dst_endian;
                ge2d_config_ex->dst_planes[0].w = d_canvas_w/2;
                ge2d_config_ex->dst_planes[0].h = d_canvas_h/2;
                ge2d_config_ex->dst_para.width = d_canvas_w/2;
                ge2d_config_ex->dst_para.height = d_canvas_h/2;
                if (output_buffer_info->plane_number == 1) {
                    ge2d_config_ex->dst_planes[0].addr = YV12_Y_ALIGNED(d_canvas_w) *
                        d_canvas_h;
                    ge2d_config_ex->dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                } else if (output_buffer_info->plane_number == 3) {
                    ge2d_config_ex->dst_planes[0].addr = output_buffer_info->offset[1];
                    ge2d_config_ex->dst_planes[0].shared_fd = output_buffer_info->shared_fd[1];
                }
                break;
            case 2:
                if (output_buffer_info->format == PIXEL_FORMAT_YV12)
                    ge2d_config_ex->dst_para.format = GE2D_FORMAT_S8_CB | dst_endian;
                else
                    ge2d_config_ex->dst_para.format = GE2D_FORMAT_S8_CR | dst_endian;
                ge2d_config_ex->dst_planes[0].w = d_canvas_w/2;
                ge2d_config_ex->dst_planes[0].h = d_canvas_h/2;
                ge2d_config_ex->dst_para.width = d_canvas_w/2;
                ge2d_config_ex->dst_para.height = d_canvas_h/2;
                if (output_buffer_info->plane_number == 1) {
                    ge2d_config_ex->dst_planes[0].addr = YV12_Y_ALIGNED(d_canvas_w) *
                        d_canvas_h + CANVAS_ALIGNED(d_canvas_w/2) * d_canvas_h/2;
                    ge2d_config_ex->dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                } else if (output_buffer_info->plane_number == 3) {
                    ge2d_config_ex->dst_planes[0].addr = output_buffer_info->offset[2];
                    ge2d_config_ex->dst_planes[0].shared_fd = output_buffer_info->shared_fd[2];
                }
                break;
            default:
                break;
            }
            break;
        case PIXEL_FORMAT_YCbCr_422_SP:
            if (output_buffer_info->plane_number == 1) {
                ge2d_config_ex->dst_planes[0].addr = output_buffer_info->offset[0];
                ge2d_config_ex->dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                ge2d_config_ex->dst_planes[0].w = d_canvas_w;
                ge2d_config_ex->dst_planes[0].h = d_canvas_h;
                ge2d_config_ex->dst_planes[1].addr = (d_canvas_w * d_canvas_h);
                ge2d_config_ex->dst_planes[1].shared_fd = 0;
                ge2d_config_ex->dst_planes[1].w = d_canvas_w;
                ge2d_config_ex->dst_planes[1].h = d_canvas_h;
            } else if (output_buffer_info->plane_number == 2) {
                ge2d_config_ex->dst_planes[0].addr = output_buffer_info->offset[0];
                ge2d_config_ex->dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                ge2d_config_ex->dst_planes[0].w = d_canvas_w;
                ge2d_config_ex->dst_planes[0].h = d_canvas_h;
                ge2d_config_ex->dst_planes[1].addr = output_buffer_info->offset[1];
                ge2d_config_ex->dst_planes[1].shared_fd = output_buffer_info->shared_fd[1];
                ge2d_config_ex->dst_planes[1].w = d_canvas_w;
                ge2d_config_ex->dst_planes[1].h = d_canvas_h;
            }
            break;
        case PIXEL_FORMAT_YCrCb_420_SP:
        case PIXEL_FORMAT_YCbCr_420_SP_NV12:
            if (output_buffer_info->plane_number == 1) {
                ge2d_config_ex->dst_planes[0].addr = output_buffer_info->offset[0];
                ge2d_config_ex->dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                ge2d_config_ex->dst_planes[0].w = d_canvas_w;
                ge2d_config_ex->dst_planes[0].h = d_canvas_h;
                ge2d_config_ex->dst_planes[1].addr = (d_canvas_w * d_canvas_h);
                ge2d_config_ex->dst_planes[1].shared_fd = 0;
                ge2d_config_ex->dst_planes[1].w = d_canvas_w;
                ge2d_config_ex->dst_planes[1].h = d_canvas_h/2;
            } else if (output_buffer_info->plane_number == 2) {
                ge2d_config_ex->dst_planes[0].addr = output_buffer_info->offset[0];
                ge2d_config_ex->dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                ge2d_config_ex->dst_planes[0].w = d_canvas_w;
                ge2d_config_ex->dst_planes[0].h = d_canvas_h;
                ge2d_config_ex->dst_planes[1].addr = output_buffer_info->offset[1];
                ge2d_config_ex->dst_planes[1].shared_fd = output_buffer_info->shared_fd[1];
                ge2d_config_ex->dst_planes[1].w = d_canvas_w;
                ge2d_config_ex->dst_planes[1].h = d_canvas_h/2;
            }
            break;
        }
    }
    ge2d_config_ex->alu_const_color = pge2dinfo->const_color;
    ge2d_config_ex->src1_gb_alpha = input_buffer_info->plane_alpha & 0xff;
    ge2d_config_ex->src1_gb_alpha_en = 1;
    D_GE2D("bilt:src1_cmult_asel=%d,src2_cmult_ase2=%d,gl_alpha=%x,src1_gb_alpha_en=%d\n",
        ge2d_config_ex->src1_cmult_asel,
        ge2d_config_ex->src2_cmult_asel,
        ge2d_config_ex->src1_gb_alpha,
        ge2d_config_ex->src1_gb_alpha_en);

    ret = ioctl(fd, GE2D_CONFIG_EX_MEM, &ge2d_config_para_ex);
    if (ret < 0) {
        E_GE2D("ge2d config ex dma failed. \n");
        return ge2d_fail;
    }
    return ge2d_success;
}

static int ge2d_blend_config_ex(int fd,aml_ge2d_info_t *pge2dinfo)
{
    int ret = -1;
    struct config_ge2d_para_ex_s ge2d_config_para_ex;
    struct config_para_ex_memtype_s *ge2d_config_mem_ex;
    struct config_para_ex_ion_s *ge2d_config_ex;
    int src_format = 0xffffffff,src2_format = 0xffffffff,dst_format = 0xffffffff;
    int s_canvas_w = 0;
    int s_canvas_h = 0;
    int s2_canvas_w = 0;
    int s2_canvas_h = 0;
    int d_canvas_w = 0;
    int d_canvas_h = 0;
    int is_one_plane_input = -1;
    int is_one_plane_input2 = -1;
    int is_one_plane_output = -1;
    int bpp = 0;
    buffer_info_t* input_buffer_info = &pge2dinfo->src_info[0];
    buffer_info_t* input2_buffer_info = &pge2dinfo->src_info[1];
    buffer_info_t* output_buffer_info = &pge2dinfo->dst_info;
    unsigned int src1_endian = input_buffer_info->endain ? GE2D_BIG_ENDIAN : GE2D_LITTLE_ENDIAN;
    unsigned int src2_endian = input2_buffer_info->endain ? GE2D_BIG_ENDIAN : GE2D_LITTLE_ENDIAN;
    unsigned int dst_endian = output_buffer_info->endain ? GE2D_BIG_ENDIAN : GE2D_LITTLE_ENDIAN;

    /* src2 not support nv21/nv12/yv12, swap src1 and src2 */
    if (is_need_swap_src2(input2_buffer_info->format, input2_buffer_info,
                          output_buffer_info, pge2dinfo->cap_attr)) {
        input_buffer_info = &pge2dinfo->src_info[1];
        input2_buffer_info = &pge2dinfo->src_info[0];
        pge2dinfo->b_src_swap = 1;
        D_GE2D("NOTE:src2 not support nv21/nv12, swap src1 and src2!\n");
    }
    else if (!(pge2dinfo->cap_attr & SRC2_ALPHA) &&
        (input_buffer_info->plane_alpha == 0xff) &&
        (input2_buffer_info->plane_alpha != 0xff) &&
        (input_buffer_info->layer_mode != LAYER_MODE_NON) &&
        (input2_buffer_info->layer_mode == LAYER_MODE_PREMULTIPLIED)) {
        /* swap src1 and src2 buffer to process plane alpha*/
        /* TODO: need check the src1 rect and src2 rect */
        input_buffer_info = &pge2dinfo->src_info[1];
        input2_buffer_info = &pge2dinfo->src_info[0];
        //pge2dinfo->gl_alpha &= SRC1_GB_ALPHA_ENABLE;
        //pge2dinfo->gl_alpha |= (input_buffer_info->plane_alpha & 0xff);
        pge2dinfo->b_src_swap = 1;
    }
    else
        pge2dinfo->b_src_swap = 0;
    D_GE2D("b_src_swap=%d\n",pge2dinfo->b_src_swap);
    if (input_buffer_info->plane_number < 1 ||
        input_buffer_info->plane_number > GE2D_MAX_PLANE)
        input_buffer_info->plane_number = 1;
    if (input2_buffer_info->plane_number < 1 ||
        input2_buffer_info->plane_number > GE2D_MAX_PLANE)
        input2_buffer_info->plane_number = 1;
    if (output_buffer_info->plane_number < 1 ||
        output_buffer_info->plane_number > GE2D_MAX_PLANE)
        output_buffer_info->plane_number = 1;
    memset(&ge2d_config_para_ex, 0, sizeof(struct config_ge2d_para_ex_s));
    ge2d_config_mem_ex = &(ge2d_config_para_ex.para_config_memtype);
    ge2d_config_mem_ex->ge2d_magic = sizeof(struct config_para_ex_memtype_s);
    ge2d_config_mem_ex->src1_mem_alloc_type = input_buffer_info->mem_alloc_type;
    ge2d_config_mem_ex->src2_mem_alloc_type = input2_buffer_info->mem_alloc_type;
    ge2d_config_mem_ex->dst_mem_alloc_type = output_buffer_info->mem_alloc_type;
    memcpy(&ge2d_config_mem_ex->matrix_custom, &pge2dinfo->matrix_custom,
           sizeof(struct ge2d_matrix_s));

    if (pge2dinfo->b_src_swap) {
        /* swap customized stride for src1 & src2 */
        memcpy(ge2d_config_mem_ex->stride_custom.src1_stride,
                pge2dinfo->stride_custom.src2_stride,
                sizeof(unsigned int) * GE2D_MAX_PLANE);
        memcpy(ge2d_config_mem_ex->stride_custom.src2_stride,
                pge2dinfo->stride_custom.src1_stride,
                sizeof(unsigned int) * GE2D_MAX_PLANE);
        memcpy(ge2d_config_mem_ex->stride_custom.dst_stride,
                pge2dinfo->stride_custom.dst_stride,
                sizeof(unsigned int) * GE2D_MAX_PLANE);
    } else {
        memcpy(&ge2d_config_mem_ex->stride_custom, &pge2dinfo->stride_custom,
                sizeof(struct ge2d_stride_s));
    }

    ge2d_config_ex = &(ge2d_config_mem_ex->_ge2d_config_ex);

    if (CANVAS_ALLOC == input_buffer_info->memtype) {
        is_one_plane_input = pixel_to_ge2d_format(&input_buffer_info->format,&src_format,&bpp);
        src_format |= src1_endian;
        if ((int)0xffffffff == src_format) {
            E_GE2D("can't get proper ge2d format\n" );
            return ge2d_fail;
        }
        ge2d_set_canvas(bpp,input_buffer_info->canvas_w,input_buffer_info->canvas_h,&s_canvas_w,&s_canvas_h);
    }
    if ((CANVAS_ALLOC == input2_buffer_info->memtype)) {
        is_one_plane_input2 = pixel_to_ge2d_format(&input2_buffer_info->format,&src2_format,&bpp);
        src2_format |= src2_endian;
        if ((int)0xffffffff == src2_format) {
            E_GE2D("can't get proper ge2d format\n" );
            return ge2d_fail;
        }
        ge2d_set_canvas(bpp,input2_buffer_info->canvas_w,input2_buffer_info->canvas_h,&s2_canvas_w,&s2_canvas_h);
    }

    if ((CANVAS_ALLOC == output_buffer_info->memtype)) {
        is_one_plane_output = pixel_to_ge2d_format(&output_buffer_info->format,&dst_format,&bpp);
        dst_format |= dst_endian;
        if ((int)0xffffffff == dst_format) {
            E_GE2D("can't get proper ge2d format\n" );
            return ge2d_fail;
        }
        ge2d_set_canvas(bpp,output_buffer_info->canvas_w,output_buffer_info->canvas_h,&d_canvas_w,&d_canvas_h);

    }
    D_GE2D("ge2d_blit_config_ex,memtype=%x,src_format=%x,s_canvas_w=%d,s_canvas_h=%d,rotation=%d\n",
        input_buffer_info->memtype,src_format,s_canvas_w,s_canvas_h,input_buffer_info->rotation);

    D_GE2D("ge2d_blit_config_ex,memtype=%x,src2_format=%x,s2_canvas_w=%d,s2_canvas_h=%d,rotation=%d\n",
        input2_buffer_info->memtype,src2_format,s2_canvas_w,s2_canvas_h,input2_buffer_info->rotation);

    D_GE2D("ge2d_blit_config_ex,memtype=%x,dst_format=%x,d_canvas_w=%d,d_canvas_h=%d,rotation=%d\n",
        output_buffer_info->memtype,dst_format,d_canvas_w,d_canvas_h,output_buffer_info->rotation);

    ge2d_config_ex->src_para.mem_type = input_buffer_info->memtype;
    ge2d_config_ex->src_para.format = src_format;
    ge2d_config_ex->src_para.left = input_buffer_info->rect.x;
    ge2d_config_ex->src_para.top = input_buffer_info->rect.y;
    ge2d_config_ex->src_para.width = input_buffer_info->rect.w;
    ge2d_config_ex->src_para.height = input_buffer_info->rect.h;
    ge2d_config_ex->src_para.color = input_buffer_info->def_color;
    ge2d_config_ex->src_para.fill_color_en = input_buffer_info->fill_color_en;

    /* top layer factor, use premult/coverage/none
     * depends on top layer mode
     */
    if (pge2dinfo->src_info[0].layer_mode == LAYER_MODE_PREMULTIPLIED)
        pge2dinfo->blend_mode = BLEND_MODE_PREMULTIPLIED;
    else if (pge2dinfo->src_info[0].layer_mode == LAYER_MODE_COVERAGE)
        pge2dinfo->blend_mode = BLEND_MODE_COVERAGE;
    else
        pge2dinfo->blend_mode = BLEND_MODE_NONE;

    if (pge2dinfo->src_info[0].layer_mode == LAYER_MODE_PREMULTIPLIED)
        ge2d_config_ex->src1_cmult_asel = 2;
    else if (pge2dinfo->src_info[0].layer_mode == LAYER_MODE_COVERAGE)
        ge2d_config_ex->src1_cmult_asel = 0;
    else if (pge2dinfo->src_info[0].layer_mode == LAYER_MODE_NON)
        ge2d_config_ex->src1_cmult_asel = 0;

    ge2d_config_ex->src2_para.mem_type = input2_buffer_info->memtype;
    ge2d_config_ex->src2_para.format = src2_format;
    ge2d_config_ex->src2_para.left = input2_buffer_info->rect.x;
    ge2d_config_ex->src2_para.top = input2_buffer_info->rect.y;
    ge2d_config_ex->src2_para.width = input2_buffer_info->rect.w;
    ge2d_config_ex->src2_para.height = input2_buffer_info->rect.h;
    ge2d_config_ex->src2_para.color = input2_buffer_info->def_color;
    ge2d_config_ex->src2_para.fill_color_en = input2_buffer_info->fill_color_en;

    if (pge2dinfo->cap_attr & SRC2_ALPHA) {
        if (pge2dinfo->src_info[1].layer_mode == LAYER_MODE_PREMULTIPLIED)
            ge2d_config_ex->src2_cmult_asel = 2;
        else if (pge2dinfo->src_info[1].layer_mode == LAYER_MODE_COVERAGE)
            ge2d_config_ex->src2_cmult_asel = 1;
        else if (pge2dinfo->src_info[1].layer_mode == LAYER_MODE_NON)
            ge2d_config_ex->src2_cmult_asel= 0;

        if ((pge2dinfo->src_info[0].layer_mode == LAYER_MODE_PREMULTIPLIED) && (pge2dinfo->src_info[1].layer_mode == LAYER_MODE_PREMULTIPLIED))
            ge2d_config_ex->src2_cmult_ad = 0;
        else if ((pge2dinfo->src_info[0].layer_mode == LAYER_MODE_COVERAGE) && (pge2dinfo->src_info[1].layer_mode == LAYER_MODE_PREMULTIPLIED))
            ge2d_config_ex->src2_cmult_ad = 0;
        else if ((pge2dinfo->src_info[0].layer_mode == LAYER_MODE_PREMULTIPLIED) && (pge2dinfo->src_info[1].layer_mode == LAYER_MODE_COVERAGE))
            ge2d_config_ex->src2_cmult_ad = 1;
        else if ((pge2dinfo->src_info[0].layer_mode == LAYER_MODE_COVERAGE) && (pge2dinfo->src_info[1].layer_mode == LAYER_MODE_COVERAGE))
            ge2d_config_ex->src2_cmult_ad = 1;
        else if ((pge2dinfo->src_info[0].layer_mode == LAYER_MODE_COVERAGE) && (pge2dinfo->src_info[1].layer_mode == LAYER_MODE_NON))
            ge2d_config_ex->src2_cmult_ad = 3;
        else
            ge2d_config_ex->src2_cmult_ad = 0;
    } else
        ge2d_config_ex->src2_cmult_asel= 0;
    ge2d_config_ex->dst_para.mem_type = output_buffer_info->memtype;
    ge2d_config_ex->dst_para.format = dst_format;
    ge2d_config_ex->dst_para.left = 0;
    ge2d_config_ex->dst_para.top = 0;
    ge2d_config_ex->dst_para.width = d_canvas_w;
    ge2d_config_ex->dst_para.height = d_canvas_h;
    switch (pge2dinfo->src_info[0].rotation) {
        case GE2D_ROTATION_0:
            break;
        case GE2D_ROTATION_90:
            ge2d_config_ex->dst_xy_swap = 1;
            ge2d_config_ex->src_para.y_rev = 1;
            break;
        case GE2D_ROTATION_180:
            ge2d_config_ex->src_para.x_rev = 1;
            ge2d_config_ex->src_para.y_rev = 1;
            break;
        case GE2D_ROTATION_270:
            ge2d_config_ex->dst_xy_swap = 1;
            ge2d_config_ex->src_para.x_rev = 1;
            break;
        default:
            break;
    }

    switch (pge2dinfo->src_info[1].rotation) {
        case GE2D_ROTATION_0:
            break;
        case GE2D_ROTATION_90:
            ge2d_config_ex->dst_xy_swap = 1;
            ge2d_config_ex->src2_para.y_rev = 1;
            break;
        case GE2D_ROTATION_180:
            ge2d_config_ex->src2_para.x_rev = 1;
            ge2d_config_ex->src2_para.y_rev = 1;
            break;
        case GE2D_ROTATION_270:
            ge2d_config_ex->dst_xy_swap = 1;
            ge2d_config_ex->src2_para.x_rev = 1;
            break;
        default:
            break;
    }

    switch (pge2dinfo->dst_info.rotation) {
         case GE2D_ROTATION_0:
             break;
         case GE2D_ROTATION_90:
             ge2d_config_ex->dst_xy_swap = 1;
             ge2d_config_ex->dst_para.x_rev = 1;
             break;
         case GE2D_ROTATION_180:
             ge2d_config_ex->dst_para.x_rev = 1;
             ge2d_config_ex->dst_para.y_rev = 1;
             break;
         case GE2D_ROTATION_270:
             ge2d_config_ex->dst_xy_swap = 1;
             ge2d_config_ex->dst_para.y_rev = 1;
             break;
        case GE2D_MIRROR_X:
            ge2d_config_ex->src_para.x_rev = 1;
            break;
        case GE2D_MIRROR_Y:
            ge2d_config_ex->src_para.y_rev = 1;
            break;
         default:
             break;
    }

    if (CANVAS_ALLOC == input_buffer_info->memtype) {
        if (is_one_plane_input) {
            ge2d_config_ex->src_planes[0].addr = input_buffer_info->offset[0];
            ge2d_config_ex->src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
            ge2d_config_ex->src_planes[0].w = s_canvas_w;
            ge2d_config_ex->src_planes[0].h = s_canvas_h;
        } else if ((input_buffer_info->format == PIXEL_FORMAT_YCrCb_420_SP) ||
            (input_buffer_info->format ==PIXEL_FORMAT_YCbCr_420_SP_NV12)) {
            if (input_buffer_info->plane_number == 1) {
                ge2d_config_ex->src_planes[0].addr = input_buffer_info->offset[0];
                ge2d_config_ex->src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
                ge2d_config_ex->src_planes[0].w = s_canvas_w;
                ge2d_config_ex->src_planes[0].h = s_canvas_h;
                ge2d_config_ex->src_planes[1].addr = (s_canvas_w * s_canvas_h);
                ge2d_config_ex->src_planes[1].shared_fd = 0;
                ge2d_config_ex->src_planes[1].w = s_canvas_w;
                ge2d_config_ex->src_planes[1].h = s_canvas_h/2;
            } else if (input_buffer_info->plane_number == 2) {
                ge2d_config_ex->src_planes[0].addr = input_buffer_info->offset[0];
                ge2d_config_ex->src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
                ge2d_config_ex->src_planes[0].w = s_canvas_w;
                ge2d_config_ex->src_planes[0].h = s_canvas_h;
                ge2d_config_ex->src_planes[1].addr = input_buffer_info->offset[1];
                ge2d_config_ex->src_planes[1].shared_fd = input_buffer_info->shared_fd[1];
                ge2d_config_ex->src_planes[1].w = s_canvas_w;
                ge2d_config_ex->src_planes[1].h = s_canvas_h/2;
            }
        } else if (input_buffer_info->format == PIXEL_FORMAT_Y8 ||
                   input_buffer_info->format == PIXEL_FORMAT_CLUT8 ||
                   input_buffer_info->format == PIXEL_FORMAT_ALPHA8) {
            ge2d_config_ex->src_planes[0].addr = input_buffer_info->offset[0];
            ge2d_config_ex->src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
            ge2d_config_ex->src_planes[0].w = s_canvas_w;
            ge2d_config_ex->src_planes[0].h = s_canvas_h;
        } else if (input_buffer_info->format == PIXEL_FORMAT_YV12) {
            if (input_buffer_info->plane_number == 1) {
                ge2d_config_ex->src_planes[0].addr = input_buffer_info->offset[0];
                ge2d_config_ex->src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
                ge2d_config_ex->src_planes[0].w = s_canvas_w;
                ge2d_config_ex->src_planes[0].h = s_canvas_h;
                /* android is ycrcb,kernel is ycbcr,swap the addr */
                ge2d_config_ex->src_planes[1].addr = YV12_Y_ALIGNED(s_canvas_w) *
                    s_canvas_h + CANVAS_ALIGNED(s_canvas_w/2 ) * s_canvas_h/2;
                ge2d_config_ex->src_planes[1].shared_fd = 0;
                ge2d_config_ex->src_planes[1].w = CANVAS_ALIGNED(s_canvas_w/2);
                ge2d_config_ex->src_planes[1].h = s_canvas_h/2;
                ge2d_config_ex->src_planes[2].addr = YV12_Y_ALIGNED(s_canvas_w) * s_canvas_h;
                ge2d_config_ex->src_planes[2].shared_fd = 0;
                ge2d_config_ex->src_planes[2].w = CANVAS_ALIGNED(s_canvas_w/2);
                ge2d_config_ex->src_planes[2].h = s_canvas_h/2;
                if ( !(d_canvas_w % 64) || !(d_canvas_w % 32))
                    D_GE2D("yv12 src canvas_w not alligned\n ");
            } else if (input_buffer_info->plane_number == 3) {
                ge2d_config_ex->src_planes[0].addr = input_buffer_info->offset[0];
                ge2d_config_ex->src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
                ge2d_config_ex->src_planes[0].w = s_canvas_w;
                ge2d_config_ex->src_planes[0].h = s_canvas_h;
                /* android is ycrcb,kernel is ycbcr,swap the addr */
                ge2d_config_ex->src_planes[1].addr = input_buffer_info->offset[1];
                ge2d_config_ex->src_planes[1].shared_fd = input_buffer_info->shared_fd[2];
                ge2d_config_ex->src_planes[1].w = CANVAS_ALIGNED(s_canvas_w/2);
                ge2d_config_ex->src_planes[1].h = s_canvas_h/2;
                ge2d_config_ex->src_planes[2].addr = input_buffer_info->offset[2];
                ge2d_config_ex->src_planes[2].shared_fd = input_buffer_info->shared_fd[1];
                ge2d_config_ex->src_planes[2].w = CANVAS_ALIGNED(s_canvas_w/2);
                ge2d_config_ex->src_planes[2].h = s_canvas_h/2;
                if ( !(d_canvas_w % 64) || !(d_canvas_w % 32))
                    D_GE2D("yv12 src canvas_w not alligned\n ");
            }
        } else if (input_buffer_info->format == PIXEL_FORMAT_YU12) {
            if (input_buffer_info->plane_number == 1) {
                ge2d_config_ex->src_planes[0].addr = input_buffer_info->offset[0];
                ge2d_config_ex->src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
                ge2d_config_ex->src_planes[0].w = s_canvas_w;
                ge2d_config_ex->src_planes[0].h = s_canvas_h;
                ge2d_config_ex->src_planes[1].addr = YV12_Y_ALIGNED(s_canvas_w) * s_canvas_h;
                ge2d_config_ex->src_planes[1].shared_fd = 0;
                ge2d_config_ex->src_planes[1].w = CANVAS_ALIGNED(s_canvas_w/2);
                ge2d_config_ex->src_planes[1].h = s_canvas_h/2;
                ge2d_config_ex->src_planes[2].addr = YV12_Y_ALIGNED(s_canvas_w) *
                    s_canvas_h + CANVAS_ALIGNED(s_canvas_w/2 ) * s_canvas_h/2;
                ge2d_config_ex->src_planes[2].shared_fd = 0;
                ge2d_config_ex->src_planes[2].w = CANVAS_ALIGNED(s_canvas_w/2);
                ge2d_config_ex->src_planes[2].h = s_canvas_h/2;
                if ( !(d_canvas_w % 64) || !(d_canvas_w % 32))
                    D_GE2D("yu12 src canvas_w not alligned\n ");
            } else if (input_buffer_info->plane_number == 3) {
                ge2d_config_ex->src_planes[0].addr = input_buffer_info->offset[0];
                ge2d_config_ex->src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
                ge2d_config_ex->src_planes[0].w = s_canvas_w;
                ge2d_config_ex->src_planes[0].h = s_canvas_h;
                ge2d_config_ex->src_planes[1].addr = input_buffer_info->offset[1];
                ge2d_config_ex->src_planes[1].shared_fd = input_buffer_info->shared_fd[1];
                ge2d_config_ex->src_planes[1].w = CANVAS_ALIGNED(s_canvas_w/2);
                ge2d_config_ex->src_planes[1].h = s_canvas_h/2;
                ge2d_config_ex->src_planes[2].addr = input_buffer_info->offset[2];
                ge2d_config_ex->src_planes[2].shared_fd = input_buffer_info->shared_fd[2];
                ge2d_config_ex->src_planes[2].w = CANVAS_ALIGNED(s_canvas_w/2);
                ge2d_config_ex->src_planes[2].h = s_canvas_h/2;
                if ( !(d_canvas_w % 64) || !(d_canvas_w % 32))
                    D_GE2D("yu12 src canvas_w not alligned\n ");
            }
        } else if (input_buffer_info->format == PIXEL_FORMAT_YCbCr_422_SP) {
            if (input_buffer_info->plane_number == 1) {
                ge2d_config_ex->src_planes[0].addr = input_buffer_info->offset[0];
                ge2d_config_ex->src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
                ge2d_config_ex->src_planes[0].w = s_canvas_w;
                ge2d_config_ex->src_planes[0].h = s_canvas_h;
                ge2d_config_ex->src_planes[1].addr = (s_canvas_w * s_canvas_h);
                ge2d_config_ex->src_planes[1].shared_fd = 0;
                ge2d_config_ex->src_planes[1].w = s_canvas_w;
                ge2d_config_ex->src_planes[1].h = s_canvas_h;
            } else if (input_buffer_info->plane_number == 2) {
                ge2d_config_ex->src_planes[0].addr = input_buffer_info->offset[0];
                ge2d_config_ex->src_planes[0].shared_fd = input_buffer_info->shared_fd[0];
                ge2d_config_ex->src_planes[0].w = s_canvas_w;
                ge2d_config_ex->src_planes[0].h = s_canvas_h;
                ge2d_config_ex->src_planes[1].addr = input_buffer_info->offset[1];
                ge2d_config_ex->src_planes[1].shared_fd = input_buffer_info->shared_fd[1];
                ge2d_config_ex->src_planes[1].w = s_canvas_w;
                ge2d_config_ex->src_planes[1].h = s_canvas_h;
            }
        } else {
            E_GE2D("format is not match, should config src_planes correct.\n");
            return ge2d_fail;
        }
    }


    if (CANVAS_ALLOC == input2_buffer_info->memtype) {
        if (is_one_plane_input2) {
            ge2d_config_ex->src2_planes[0].addr = input2_buffer_info->offset[0];
            ge2d_config_ex->src2_planes[0].shared_fd = input2_buffer_info->shared_fd[0];
            ge2d_config_ex->src2_planes[0].w = s2_canvas_w;
            ge2d_config_ex->src2_planes[0].h = s2_canvas_h;
        } else if ((input2_buffer_info->format == PIXEL_FORMAT_YCrCb_420_SP) ||
            (input2_buffer_info->format == PIXEL_FORMAT_YCbCr_420_SP_NV12)) {
            if (input2_buffer_info->plane_number == 1) {
                ge2d_config_ex->src2_planes[0].addr = input2_buffer_info->offset[0];
                ge2d_config_ex->src2_planes[0].shared_fd = input2_buffer_info->shared_fd[0];
                ge2d_config_ex->src2_planes[0].w = s2_canvas_w;
                ge2d_config_ex->src2_planes[0].h = s2_canvas_h;
                ge2d_config_ex->src2_planes[1].addr = (s2_canvas_w * s2_canvas_h);
                ge2d_config_ex->src2_planes[1].shared_fd = 0;
                ge2d_config_ex->src2_planes[1].w = s2_canvas_w;
                ge2d_config_ex->src2_planes[1].h = s2_canvas_h/2;
            } else if (input2_buffer_info->plane_number == 2) {
                ge2d_config_ex->src2_planes[0].addr = input2_buffer_info->offset[0];
                ge2d_config_ex->src2_planes[0].shared_fd = input2_buffer_info->shared_fd[0];
                ge2d_config_ex->src2_planes[0].w = s2_canvas_w;
                ge2d_config_ex->src2_planes[0].h = s2_canvas_h;
                ge2d_config_ex->src2_planes[1].addr = input2_buffer_info->offset[1];
                ge2d_config_ex->src2_planes[1].shared_fd = input2_buffer_info->shared_fd[1];
                ge2d_config_ex->src2_planes[1].w = s2_canvas_w;
                ge2d_config_ex->src2_planes[1].h = s2_canvas_h/2;
            }
        } else if (input2_buffer_info->format == PIXEL_FORMAT_Y8 ||
                   input2_buffer_info->format == PIXEL_FORMAT_CLUT8 ||
                   input2_buffer_info->format == PIXEL_FORMAT_ALPHA8) {
            ge2d_config_ex->src2_planes[0].addr = input2_buffer_info->offset[0];
            ge2d_config_ex->src2_planes[0].shared_fd = input2_buffer_info->shared_fd[0];
            ge2d_config_ex->src2_planes[0].w = s2_canvas_w;
            ge2d_config_ex->src2_planes[0].h = s2_canvas_h;
        } else if (input2_buffer_info->format == PIXEL_FORMAT_YV12) {
            if (input2_buffer_info->plane_number == 1) {
                ge2d_config_ex->src2_planes[0].addr = input2_buffer_info->offset[0];
                ge2d_config_ex->src2_planes[0].shared_fd = input2_buffer_info->shared_fd[0];
                ge2d_config_ex->src2_planes[0].w = s2_canvas_w;
                ge2d_config_ex->src2_planes[0].h = s2_canvas_h;
                /* android is ycrcb,kernel is ycbcr,swap the addr */
                ge2d_config_ex->src2_planes[1].addr = YV12_Y_ALIGNED(s2_canvas_w) *
                    s2_canvas_h + CANVAS_ALIGNED(s2_canvas_w/2)* s2_canvas_h/2;
                ge2d_config_ex->src2_planes[1].shared_fd = 0;
                ge2d_config_ex->src2_planes[1].w = CANVAS_ALIGNED(s2_canvas_w/2);
                ge2d_config_ex->src2_planes[1].h = s2_canvas_h/2;
                ge2d_config_ex->src2_planes[2].addr = YV12_Y_ALIGNED(s2_canvas_w) * s2_canvas_h;
                ge2d_config_ex->src2_planes[2].shared_fd = 0;
                ge2d_config_ex->src2_planes[2].w = CANVAS_ALIGNED(s2_canvas_w/2);
                ge2d_config_ex->src2_planes[2].h = s2_canvas_h/2;
                if ( !(d_canvas_w % 64) || !(d_canvas_w % 32))
                    D_GE2D("yv12 src2 canvas_w not alligned\n ");
             } else if (input2_buffer_info->plane_number == 3) {
                ge2d_config_ex->src2_planes[0].addr = input2_buffer_info->offset[0];
                ge2d_config_ex->src2_planes[0].shared_fd = input2_buffer_info->shared_fd[0];
                ge2d_config_ex->src2_planes[0].w = s2_canvas_w;
                ge2d_config_ex->src2_planes[0].h = s2_canvas_h;
                /* android is ycrcb,kernel is ycbcr,swap the addr */
                ge2d_config_ex->src2_planes[1].addr = input2_buffer_info->offset[1];
                ge2d_config_ex->src2_planes[1].shared_fd = input2_buffer_info->shared_fd[2];
                ge2d_config_ex->src2_planes[1].w = CANVAS_ALIGNED(s2_canvas_w/2);
                ge2d_config_ex->src2_planes[1].h = s2_canvas_h/2;
                ge2d_config_ex->src2_planes[2].addr = input2_buffer_info->offset[2];
                ge2d_config_ex->src2_planes[2].shared_fd = input2_buffer_info->shared_fd[1];
                ge2d_config_ex->src2_planes[2].w = CANVAS_ALIGNED(s2_canvas_w/2);
                ge2d_config_ex->src2_planes[2].h = s2_canvas_h/2;
                if ( !(d_canvas_w % 64) || !(d_canvas_w % 32))
                    D_GE2D("yv12 src2 canvas_w not alligned\n ");
             }
        } else if (input2_buffer_info->format == PIXEL_FORMAT_YU12) {
            if (input2_buffer_info->plane_number == 1) {
                ge2d_config_ex->src2_planes[0].addr = input2_buffer_info->offset[0];
                ge2d_config_ex->src2_planes[0].shared_fd = input2_buffer_info->shared_fd[0];
                ge2d_config_ex->src2_planes[0].w = s2_canvas_w;
                ge2d_config_ex->src2_planes[0].h = s2_canvas_h;
                ge2d_config_ex->src2_planes[1].addr = YV12_Y_ALIGNED(s2_canvas_w) * s2_canvas_h;
                ge2d_config_ex->src2_planes[1].shared_fd = 0;
                ge2d_config_ex->src2_planes[1].w = CANVAS_ALIGNED(s2_canvas_w/2);
                ge2d_config_ex->src2_planes[1].h = s2_canvas_h/2;
                ge2d_config_ex->src2_planes[2].addr = YV12_Y_ALIGNED(s2_canvas_w) *
                    s2_canvas_h + CANVAS_ALIGNED(s2_canvas_w/2)* s2_canvas_h/2;
                ge2d_config_ex->src2_planes[2].shared_fd = 0;
                ge2d_config_ex->src2_planes[2].w = CANVAS_ALIGNED(s2_canvas_w/2);
                ge2d_config_ex->src2_planes[2].h = s2_canvas_h/2;
                if ( !(d_canvas_w % 64) || !(d_canvas_w % 32))
                    D_GE2D("yu12 src2 canvas_w not alligned\n ");
             } else if (input2_buffer_info->plane_number == 3) {
                ge2d_config_ex->src2_planes[0].addr = input2_buffer_info->offset[0];
                ge2d_config_ex->src2_planes[0].shared_fd = input2_buffer_info->shared_fd[0];
                ge2d_config_ex->src2_planes[0].w = s2_canvas_w;
                ge2d_config_ex->src2_planes[0].h = s2_canvas_h;
                ge2d_config_ex->src2_planes[1].addr = input2_buffer_info->offset[1];
                ge2d_config_ex->src2_planes[1].shared_fd = input2_buffer_info->shared_fd[1];
                ge2d_config_ex->src2_planes[1].w = CANVAS_ALIGNED(s2_canvas_w/2);
                ge2d_config_ex->src2_planes[1].h = s2_canvas_h/2;
                ge2d_config_ex->src2_planes[2].addr = input2_buffer_info->offset[2];
                ge2d_config_ex->src2_planes[2].shared_fd = input2_buffer_info->shared_fd[2];
                ge2d_config_ex->src2_planes[2].w = CANVAS_ALIGNED(s2_canvas_w/2);
                ge2d_config_ex->src2_planes[2].h = s2_canvas_h/2;
                if ( !(d_canvas_w % 64) || !(d_canvas_w % 32))
                    D_GE2D("yu12 src2 canvas_w not alligned\n ");
             }
        } else if (input2_buffer_info->format == PIXEL_FORMAT_YCbCr_422_SP) {
            if (input2_buffer_info->plane_number == 1) {
                ge2d_config_ex->src2_planes[0].addr = input2_buffer_info->offset[0];
                ge2d_config_ex->src2_planes[0].shared_fd = input2_buffer_info->shared_fd[0];
                ge2d_config_ex->src2_planes[0].w = s2_canvas_w;
                ge2d_config_ex->src2_planes[0].h = s2_canvas_h;
                ge2d_config_ex->src2_planes[1].addr = (s2_canvas_w * s2_canvas_h);
                ge2d_config_ex->src2_planes[1].shared_fd = 0;
                ge2d_config_ex->src2_planes[1].w = s2_canvas_w;
                ge2d_config_ex->src2_planes[1].h = s2_canvas_h;
            } else if (input2_buffer_info->plane_number == 2) {
                ge2d_config_ex->src2_planes[0].addr = input2_buffer_info->offset[0];
                ge2d_config_ex->src2_planes[0].shared_fd = input2_buffer_info->shared_fd[0];
                ge2d_config_ex->src2_planes[0].w = s2_canvas_w;
                ge2d_config_ex->src2_planes[0].h = s2_canvas_h;
                ge2d_config_ex->src2_planes[1].addr = input2_buffer_info->offset[1];
                ge2d_config_ex->src2_planes[1].shared_fd = input2_buffer_info->shared_fd[1];
                ge2d_config_ex->src2_planes[1].w = s2_canvas_w;
                ge2d_config_ex->src2_planes[1].h = s2_canvas_h;

            }
        } else {
            E_GE2D("format is not match, should config src2_planes correct.\n");
            return ge2d_fail;
        }
    }

    if (CANVAS_ALLOC == output_buffer_info->memtype) {
        if (is_one_plane_output) {
            ge2d_config_ex->dst_planes[0].addr = output_buffer_info->offset[0];
            ge2d_config_ex->dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
            ge2d_config_ex->dst_planes[0].w = d_canvas_w;
            ge2d_config_ex->dst_planes[0].h = d_canvas_h;
        } else if ((output_buffer_info->format == PIXEL_FORMAT_YCrCb_420_SP) ||
                (output_buffer_info->format ==PIXEL_FORMAT_YCbCr_420_SP_NV12)) {
            if (output_buffer_info->plane_number == 1) {
                ge2d_config_ex->dst_planes[0].addr = output_buffer_info->offset[0];
                ge2d_config_ex->dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                ge2d_config_ex->dst_planes[0].w = d_canvas_w;
                ge2d_config_ex->dst_planes[0].h = d_canvas_h;
                ge2d_config_ex->dst_planes[1].addr = (d_canvas_w * d_canvas_h);
                ge2d_config_ex->dst_planes[1].shared_fd = 0;
                ge2d_config_ex->dst_planes[1].w = d_canvas_w;
                ge2d_config_ex->dst_planes[1].h = d_canvas_h/2;
            } else if (output_buffer_info->plane_number == 2) {
                ge2d_config_ex->dst_planes[0].addr = output_buffer_info->offset[0];
                ge2d_config_ex->dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                ge2d_config_ex->dst_planes[0].w = d_canvas_w;
                ge2d_config_ex->dst_planes[0].h = d_canvas_h;
                ge2d_config_ex->dst_planes[1].addr = output_buffer_info->offset[1];
                ge2d_config_ex->dst_planes[1].shared_fd = output_buffer_info->shared_fd[1];
                ge2d_config_ex->dst_planes[1].w = d_canvas_w;
                ge2d_config_ex->dst_planes[1].h = d_canvas_h/2;

            }
        }  else if (output_buffer_info->format == PIXEL_FORMAT_Y8 ||
                    output_buffer_info->format == PIXEL_FORMAT_CLUT8 ||
                    output_buffer_info->format == PIXEL_FORMAT_ALPHA8) {
            ge2d_config_ex->dst_planes[0].addr = output_buffer_info->offset[0];
            ge2d_config_ex->dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
            ge2d_config_ex->dst_planes[0].w = d_canvas_w;
            ge2d_config_ex->dst_planes[0].h = d_canvas_h;
        } else if (output_buffer_info->format == PIXEL_FORMAT_YV12 ||
                   output_buffer_info->format == PIXEL_FORMAT_YU12) {
            switch (pge2dinfo->dst_op_cnt) {
            case 0:
                ge2d_config_ex->dst_para.format = GE2D_FORMAT_S8_Y | dst_endian;
                ge2d_config_ex->dst_planes[0].addr = output_buffer_info->offset[0];
                ge2d_config_ex->dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                ge2d_config_ex->dst_planes[0].w = d_canvas_w;
                ge2d_config_ex->dst_planes[0].h = d_canvas_h;
                ge2d_config_ex->dst_para.width = d_canvas_w;
                ge2d_config_ex->dst_para.height = d_canvas_h;
                break;
            case 1:
                if (output_buffer_info->format == PIXEL_FORMAT_YV12)
                    ge2d_config_ex->dst_para.format = GE2D_FORMAT_S8_CR | dst_endian;
                else
                    ge2d_config_ex->dst_para.format = GE2D_FORMAT_S8_CB | dst_endian;
                ge2d_config_ex->dst_planes[0].w = d_canvas_w/2;
                ge2d_config_ex->dst_planes[0].h = d_canvas_h/2;
                ge2d_config_ex->dst_para.width = d_canvas_w/2;
                ge2d_config_ex->dst_para.height = d_canvas_h/2;
                if (output_buffer_info->plane_number == 1) {
                    ge2d_config_ex->dst_planes[0].addr = YV12_Y_ALIGNED(d_canvas_w) *
                        d_canvas_h;
                    ge2d_config_ex->dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                } else if (output_buffer_info->plane_number == 3) {
                    ge2d_config_ex->dst_planes[0].addr = output_buffer_info->offset[1];
                    ge2d_config_ex->dst_planes[0].shared_fd = output_buffer_info->shared_fd[1];
                }
                break;
            case 2:
                if (output_buffer_info->format == PIXEL_FORMAT_YV12)
                    ge2d_config_ex->dst_para.format = GE2D_FORMAT_S8_CB | dst_endian;
                else
                    ge2d_config_ex->dst_para.format = GE2D_FORMAT_S8_CR | dst_endian;
                ge2d_config_ex->dst_planes[0].w = d_canvas_w/2;
                ge2d_config_ex->dst_planes[0].h = d_canvas_h/2;
                ge2d_config_ex->dst_para.width = d_canvas_w/2;
                ge2d_config_ex->dst_para.height = d_canvas_h/2;
                if (output_buffer_info->plane_number == 1) {
                    ge2d_config_ex->dst_planes[0].addr = YV12_Y_ALIGNED(d_canvas_w) *
                        d_canvas_h + CANVAS_ALIGNED(d_canvas_w/2) * d_canvas_h/2;
                    ge2d_config_ex->dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                } else if (output_buffer_info->plane_number == 3) {
                    ge2d_config_ex->dst_planes[0].addr = output_buffer_info->offset[2];
                    ge2d_config_ex->dst_planes[0].shared_fd = output_buffer_info->shared_fd[2];
                }
                break;
            default:
                break;
            }
        } else if (output_buffer_info->format == PIXEL_FORMAT_YCbCr_422_SP) {
            if (output_buffer_info->plane_number == 1) {
                ge2d_config_ex->dst_planes[0].addr = output_buffer_info->offset[0];
                ge2d_config_ex->dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                ge2d_config_ex->dst_planes[0].w = d_canvas_w;
                ge2d_config_ex->dst_planes[0].h = d_canvas_h;
                ge2d_config_ex->dst_planes[1].addr = (d_canvas_w * d_canvas_h);
                ge2d_config_ex->dst_planes[1].shared_fd = 0;
                ge2d_config_ex->dst_planes[1].w = d_canvas_w;
                ge2d_config_ex->dst_planes[1].h = d_canvas_h;
            } else if (output_buffer_info->plane_number == 2) {
                ge2d_config_ex->dst_planes[0].addr = output_buffer_info->offset[0];
                ge2d_config_ex->dst_planes[0].shared_fd = output_buffer_info->shared_fd[0];
                ge2d_config_ex->dst_planes[0].w = d_canvas_w;
                ge2d_config_ex->dst_planes[0].h = d_canvas_h;
                ge2d_config_ex->dst_planes[1].addr = output_buffer_info->offset[1];
                ge2d_config_ex->dst_planes[1].shared_fd = output_buffer_info->shared_fd[1];
                ge2d_config_ex->dst_planes[1].w = d_canvas_w;
                ge2d_config_ex->dst_planes[1].h = d_canvas_h;
            }
        } else {
            E_GE2D("format is not match, should config dst_planes correct.\n");
            return ge2d_fail;
        }
    }
    ge2d_config_ex->alu_const_color = pge2dinfo->const_color;
    ge2d_config_ex->src1_gb_alpha = input_buffer_info->plane_alpha & 0xff;
    ge2d_config_ex->src1_gb_alpha_en = 1;
    if (pge2dinfo->cap_attr & SRC2_ALPHA) {
        ge2d_config_ex->src2_gb_alpha = input2_buffer_info->plane_alpha & 0xff;
        ge2d_config_ex->src2_gb_alpha_en = 1;
    }
    D_GE2D("blend:src1_cmult_asel=%d,src2_cmult_ase2=%d,gl_alpha=%x,src1_gb_alpha_en=%d, src2_gb_alpha=%x, src2_gb_alpha_en=%d, src2_cmult_ad=%d\n",
        ge2d_config_ex->src1_cmult_asel,
        ge2d_config_ex->src2_cmult_asel,
        ge2d_config_ex->src1_gb_alpha,
        ge2d_config_ex->src1_gb_alpha_en,
        ge2d_config_ex->src2_gb_alpha,
         ge2d_config_ex->src2_gb_alpha_en,
        ge2d_config_ex->src2_cmult_ad);

    ret = ioctl(fd, GE2D_CONFIG_EX_MEM, &ge2d_config_para_ex);
    if (ret < 0) {
        E_GE2D("ge2d config ex dma failed. \n");
        return ge2d_fail;
    }

    return ge2d_success;
}

static int ge2d_fillrectangle(int fd,rectangle_t *rect,unsigned int color,
                              int enqueue)
{
    int ret;
    ge2d_op_para_t op_ge2d_info;
    memset(&op_ge2d_info, 0, sizeof(ge2d_op_para_t));
    D_GE2D("ge2d_fillrectangle:rect x is %d, y is %d, w is %d, h is %d\n",
        rect->x, rect->y, rect->w, rect->h);
    D_GE2D("color is %d\n", color);

    op_ge2d_info.src1_rect.x = rect->x;
    op_ge2d_info.src1_rect.y = rect->y;
    op_ge2d_info.src1_rect.w = rect->w;
    op_ge2d_info.src1_rect.h = rect->h;

    op_ge2d_info.dst_rect.x = rect->x;
    op_ge2d_info.dst_rect.y = rect->y;
    op_ge2d_info.dst_rect.w = rect->w;
    op_ge2d_info.dst_rect.h = rect->h;
    op_ge2d_info.color = color;

    if (!enqueue)
        ret = ioctl(fd, GE2D_FILLRECTANGLE, &op_ge2d_info);
    else
        ret = ioctl(fd, GE2D_FILLRECTANGLE_ENQUEUE, &op_ge2d_info);
    if (ret != 0) {
        E_GE2D("%s,%d,ret %d,ioctl failed!\n",__FUNCTION__,__LINE__, ret);
        return ge2d_fail;
    }
    return ge2d_success;
}

static int ge2d_blit(int fd, aml_ge2d_info_t *pge2dinfo, rectangle_t *rect,
                     unsigned int dx, unsigned int dy, int enqueue)
{
    int ret;
    int dst_format = pge2dinfo->dst_info.format;
    ge2d_op_para_t op_ge2d_info;
    memset(&op_ge2d_info, 0, sizeof(ge2d_op_para_t));
    D_GE2D("ge2d_blit:rect x is %d, y is %d, w is %d, h is %d\n",
        rect->x, rect->y, rect->w, rect->h);
    D_GE2D("dx is %d, dy is %d\n", dx, dy);

    op_ge2d_info.src1_rect.x = rect->x;
    op_ge2d_info.src1_rect.y = rect->y;
    op_ge2d_info.src1_rect.w = rect->w;
    op_ge2d_info.src1_rect.h = rect->h;

    switch (dst_format) {
    case PIXEL_FORMAT_RGBA_8888:
    case PIXEL_FORMAT_RGBX_8888:
    case PIXEL_FORMAT_ARGB_8888:
    case PIXEL_FORMAT_ABGR_8888:
    case PIXEL_FORMAT_RGB_888:
    case PIXEL_FORMAT_BGR_888:
    case PIXEL_FORMAT_RGB_565:
    case PIXEL_FORMAT_ARGB_4444:
    case PIXEL_FORMAT_RGBA_4444:
    case PIXEL_FORMAT_ARGB_1555:
    case PIXEL_FORMAT_BGRA_8888:
    case PIXEL_FORMAT_Y8:
    case PIXEL_FORMAT_CLUT8:
    case PIXEL_FORMAT_YCrCb_420_SP:
    case PIXEL_FORMAT_YCbCr_422_SP:
    case PIXEL_FORMAT_YCbCr_420_SP_NV12:
    case PIXEL_FORMAT_YCbCr_422_UYVY:
    case PIXEL_FORMAT_ALPHA8:
        if (pge2dinfo->dst_op_cnt == 0) {
            op_ge2d_info.dst_rect.x = dx;
            op_ge2d_info.dst_rect.y = dy;
            op_ge2d_info.dst_rect.w = rect->w;
            op_ge2d_info.dst_rect.h = rect->h;
        }
        break;
    case PIXEL_FORMAT_YV12:
    case PIXEL_FORMAT_YU12:
        switch (pge2dinfo->dst_op_cnt) {
        case 0:
            op_ge2d_info.dst_rect.x = dx;
            op_ge2d_info.dst_rect.y = dy;
            op_ge2d_info.dst_rect.w = rect->w;
            op_ge2d_info.dst_rect.h = rect->h;
            break;
        case 1:
        case 2:
            op_ge2d_info.dst_rect.x = dx/2;
            op_ge2d_info.dst_rect.y = dy/2;
            op_ge2d_info.dst_rect.w = rect->w/2;
            op_ge2d_info.dst_rect.h = rect->h/2;
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }

    if (!enqueue)
        ret = ioctl(fd, GE2D_BLIT, &op_ge2d_info);
    else
        ret = ioctl(fd, GE2D_BLIT_ENQUEUE, &op_ge2d_info);
    if (ret != 0) {
        E_GE2D("%s,%d,ret %d,ioctl failed!\n",__FUNCTION__,__LINE__, ret);
        return ge2d_fail;
    }
    return ge2d_success;
}

static int ge2d_blit_noalpha(int fd, aml_ge2d_info_t *pge2dinfo,
                             rectangle_t *rect,unsigned int dx,unsigned int dy,
                             int enqueue)
{
    int ret;
    int dst_format = pge2dinfo->dst_info.format;
    ge2d_op_para_t op_ge2d_info;
    memset(&op_ge2d_info, 0, sizeof(ge2d_op_para_t));
    D_GE2D("ge2d_blit_noalpha:rect x is %d, y is %d, w is %d, h is %d\n",
        rect->x, rect->y, rect->w, rect->h);

    D_GE2D("dx is %d, dy is %d\n", dx, dy);

    op_ge2d_info.src1_rect.x = rect->x;
    op_ge2d_info.src1_rect.y = rect->y;
    op_ge2d_info.src1_rect.w = rect->w;
    op_ge2d_info.src1_rect.h = rect->h;

    switch (dst_format) {
    case PIXEL_FORMAT_RGBA_8888:
    case PIXEL_FORMAT_RGBX_8888:
    case PIXEL_FORMAT_ARGB_8888:
    case PIXEL_FORMAT_ABGR_8888:
    case PIXEL_FORMAT_RGB_888:
    case PIXEL_FORMAT_BGR_888:
    case PIXEL_FORMAT_RGB_565:
    case PIXEL_FORMAT_ARGB_4444:
    case PIXEL_FORMAT_RGBA_4444:
    case PIXEL_FORMAT_ARGB_1555:
    case PIXEL_FORMAT_BGRA_8888:
    case PIXEL_FORMAT_Y8:
    case PIXEL_FORMAT_CLUT8:
    case PIXEL_FORMAT_YCrCb_420_SP:
    case PIXEL_FORMAT_YCbCr_422_SP:
    case PIXEL_FORMAT_YCbCr_420_SP_NV12:
    case PIXEL_FORMAT_YCbCr_422_UYVY:
    case PIXEL_FORMAT_ALPHA8:
        if (pge2dinfo->dst_op_cnt == 0) {
            op_ge2d_info.dst_rect.x = dx;
            op_ge2d_info.dst_rect.y = dy;
            op_ge2d_info.dst_rect.w = rect->w;
            op_ge2d_info.dst_rect.h = rect->h;
        }
        break;
    case PIXEL_FORMAT_YV12:
    case PIXEL_FORMAT_YU12:
        switch (pge2dinfo->dst_op_cnt) {
        case 0:
            op_ge2d_info.dst_rect.x = dx;
            op_ge2d_info.dst_rect.y = dy;
            op_ge2d_info.dst_rect.w = rect->w;
            op_ge2d_info.dst_rect.h = rect->h;
            break;
        case 1:
        case 2:
            op_ge2d_info.dst_rect.x = dx/2;
            op_ge2d_info.dst_rect.y = dy/2;
            op_ge2d_info.dst_rect.w = rect->w/2;
            op_ge2d_info.dst_rect.h = rect->h/2;
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }

    if (!enqueue)
        ret = ioctl(fd, GE2D_BLIT_NOALPHA, &op_ge2d_info);
    else
        ret = ioctl(fd, GE2D_BLIT_NOALPHA_ENQUEUE, &op_ge2d_info);
    if ( ret != 0) {
        E_GE2D("%s,%d,ret %d,ioctl failed!\n",__FUNCTION__,__LINE__, ret);
        return ge2d_fail;
    }
    return ge2d_success;
}

static int ge2d_strechblit(int fd, aml_ge2d_info_t *pge2dinfo,
                           rectangle_t *srect,rectangle_t *drect, int enqueue)
{
    int ret;
    int dst_format = pge2dinfo->dst_info.format;
    ge2d_op_para_t op_ge2d_info;
    memset(&op_ge2d_info, 0, sizeof(ge2d_op_para_t));
    D_GE2D("stretchblit srect[%d %d %d %d] drect[%d %d %d %d]\n",
        srect->x,srect->y,srect->w,srect->h,drect->x,drect->y,drect->w,drect->h);

    op_ge2d_info.src1_rect.x = srect->x;
    op_ge2d_info.src1_rect.y = srect->y;
    op_ge2d_info.src1_rect.w = srect->w;
    op_ge2d_info.src1_rect.h = srect->h;

    switch (dst_format) {
    case PIXEL_FORMAT_RGBA_8888:
    case PIXEL_FORMAT_RGBX_8888:
    case PIXEL_FORMAT_ARGB_8888:
    case PIXEL_FORMAT_ABGR_8888:
    case PIXEL_FORMAT_RGB_888:
    case PIXEL_FORMAT_BGR_888:
    case PIXEL_FORMAT_RGB_565:
    case PIXEL_FORMAT_ARGB_4444:
    case PIXEL_FORMAT_RGBA_4444:
    case PIXEL_FORMAT_ARGB_1555:
    case PIXEL_FORMAT_BGRA_8888:
    case PIXEL_FORMAT_Y8:
    case PIXEL_FORMAT_CLUT8:
    case PIXEL_FORMAT_YCbCr_422_SP:
    case PIXEL_FORMAT_YCrCb_420_SP:
    case PIXEL_FORMAT_YCbCr_420_SP_NV12:
    case PIXEL_FORMAT_YCbCr_422_UYVY:
    case PIXEL_FORMAT_ALPHA8:
        if (pge2dinfo->dst_op_cnt == 0) {
            op_ge2d_info.dst_rect.x = drect->x;
            op_ge2d_info.dst_rect.y = drect->y;
            op_ge2d_info.dst_rect.w = drect->w;
            op_ge2d_info.dst_rect.h = drect->h;
        }
        break;
    case PIXEL_FORMAT_YV12:
    case PIXEL_FORMAT_YU12:
        switch (pge2dinfo->dst_op_cnt) {
        case 0:
            op_ge2d_info.dst_rect.x = drect->x;
            op_ge2d_info.dst_rect.y = drect->y;
            op_ge2d_info.dst_rect.w = drect->w;
            op_ge2d_info.dst_rect.h = drect->h;
            break;
        case 1:
        case 2:
            op_ge2d_info.dst_rect.x = drect->x/2;
            op_ge2d_info.dst_rect.y = drect->y/2;
            op_ge2d_info.dst_rect.w = drect->w/2;
            op_ge2d_info.dst_rect.h = drect->h/2;
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }

    if (!enqueue)
        ret = ioctl(fd, GE2D_STRETCHBLIT, &op_ge2d_info);
    else
        ret = ioctl(fd, GE2D_STRETCHBLIT_ENQUEUE, &op_ge2d_info);
    if (ret != 0) {
        E_GE2D("%s,%d,ret %d,ioctl failed!\n",__FUNCTION__,__LINE__, ret);
        return ge2d_fail;
    }
    return ge2d_success;
}


static int ge2d_strechblit_noalpha(int fd, aml_ge2d_info_t *pge2dinfo,
                                   rectangle_t *srect,rectangle_t *drect,
                                   int enqueue)
{
    int ret;
    int dst_format = pge2dinfo->dst_info.format;
    ge2d_op_para_t op_ge2d_info;
    memset(&op_ge2d_info, 0, sizeof(ge2d_op_para_t));
    D_GE2D("stretchblit srect[%d %d %d %d] drect[%d %d %d %d]\n",
        srect->x,srect->y,srect->w,srect->h,drect->x,drect->y,drect->w,drect->h);

    op_ge2d_info.src1_rect.x = srect->x;
    op_ge2d_info.src1_rect.y = srect->y;
    op_ge2d_info.src1_rect.w = srect->w;
    op_ge2d_info.src1_rect.h = srect->h;

    switch (dst_format) {
    case PIXEL_FORMAT_RGBA_8888:
    case PIXEL_FORMAT_RGBX_8888:
    case PIXEL_FORMAT_ARGB_8888:
    case PIXEL_FORMAT_ABGR_8888:
    case PIXEL_FORMAT_RGB_888:
    case PIXEL_FORMAT_BGR_888:
    case PIXEL_FORMAT_RGB_565:
    case PIXEL_FORMAT_ARGB_4444:
    case PIXEL_FORMAT_RGBA_4444:
    case PIXEL_FORMAT_ARGB_1555:
    case PIXEL_FORMAT_BGRA_8888:
    case PIXEL_FORMAT_Y8:
    case PIXEL_FORMAT_CLUT8:
    case PIXEL_FORMAT_YCbCr_422_SP:
    case PIXEL_FORMAT_YCrCb_420_SP:
    case PIXEL_FORMAT_YCbCr_420_SP_NV12:
    case PIXEL_FORMAT_YCbCr_422_UYVY:
    case PIXEL_FORMAT_ALPHA8:
        if (pge2dinfo->dst_op_cnt == 0) {
            op_ge2d_info.dst_rect.x = drect->x;
            op_ge2d_info.dst_rect.y = drect->y;
            op_ge2d_info.dst_rect.w = drect->w;
            op_ge2d_info.dst_rect.h = drect->h;
        }
        break;
    case PIXEL_FORMAT_YV12:
    case PIXEL_FORMAT_YU12:
        switch (pge2dinfo->dst_op_cnt) {
        case 0:
            op_ge2d_info.dst_rect.x = drect->x;
            op_ge2d_info.dst_rect.y = drect->y;
            op_ge2d_info.dst_rect.w = drect->w;
            op_ge2d_info.dst_rect.h = drect->h;
            break;
        case 1:
        case 2:
            op_ge2d_info.dst_rect.x = drect->x/2;
            op_ge2d_info.dst_rect.y = drect->y/2;
            op_ge2d_info.dst_rect.w = drect->w/2;
            op_ge2d_info.dst_rect.h = drect->h/2;
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }

    if (!enqueue)
        ret = ioctl(fd, GE2D_STRETCHBLIT_NOALPHA, &op_ge2d_info);
    else
        ret = ioctl(fd, GE2D_STRETCHBLIT_NOALPHA_ENQUEUE, &op_ge2d_info);
    if (ret != 0) {
        E_GE2D("%s,%d,ret %d,ioctl failed!\n",__FUNCTION__,__LINE__, ret);
        return ge2d_fail;
    }
    return ge2d_success;
}


static int ge2d_blend(int fd, aml_ge2d_info_t *pge2dinfo,
                      rectangle_t *srect, rectangle_t *srect2,
                      rectangle_t *drect, unsigned int op, int enqueue)
{
    int ret;
    ge2d_op_para_t op_ge2d_info;
    ge2d_blend_op blend_op;
    int max_d_w, max_d_h;
    D_GE2D("ge2d_blend srect[%d %d %d %d], s2rect[%d %d %d %d],drect[%d %d %d %d]\n",
        srect->x,srect->y,srect->w,srect->h,
        srect2->x,srect2->y,srect2->w,srect2->h,
        drect->x,drect->y,drect->w,drect->h);
    D_GE2D("ge2d_blend:blend_mode=%d\n",op);
    memset(&blend_op,0,sizeof(ge2d_blend_op));
    #if 0
    max_d_w = (srect->w > srect2->w) ? srect2->w : srect->w;
    max_d_h = (srect->h > srect2->h) ? srect2->h : srect->h;
    if ((drect->w > max_d_w) || (drect->h > max_d_h)) {
        /* dst rect must be min(srect,srect2),otherwise ge2d will timeout */
        E_GE2D("dst rect w=%d,h=%d out of range\n",drect->w,drect->h);
        return ge2d_fail;
    }
    #endif
    op_ge2d_info.src1_rect.x = srect->x;
    op_ge2d_info.src1_rect.y = srect->y;
    op_ge2d_info.src1_rect.w = srect->w;
    op_ge2d_info.src1_rect.h = srect->h;

    op_ge2d_info.src2_rect.x = srect2->x;
    op_ge2d_info.src2_rect.y = srect2->y;
    op_ge2d_info.src2_rect.w = srect2->w;
    op_ge2d_info.src2_rect.h = srect2->h;

    op_ge2d_info.dst_rect.x = drect->x;
    op_ge2d_info.dst_rect.y = drect->y;
    op_ge2d_info.dst_rect.w = drect->w;
    op_ge2d_info.dst_rect.h = drect->h;

    blend_op.color_blending_mode = OPERATION_ADD;
    blend_op.alpha_blending_mode = OPERATION_ADD;
    /* b_src_swap = 1:src1 & src2 swap, so blend factor used dst instead src */
    switch (op) {
        case BLEND_MODE_NONE:
            if (pge2dinfo->b_src_swap) {
                blend_op.color_blending_src_factor = COLOR_FACTOR_ZERO;
                blend_op.color_blending_dst_factor = COLOR_FACTOR_ONE;
                blend_op.alpha_blending_src_factor = ALPHA_FACTOR_ZERO;
                blend_op.alpha_blending_dst_factor = ALPHA_FACTOR_ONE;
            }
            else {
                blend_op.color_blending_src_factor = COLOR_FACTOR_ONE;
                blend_op.color_blending_dst_factor = COLOR_FACTOR_ZERO;
                blend_op.alpha_blending_src_factor = ALPHA_FACTOR_ONE;
                blend_op.alpha_blending_dst_factor = ALPHA_FACTOR_ZERO;
            }
            break;
        case BLEND_MODE_PREMULTIPLIED:
            if (pge2dinfo->b_src_swap) {
                blend_op.color_blending_src_factor = COLOR_FACTOR_ONE_MINUS_DST_ALPHA;
                blend_op.color_blending_dst_factor = COLOR_FACTOR_ONE;
                blend_op.alpha_blending_src_factor = ALPHA_FACTOR_ONE_MINUS_DST_ALPHA;
                blend_op.alpha_blending_dst_factor = ALPHA_FACTOR_ONE;
            }
            else {
                blend_op.color_blending_src_factor = COLOR_FACTOR_ONE;
                blend_op.color_blending_dst_factor = COLOR_FACTOR_ONE_MINUS_SRC_ALPHA;
                blend_op.alpha_blending_src_factor = ALPHA_FACTOR_ONE;
                blend_op.alpha_blending_dst_factor = ALPHA_FACTOR_ONE_MINUS_SRC_ALPHA;
            }
            break;
        case BLEND_MODE_COVERAGE:
            if (pge2dinfo->b_src_swap) {
                if (is_append_alpha(pge2dinfo)) {
                    blend_op.color_blending_src_factor = COLOR_FACTOR_ZERO;
                    blend_op.color_blending_dst_factor = COLOR_FACTOR_ONE;
                    blend_op.alpha_blending_src_factor = ALPHA_FACTOR_ONE;
                    blend_op.alpha_blending_dst_factor = ALPHA_FACTOR_ZERO;
                } else {
                    blend_op.color_blending_src_factor = COLOR_FACTOR_ONE_MINUS_DST_ALPHA;
                    blend_op.color_blending_dst_factor = COLOR_FACTOR_DST_ALPHA;
                    blend_op.alpha_blending_src_factor = ALPHA_FACTOR_ONE_MINUS_DST_ALPHA;
                    blend_op.alpha_blending_dst_factor = ALPHA_FACTOR_DST_ALPHA;
                }
            } else {
                if (is_append_alpha(pge2dinfo)) {
                    blend_op.color_blending_src_factor = COLOR_FACTOR_ONE;
                    blend_op.color_blending_dst_factor = COLOR_FACTOR_ZERO;
                    blend_op.alpha_blending_src_factor = ALPHA_FACTOR_ZERO;
                    blend_op.alpha_blending_dst_factor = ALPHA_FACTOR_ONE;
                } else {
                    blend_op.color_blending_src_factor = COLOR_FACTOR_SRC_ALPHA;
                    blend_op.color_blending_dst_factor = COLOR_FACTOR_ONE_MINUS_SRC_ALPHA;
                    blend_op.alpha_blending_src_factor = ALPHA_FACTOR_SRC_ALPHA;
                    blend_op.alpha_blending_dst_factor = ALPHA_FACTOR_ONE_MINUS_SRC_ALPHA;
                }
            }
            break;
        case BLEND_MODE_INVALID:
            return ge2d_fail;
    }

    op_ge2d_info.op = blendop(
            blend_op.color_blending_mode,
            blend_op.color_blending_src_factor,
            blend_op.color_blending_dst_factor,
            blend_op.alpha_blending_mode,
            blend_op.alpha_blending_src_factor,
            blend_op.alpha_blending_dst_factor);

    D_GE2D("ge2d_blend op_ge2d_info.op=%x\n",op_ge2d_info.op);

    if (!enqueue)
        ret = ioctl(fd, GE2D_BLEND, &op_ge2d_info);
    else
        ret = ioctl(fd, GE2D_BLEND_ENQUEUE, &op_ge2d_info);
    if (ret != 0) {
        E_GE2D("%s,%d,ret %d,ioctl failed!\n",__FUNCTION__,__LINE__, ret);
        return ge2d_fail;
    }
    return ge2d_success;
}


static int ge2d_blend_noalpha(int fd, aml_ge2d_info_t *pge2dinfo,
                              rectangle_t *srect, rectangle_t *srect2,
                              rectangle_t *drect, unsigned int op, int enqueue)
{
    int ret;
    ge2d_op_para_t op_ge2d_info;
    ge2d_blend_op blend_op;
    int max_d_w, max_d_h;
    D_GE2D("ge2d_blend srect[%d %d %d %d], s2rect[%d %d %d %d],drect[%d %d %d %d]\n",
        srect->x,srect->y,srect->w,srect->h,
        srect2->x,srect2->y,srect2->w,srect2->h,
        drect->x,drect->y,drect->w,drect->h);
    D_GE2D("ge2d_blend_noalpha:blend_mode=%d\n",op);
    memset(&blend_op,0,sizeof(ge2d_blend_op));
    #if 0
    max_d_w = (srect->w > srect2->w) ? srect2->w : srect->w;
    max_d_h = (srect->h > srect2->h) ? srect2->h : srect->h;
    if ((drect->w > max_d_w) || (drect->h > max_d_h)) {
        /* dst rect must be min(srect,srect2),otherwise ge2d will timeout */
        E_GE2D("dst rect w=%d,h=%d out of range\n",drect->w,drect->h);
        return ge2d_fail;
    }
    #endif
    op_ge2d_info.src1_rect.x = srect->x;
    op_ge2d_info.src1_rect.y = srect->y;
    op_ge2d_info.src1_rect.w = srect->w;
    op_ge2d_info.src1_rect.h = srect->h;

    op_ge2d_info.src2_rect.x = srect2->x;
    op_ge2d_info.src2_rect.y = srect2->y;
    op_ge2d_info.src2_rect.w = srect2->w;
    op_ge2d_info.src2_rect.h = srect2->h;

    op_ge2d_info.dst_rect.x = drect->x;
    op_ge2d_info.dst_rect.y = drect->y;
    op_ge2d_info.dst_rect.w = drect->w;
    op_ge2d_info.dst_rect.h = drect->h;

    blend_op.color_blending_mode = OPERATION_ADD;
    blend_op.alpha_blending_mode = OPERATION_ADD;
    /* b_src_swap = 1:src1 & src2 swap, so blend factor used dst instead src */
    switch (op) {
        case BLEND_MODE_NONE:
            if (pge2dinfo->b_src_swap) {
                blend_op.color_blending_src_factor = COLOR_FACTOR_ZERO;
                blend_op.color_blending_dst_factor = COLOR_FACTOR_ONE;
                blend_op.alpha_blending_src_factor = ALPHA_FACTOR_ZERO;
                blend_op.alpha_blending_dst_factor = ALPHA_FACTOR_ONE;
            }
            else {
                blend_op.color_blending_src_factor = COLOR_FACTOR_ONE;
                blend_op.color_blending_dst_factor = COLOR_FACTOR_ZERO;
                blend_op.alpha_blending_src_factor = ALPHA_FACTOR_ONE;
                blend_op.alpha_blending_dst_factor = ALPHA_FACTOR_ZERO;
            }
            break;
        case BLEND_MODE_PREMULTIPLIED:
            if (pge2dinfo->b_src_swap) {
                blend_op.color_blending_src_factor = COLOR_FACTOR_ONE_MINUS_DST_ALPHA;
                blend_op.color_blending_dst_factor = COLOR_FACTOR_ONE;
                blend_op.alpha_blending_src_factor = ALPHA_FACTOR_ONE_MINUS_DST_ALPHA;
                blend_op.alpha_blending_dst_factor = ALPHA_FACTOR_ONE;
            }
            else {
                blend_op.color_blending_src_factor = COLOR_FACTOR_ONE;
                blend_op.color_blending_dst_factor = COLOR_FACTOR_ONE_MINUS_SRC_ALPHA;
                blend_op.alpha_blending_src_factor = ALPHA_FACTOR_ONE;
                blend_op.alpha_blending_dst_factor = ALPHA_FACTOR_ONE_MINUS_SRC_ALPHA;
            }
            break;
        case BLEND_MODE_COVERAGE:
            if (pge2dinfo->b_src_swap) {
                blend_op.color_blending_src_factor = COLOR_FACTOR_ONE_MINUS_DST_ALPHA;
                blend_op.color_blending_dst_factor = COLOR_FACTOR_DST_ALPHA;
                blend_op.alpha_blending_src_factor = ALPHA_FACTOR_ONE_MINUS_SRC_ALPHA;
                blend_op.alpha_blending_dst_factor = ALPHA_FACTOR_DST_ALPHA;
            } else {
                blend_op.color_blending_src_factor = COLOR_FACTOR_SRC_ALPHA;
                blend_op.color_blending_dst_factor = COLOR_FACTOR_ONE_MINUS_SRC_ALPHA;
                blend_op.alpha_blending_src_factor = ALPHA_FACTOR_SRC_ALPHA;
                blend_op.alpha_blending_dst_factor = ALPHA_FACTOR_ONE_MINUS_SRC_ALPHA;
            }
            break;
        case BLEND_MODE_INVALID:
            return ge2d_fail;
    }


    op_ge2d_info.op = blendop(
            blend_op.color_blending_mode,
            blend_op.color_blending_src_factor,
            blend_op.color_blending_dst_factor,
            blend_op.alpha_blending_mode,
            blend_op.alpha_blending_src_factor,
            blend_op.alpha_blending_dst_factor);

    D_GE2D("ge2d_blend op_ge2d_info.op=%x\n",op_ge2d_info.op);

    if (!enqueue)
        ret = ioctl(fd, GE2D_BLEND_NOALPHA, &op_ge2d_info);
    else
        ret = ioctl(fd, GE2D_BLEND_NOALPHA_ENQUEUE, &op_ge2d_info);
    if (ret != 0) {
        E_GE2D("%s,%d,ret %d,ioctl failed!\n",__FUNCTION__,__LINE__, ret);
        return ge2d_fail;
    }
    return ge2d_success;
}

void ge2d_set_canvas_align(int cap_attr)
{
    /*
     *  if CANVAS_STATUS = 0, it means platform has canvas
     *  else if CANVAS_STATUS = 1, it means canvas only support one plane
     *  else CANVAS_STATUS = 2, it means canvas support mult planes
     */
    if (cap_attr & CANVAS_STATUS)
        has_canvas = 0;
    else
        has_canvas = 1;
}

int ge2d_get_cap(int fd)
{
    int ret = -1;
    int cap_mask = 0, cap_attr;
    ret = ioctl(fd, GE2D_GET_CAP, &cap_mask);
    if (ret != 0) {
        E_GE2D("%s,%d,ret %d,ioctl failed!\n",__FUNCTION__,__LINE__, ret);
        cap_attr = -1;
        return cap_attr;
    }
    cap_attr = cap_mask;
    ge2d_set_canvas_align(cap_attr);
    return cap_attr;
}

int ge2d_set_clut8_table(int fd, struct ge2d_clut8_t *clut8_table)
{
    int ret;
    ret = ioctl(fd, GE2D_SET_CLUT, clut8_table);
    if (ret < 0) {
        E_GE2D("ge2d set clut table fail\n");
        return ge2d_fail;
    }
    return ge2d_success;
}


int ge2d_open(void)
{
    int fd = -1;
    fd = open(FILE_NAME_GE2D, O_RDWR);
    if (fd < 0) {
        E_GE2D("open %s failed!error no %d\n",FILE_NAME_GE2D,errno);
    }

    D_GE2D("%s, ge2d_fd = %d\n", __func__, fd);

    return fd;
}

int ge2d_close(int fd)
{
    int ret = -1;
    ret = close(fd);
    if (ret < 0)
        return -errno;
    return ret;
}

void sync_dst_dmabuf_to_cpu(aml_ge2d_info_t *pge2dinfo)
{
    int dma_fd = -1;

    for (int i = 0; i < pge2dinfo->dst_info.plane_number; i++) {
        dma_fd = pge2dinfo->dst_info.shared_fd[i];
        if (dma_fd > 0)
                dmabuf_sync_for_cpu(pge2dinfo->ge2d_fd, dma_fd);
    }
}

void sync_src_dmabuf_to_device(aml_ge2d_info_t *pge2dinfo, int src_id)
{
    int dma_fd = -1;

    for (int i = 0; i < pge2dinfo->src_info[src_id].plane_number; i++) {
        dma_fd = pge2dinfo->src_info[src_id].shared_fd[i];
        if (dma_fd > 0)
            dmabuf_sync_for_device(pge2dinfo->ge2d_fd, dma_fd);
    }
}

/* if get_dst_op_number() > 1, use this interface
 * ge2d_process = ge2d_config + ge2d_execute
 */
int ge2d_process(int fd, aml_ge2d_info_t *pge2dinfo, int enqueue)
{
    rectangle_t dst_rect;
    int dx = 0, dy = 0;
    int ret = -1;
    int i;
    int op_number = 0;

    if (!pge2dinfo) {
        E_GE2D("pge2dinfo is NULL!\n");
        return ge2d_fail;
    }

    switch (pge2dinfo->ge2d_op) {
        case AML_GE2D_FILLRECTANGLE:
            dst_rect.w = pge2dinfo->dst_info.rect.w;
            dst_rect.h = pge2dinfo->dst_info.rect.h;
            dst_rect.x =  pge2dinfo->dst_info.rect.x;
            dst_rect.y = pge2dinfo->offset + pge2dinfo->dst_info.rect.y;

            ret = ge2d_fillrectangle_config_ex(fd,pge2dinfo);
            if (ret == ge2d_success)
                ge2d_fillrectangle(fd,&dst_rect,pge2dinfo->color, enqueue);
            break;
        case AML_GE2D_BLIT:
            if (!is_rect_valid(&pge2dinfo->src_info[0]))
                return ge2d_fail;
            if (!is_rect_valid(&pge2dinfo->dst_info))
                return ge2d_fail;

            dx = pge2dinfo->dst_info.rect.x;
            dy = pge2dinfo->offset + pge2dinfo->dst_info.rect.y;
            op_number = get_dst_op_number(pge2dinfo);
            for (i = 0; i < op_number; i++) {
                pge2dinfo->dst_op_cnt = i;
                ret = ge2d_blit_config_ex(fd,pge2dinfo);
                if (ret == ge2d_success) {
                    if (is_no_alpha(pge2dinfo->src_info[0].format))
                        ge2d_blit_noalpha(fd,pge2dinfo,&pge2dinfo->src_info[0].rect,dx,dy, enqueue);
                    else
                        ge2d_blit(fd,pge2dinfo,&pge2dinfo->src_info[0].rect,dx,dy, enqueue);
                }
            }
            break;
        case AML_GE2D_STRETCHBLIT:
            if (!is_rect_valid(&pge2dinfo->src_info[0]))
                return ge2d_fail;
            dst_rect.w = pge2dinfo->dst_info.rect.w;
            dst_rect.h = pge2dinfo->dst_info.rect.h;
            dst_rect.x =  pge2dinfo->dst_info.rect.x;
            dst_rect.y = pge2dinfo->offset + pge2dinfo->dst_info.rect.y;
            op_number = get_dst_op_number(pge2dinfo);

            for (i = 0; i < op_number; i++) {
                pge2dinfo->dst_op_cnt = i;
                ret = ge2d_blit_config_ex(fd,pge2dinfo);
                if (ret == ge2d_success) {

                    if (is_no_alpha(pge2dinfo->src_info[0].format))
                        ge2d_strechblit_noalpha(fd,pge2dinfo,&pge2dinfo->src_info[0].rect,&dst_rect, enqueue);
                    else
                        ge2d_strechblit(fd,pge2dinfo,&pge2dinfo->src_info[0].rect,&dst_rect, enqueue);

                }
            }
            break;
        case AML_GE2D_BLEND:
            if ((pge2dinfo->dst_info.memtype == CANVAS_OSD0) && (pge2dinfo->src_info[1].memtype == CANVAS_OSD0)) {
                memcpy(&pge2dinfo->src_info[1],&pge2dinfo->dst_info,sizeof(buffer_info_t));
                pge2dinfo->src_info[1].rect.y = pge2dinfo->offset + pge2dinfo->src_info[1].rect.y;
            }
            if (!is_rect_valid(&pge2dinfo->src_info[0]))
                return ge2d_fail;
            if (!is_rect_valid(&pge2dinfo->src_info[1]))
                return ge2d_fail;
            if (!is_rect_valid(&pge2dinfo->dst_info))
                return ge2d_fail;

            dst_rect.w = pge2dinfo->dst_info.rect.w;
            dst_rect.h = pge2dinfo->dst_info.rect.h;
            dst_rect.x =  pge2dinfo->dst_info.rect.x;
            dst_rect.y = pge2dinfo->offset + pge2dinfo->dst_info.rect.y;
            ret = ge2d_blend_config_ex(fd,pge2dinfo);
            if (ret == ge2d_success) {
                if (is_not_append_alpha(pge2dinfo)
                    && ((is_no_alpha(pge2dinfo->src_info[0].format))
                    || (is_no_alpha(pge2dinfo->src_info[1].format))
                    || (pge2dinfo->src_info[0].layer_mode == LAYER_MODE_NON))) {
                    if (pge2dinfo->b_src_swap)
                        ge2d_blend_noalpha(fd,pge2dinfo,&(pge2dinfo->src_info[1].rect),
                            &(pge2dinfo->src_info[0].rect),
                            &dst_rect,pge2dinfo->blend_mode, enqueue);
                    else
                        ge2d_blend_noalpha(fd,pge2dinfo,&(pge2dinfo->src_info[0].rect),
                            &(pge2dinfo->src_info[1].rect),
                            &dst_rect,pge2dinfo->blend_mode, enqueue);
                } else {
                    if (pge2dinfo->b_src_swap)
                        ge2d_blend(fd,pge2dinfo,&(pge2dinfo->src_info[1].rect),
                            &(pge2dinfo->src_info[0].rect),
                            &dst_rect,pge2dinfo->blend_mode, enqueue);
                    else
                        ge2d_blend(fd,pge2dinfo,&(pge2dinfo->src_info[0].rect),
                            &(pge2dinfo->src_info[1].rect),
                            &dst_rect,pge2dinfo->blend_mode, enqueue);
                }
            }
            break;
        default:
            E_GE2D("ge2d(%d) opration not support!\n",pge2dinfo->ge2d_op);
            return ge2d_fail;
    }

    return ge2d_success;
}

int ge2d_post_queue(int fd)
{
    int ret = -1;

    ret = ioctl(fd, GE2D_POST_QUEUE, NULL);
    if (ret < 0) {
        E_GE2D("%s, attatch failed\n", __func__);
        return ge2d_fail;
    }
}

int ge2d_attach_dma_fd(int fd, aml_ge2d_info_t *pge2dinfo,
                       enum ge2d_data_type_e data_type)
{
    struct ge2d_dmabuf_attach_s attach;
    int i, ret = -1;
    int *shared_fd = NULL;

    switch (data_type) {
    case AML_GE2D_SRC:
        shared_fd = pge2dinfo->src_info[0].shared_fd;
        break;
    case AML_GE2D_SRC2:
        shared_fd = pge2dinfo->src_info[1].shared_fd;
        break;
    case AML_GE2D_DST:
        shared_fd = pge2dinfo->dst_info.shared_fd;
        break;
    }
    if (shared_fd == NULL) {
        E_GE2D("%s, data_type error\n", __func__);
        return ge2d_fail;
    }
    memcpy(attach.dma_fd, shared_fd, sizeof(int) * GE2D_MAX_PLANE);
    attach.data_type = data_type;

    ret = ioctl(fd, GE2D_ATTACH_DMA_FD, &attach);
    if (ret < 0) {
        E_GE2D("%s, attatch failed\n", __func__);
        return ge2d_fail;
    }

    /* shared_fd have been attatched using ge2d_attach_dma_fd()
     * reset them to DMA_FD_ATTACHED
     */
    switch (data_type) {
    case AML_GE2D_SRC:
        for (i = 0; i < pge2dinfo->src_info[0].plane_number; i++)
            pge2dinfo->src_info[0].shared_fd[i] = DMA_FD_ATTACHED;
        break;
    case AML_GE2D_SRC2:
        for (i = 0; i < pge2dinfo->src_info[1].plane_number; i++)
            pge2dinfo->src_info[1].shared_fd[i] = DMA_FD_ATTACHED;
        break;
    case AML_GE2D_DST:
        for (i = 0; i < pge2dinfo->dst_info.plane_number; i++)
            pge2dinfo->dst_info.shared_fd[i] = DMA_FD_ATTACHED;
        break;
    }

    return ge2d_success;
}

void ge2d_detach_dma_fd(int fd, enum ge2d_data_type_e data_type)
{
    int ret = -1;

    ret = ioctl(fd, GE2D_DETACH_DMA_FD, &data_type);
    if (ret < 0) {
        E_GE2D("%s, detatch failed\n", __func__);
        return;
    }
}

int ge2d_config(int fd,aml_ge2d_info_t *pge2dinfo)
{
    int ret = -1, i;

    if (!pge2dinfo) {
        E_GE2D("pge2dinfo is NULL!\n");
        return ge2d_fail;
    }

    switch (pge2dinfo->ge2d_op) {
        case AML_GE2D_FILLRECTANGLE:
            ret = ge2d_fillrectangle_config_ex(fd, pge2dinfo);
            break;
        case AML_GE2D_BLIT:
            if (!is_rect_valid(&pge2dinfo->src_info[0]))
                return ge2d_fail;
            if (!is_rect_valid(&pge2dinfo->dst_info))
                return ge2d_fail;

            pge2dinfo->dst_op_cnt = 0;
            ret = ge2d_blit_config_ex(fd, pge2dinfo);
            break;
        case AML_GE2D_STRETCHBLIT:
            if (!is_rect_valid(&pge2dinfo->src_info[0]))
                return ge2d_fail;

            pge2dinfo->dst_op_cnt = 0;
            ret = ge2d_blit_config_ex(fd, pge2dinfo);
            break;
        case AML_GE2D_BLEND:
            if ((pge2dinfo->dst_info.memtype == CANVAS_OSD0) && (pge2dinfo->src_info[1].memtype == CANVAS_OSD0)) {
                memcpy(&pge2dinfo->src_info[1],&pge2dinfo->dst_info,sizeof(buffer_info_t));
                pge2dinfo->src_info[1].rect.y = pge2dinfo->offset + pge2dinfo->src_info[1].rect.y;
            }
            if (!is_rect_valid(&pge2dinfo->src_info[0]))
                return ge2d_fail;
            if (!is_rect_valid(&pge2dinfo->src_info[1]))
                return ge2d_fail;
            if (!is_rect_valid(&pge2dinfo->dst_info))
                return ge2d_fail;

            ret = ge2d_blend_config_ex(fd, pge2dinfo);
            break;
        default:
            E_GE2D("ge2d(%d) config not support!\n",pge2dinfo->ge2d_op);
            return ge2d_fail;
    }

    return ge2d_success;
}

int ge2d_execute(int fd,aml_ge2d_info_t *pge2dinfo, int enqueue)
{
    rectangle_t dst_rect;
    int dx = 0, dy = 0;
    int ret = -1;

    if (!pge2dinfo) {
        E_GE2D("pge2dinfo is NULL!\n");
        return ge2d_fail;
    }

    switch (pge2dinfo->ge2d_op) {
        case AML_GE2D_FILLRECTANGLE:
            dst_rect.w = pge2dinfo->dst_info.rect.w;
            dst_rect.h = pge2dinfo->dst_info.rect.h;
            dst_rect.x =  pge2dinfo->dst_info.rect.x;
            dst_rect.y = pge2dinfo->offset + pge2dinfo->dst_info.rect.y;

            ret = ge2d_fillrectangle(fd,&dst_rect,pge2dinfo->color, enqueue);
            break;
        case AML_GE2D_BLIT:
            dx = pge2dinfo->dst_info.rect.x;
            dy = pge2dinfo->offset + pge2dinfo->dst_info.rect.y;
            pge2dinfo->dst_op_cnt = 0;
            if (is_no_alpha(pge2dinfo->src_info[0].format))
                ret = ge2d_blit_noalpha(fd,pge2dinfo,&pge2dinfo->src_info[0].rect,dx,dy, enqueue);
            else
                ret = ge2d_blit(fd,pge2dinfo,&pge2dinfo->src_info[0].rect,dx,dy, enqueue);
            break;
        case AML_GE2D_STRETCHBLIT:
            dst_rect.w = pge2dinfo->dst_info.rect.w;
            dst_rect.h = pge2dinfo->dst_info.rect.h;
            dst_rect.x =  pge2dinfo->dst_info.rect.x;
            dst_rect.y = pge2dinfo->offset + pge2dinfo->dst_info.rect.y;

            pge2dinfo->dst_op_cnt = 0;
            if (is_no_alpha(pge2dinfo->src_info[0].format))
                ret = ge2d_strechblit_noalpha(fd,pge2dinfo,&pge2dinfo->src_info[0].rect,&dst_rect, enqueue);
            else
                ret = ge2d_strechblit(fd,pge2dinfo,&pge2dinfo->src_info[0].rect,&dst_rect, enqueue);

            break;
        case AML_GE2D_BLEND:
            dst_rect.w = pge2dinfo->dst_info.rect.w;
            dst_rect.h = pge2dinfo->dst_info.rect.h;
            dst_rect.x =  pge2dinfo->dst_info.rect.x;
            dst_rect.y = pge2dinfo->offset + pge2dinfo->dst_info.rect.y;

            if (is_not_append_alpha(pge2dinfo)
                && ((is_no_alpha(pge2dinfo->src_info[0].format))
                || (is_no_alpha(pge2dinfo->src_info[1].format))
                || (pge2dinfo->src_info[0].layer_mode == LAYER_MODE_NON))) {
                if (pge2dinfo->b_src_swap)
                    ret = ge2d_blend_noalpha(fd,pge2dinfo,&(pge2dinfo->src_info[1].rect),
                        &(pge2dinfo->src_info[0].rect),
                        &dst_rect,pge2dinfo->blend_mode, enqueue);
                else
                    ret = ge2d_blend_noalpha(fd,pge2dinfo,&(pge2dinfo->src_info[0].rect),
                        &(pge2dinfo->src_info[1].rect),
                        &dst_rect,pge2dinfo->blend_mode, enqueue);
            } else {
                if (pge2dinfo->b_src_swap)
                    ret = ge2d_blend(fd,pge2dinfo,&(pge2dinfo->src_info[1].rect),
                        &(pge2dinfo->src_info[0].rect),
                        &dst_rect,pge2dinfo->blend_mode, enqueue);
                else
                    ret = ge2d_blend(fd,pge2dinfo,&(pge2dinfo->src_info[0].rect),
                        &(pge2dinfo->src_info[1].rect),
                        &dst_rect,pge2dinfo->blend_mode, enqueue);
            }
            break;
        default:
            E_GE2D("ge2d(%d) opration not support!\n",pge2dinfo->ge2d_op);
            return ge2d_fail;
    }

    return ge2d_success;
}

int ge2d_process_ion(int fd,aml_ge2d_info_t *pge2dinfo)
{
    rectangle_t src_rect[2];
    rectangle_t dst_rect;
    int dx = 0, dy = 0;
    int ret = -1, dma_fd = -1;
    int i;
    int op_number = 0;

    if (!pge2dinfo) {
        E_GE2D("pge2dinfo is NULL!\n");
        return ge2d_fail;
    }

    switch (pge2dinfo->ge2d_op) {
        case AML_GE2D_FILLRECTANGLE:
            dst_rect.w = pge2dinfo->dst_info.rect.w;
            dst_rect.h = pge2dinfo->dst_info.rect.h;
            dst_rect.x =  pge2dinfo->dst_info.rect.x;
            dst_rect.y = pge2dinfo->offset + pge2dinfo->dst_info.rect.y;
            ret = ge2d_fillrectangle_config_ex_ion(fd,pge2dinfo);
            if (ret == ge2d_success)
                ge2d_fillrectangle(fd,&dst_rect,pge2dinfo->color, 0);
            break;
        case AML_GE2D_BLIT:
            if (!is_rect_valid(&pge2dinfo->src_info[0]))
                return ge2d_fail;
            if (!is_rect_valid(&pge2dinfo->dst_info))
                return ge2d_fail;

            dx = pge2dinfo->dst_info.rect.x;
            dy = pge2dinfo->offset + pge2dinfo->dst_info.rect.y;
            op_number = get_dst_op_number(pge2dinfo);
            for (i = 0; i < op_number; i++) {
                pge2dinfo->dst_op_cnt = i;
                ret = ge2d_blit_config_ex_ion(fd,pge2dinfo);
                if (ret == ge2d_success) {
                    if (is_no_alpha(pge2dinfo->src_info[0].format))
                        ret = ge2d_blit_noalpha(fd,pge2dinfo,&pge2dinfo->src_info[0].rect,dx,dy, 0);
                    else
                        ret = ge2d_blit(fd,pge2dinfo,&pge2dinfo->src_info[0].rect,dx,dy, 0);
                }
            }
            break;
        case AML_GE2D_STRETCHBLIT:
            if (!is_rect_valid(&pge2dinfo->src_info[0]))
                return ge2d_fail;
            dst_rect.w = pge2dinfo->dst_info.rect.w;
            dst_rect.h = pge2dinfo->dst_info.rect.h;
            dst_rect.x =  pge2dinfo->dst_info.rect.x;
            dst_rect.y = pge2dinfo->offset + pge2dinfo->dst_info.rect.y;
            op_number = get_dst_op_number(pge2dinfo);

            for (i = 0; i < op_number; i++) {
                pge2dinfo->dst_op_cnt = i;
                ret = ge2d_blit_config_ex_ion(fd,pge2dinfo);
                if (ret == ge2d_success) {

                    if (is_no_alpha(pge2dinfo->src_info[0].format))
                        ret = ge2d_strechblit_noalpha(fd,pge2dinfo,&pge2dinfo->src_info[0].rect,&dst_rect, 0);
                    else
                        ret = ge2d_strechblit(fd,pge2dinfo,&pge2dinfo->src_info[0].rect,&dst_rect, 0);

                }
            }
            break;
        case AML_GE2D_BLEND:
            if ((pge2dinfo->dst_info.memtype == CANVAS_OSD0) && (pge2dinfo->src_info[1].memtype == CANVAS_OSD0)) {
                memcpy(&pge2dinfo->src_info[1],&pge2dinfo->dst_info,sizeof(buffer_info_t));
                pge2dinfo->src_info[1].rect.y = pge2dinfo->offset + pge2dinfo->src_info[1].rect.y;
            }
            if (!is_rect_valid(&pge2dinfo->src_info[0]))
                return ge2d_fail;
            if (!is_rect_valid(&pge2dinfo->src_info[1]))
                return ge2d_fail;
            if (!is_rect_valid(&pge2dinfo->dst_info))
                return ge2d_fail;

            dst_rect.w = pge2dinfo->dst_info.rect.w;
            dst_rect.h = pge2dinfo->dst_info.rect.h;
            dst_rect.x =  pge2dinfo->dst_info.rect.x;
            dst_rect.y = pge2dinfo->offset + pge2dinfo->dst_info.rect.y;
            ret = ge2d_blend_config_ex_ion(fd,pge2dinfo);
            if (ret == ge2d_success) {
                if (is_not_append_alpha(pge2dinfo)
                    && ((is_no_alpha(pge2dinfo->src_info[0].format))
                    || (is_no_alpha(pge2dinfo->src_info[1].format))
                    || (pge2dinfo->src_info[0].layer_mode == LAYER_MODE_NON))) {
                    if (pge2dinfo->b_src_swap)
                        ret = ge2d_blend_noalpha(fd,pge2dinfo,&(pge2dinfo->src_info[1].rect),
                            &(pge2dinfo->src_info[0].rect),
                            &dst_rect,pge2dinfo->blend_mode, 0);
                    else
                        ret = ge2d_blend_noalpha(fd,pge2dinfo,&(pge2dinfo->src_info[0].rect),
                            &(pge2dinfo->src_info[1].rect),
                            &dst_rect,pge2dinfo->blend_mode, 0);
                } else {
                    if (pge2dinfo->b_src_swap)
                        ret = ge2d_blend(fd,pge2dinfo,&(pge2dinfo->src_info[1].rect),
                            &(pge2dinfo->src_info[0].rect),
                            &dst_rect,pge2dinfo->blend_mode, 0);
                    else
                        ret = ge2d_blend(fd,pge2dinfo,&(pge2dinfo->src_info[0].rect),
                            &(pge2dinfo->src_info[1].rect),
                            &dst_rect,pge2dinfo->blend_mode, 0);
                }
            }
            break;
        default:
            E_GE2D("ge2d(%d) opration not support!\n",pge2dinfo->ge2d_op);
            return ge2d_fail;
    }

    return ret;
}

