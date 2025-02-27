/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef AML_GE2D_H_
#define AML_GE2D_H_
#include "ge2d_port.h"

#if defined (__cplusplus)
extern "C" {
#endif


typedef struct aml_ge2d {
    aml_ge2d_info_t ge2dinfo;
    char *src_data[GE2D_MAX_PLANE];
    char *src2_data[GE2D_MAX_PLANE];
    char *dst_data[GE2D_MAX_PLANE];
    unsigned int src_size[GE2D_MAX_PLANE];
    unsigned int src2_size[GE2D_MAX_PLANE];
    unsigned int dst_size[GE2D_MAX_PLANE];
    void *cmemParm_src[GE2D_MAX_PLANE];
    void *cmemParm_src2[GE2D_MAX_PLANE];
    void *cmemParm_dst[GE2D_MAX_PLANE];
} aml_ge2d_t;

int aml_ge2d_init(aml_ge2d_t *pge2d);
void aml_ge2d_exit(aml_ge2d_t *pge2d);

int aml_ge2d_get_cap(int fd_ge2d);

int aml_ge2d_mem_alloc_ion(aml_ge2d_t *pge2d);
void aml_ge2d_mem_free_ion(aml_ge2d_t *pge2d);
/* support both ion & dma_buf */
int aml_ge2d_mem_alloc(aml_ge2d_t *pge2d);
void aml_ge2d_mem_free(aml_ge2d_t *pge2d);
int aml_ge2d_process(aml_ge2d_info_t *pge2dinfo);
int aml_ge2d_process_enqueue(aml_ge2d_info_t *pge2dinfo);
int aml_ge2d_post_queue(aml_ge2d_info_t *pge2dinfo);

int aml_ge2d_attach_dma_fd(aml_ge2d_info_t *pge2dinfo,
			   enum ge2d_data_type_e data_type);
int aml_ge2d_config(aml_ge2d_info_t *pge2dinfo);
int aml_ge2d_execute(aml_ge2d_info_t *pge2dinfo);
int aml_ge2d_execute_enqueue(aml_ge2d_info_t *pge2dinfo);
void aml_ge2d_detach_dma_fd(aml_ge2d_info_t *pge2dinfo,
			    enum ge2d_data_type_e data_type);
int aml_ge2d_process_ion(aml_ge2d_info_t *pge2dinfo);
int  aml_ge2d_sync_cache(aml_ge2d_info_t *pge2dinfo, int src_id);
int  aml_ge2d_invalid_cache(aml_ge2d_info_t *pge2dinfo);
void aml_ge2d_sync_for_device(aml_ge2d_info_t *pge2dinfo, int src_id);
void aml_ge2d_sync_for_cpu(aml_ge2d_info_t *pge2dinfo);
#if defined (__cplusplus)
}
#endif

#endif

