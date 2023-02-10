/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <asm/types.h>
#include <string.h>
#include <errno.h>
#include "include/ge2d_port.h"
#include "include/ge2d_com.h"
#include "include/dmabuf.h"
#include "kernel-headers/linux/dma-direction.h"
#include "kernel-headers/linux/ge2d.h"
#include "kernel-headers/linux/dma-heap.h"

#define PAGE_ALIGN(x)        (((x) + 4095) & ~4095)

int dmabuf_heap_open(char *name)
{
    int ret, fd;
    char buf[256];

    ret = snprintf(buf, 256, "%s/%s", DEVPATH, name);
    if (ret < 0) {
        E_GE2D("snprintf failed!\n");
        return ret;
    }

    fd = open(buf, O_RDWR);
    /* if errno == 2, which means there is no heap_codecmm, using ion replace
     */
    if (fd < 0) {
        if (errno != 2)
            E_GE2D("open %s failed, %s!\n", buf, strerror(errno));
    }
    return fd;
}

static int dmabuf_heap_alloc_fdflags(int fd, size_t len, unsigned int fd_flags,
                     unsigned int heap_flags, int *dmabuf_fd)
{
    struct dma_heap_allocation_data data = {
        .len = len,
        .fd = 0,
        .fd_flags = fd_flags,
        .heap_flags = heap_flags,
    };
    int ret;

    if (!dmabuf_fd)
        return -EINVAL;

    ret = ioctl(fd, DMA_HEAP_IOCTL_ALLOC, &data);
    if (ret < 0)
        return ret;
    *dmabuf_fd = (int)data.fd;
    return ret;
}

int dmabuf_heap_alloc(int fd, size_t len, unsigned int flags,
			     int *dmabuf_fd)
{
    return dmabuf_heap_alloc_fdflags(fd, len, O_RDWR | O_CLOEXEC, flags,
					 dmabuf_fd);
}

void dmabuf_sync(int fd, int start_stop)
{
    struct dma_buf_sync sync = {
        .flags = start_stop | DMA_BUF_SYNC_RW,
    };
    int ret;

    ret = ioctl(fd, DMA_BUF_IOCTL_SYNC, &sync);
    if (ret)
        E_GE2D("sync failed %d\n", errno);
}

void dmabuf_heap_close(int heap_fd)
{
    if (heap_fd >= 0)
        close(heap_fd);
}

static int ge2d_alloc_dma_buffer(int ge2d_fd, unsigned int dir, unsigned int len)
{
    struct ge2d_dmabuf_req_s buf_cfg;

    memset(&buf_cfg, 0, sizeof(buf_cfg));

    buf_cfg.dma_dir = dir;
    buf_cfg.len = len;

    if (ioctl(ge2d_fd, GE2D_REQUEST_BUFF, &buf_cfg)) {
        E_GE2D("failed alloc dma buffer\n");
        return -1;
    }
    E_GE2D("dma buffer alloc, index=%d\n", buf_cfg.index);
    return buf_cfg.index;
}

static int ge2d_get_dma_buffer_fd(int ge2d_fd, int index)
{
    struct ge2d_dmabuf_exp_s ex_buf;

    ex_buf.index = index;
    ex_buf.flags = O_RDWR;
    ex_buf.fd = -1;

    if (ioctl(ge2d_fd, GE2D_EXP_BUFF, &ex_buf))  {
        E_GE2D("failed get dma buf fd\n");
        return -1;
    }
    printf("dma buffer export, fd=%d\n", ex_buf.fd);

    return ex_buf.fd;
}

static int ge2d_free_dma_buffer(int ge2d_fd, int index)
{
    if (ioctl(ge2d_fd, GE2D_FREE_BUFF, &index))  {
        E_GE2D("failed free dma buf fd\n");
        return -1;
    }
    return 0;
}

int dmabuf_alloc(int ge2d_fd, int type, unsigned int len)
{
    int index = -1;
    int dma_fd = -1;
    unsigned int dir;

    /* alloc dma*/
    if (type == GE2D_BUF_OUTPUT)
        dir = DMA_FROM_DEVICE;
    else
        dir = DMA_TO_DEVICE;

    index = ge2d_alloc_dma_buffer(ge2d_fd, dir, len);
    if (index < 0)
        return -1;

    /* get  dma fd*/
    dma_fd = ge2d_get_dma_buffer_fd(ge2d_fd, index);
    if (dma_fd < 0)
    return -1;
    /* after alloc, dmabuffer free can be called, it just dec refcount */
    ge2d_free_dma_buffer(ge2d_fd, index);

    return dma_fd;
}

int dmabuf_sync_for_device(int ge2d_fd, int dma_fd)
{
    if (ioctl(ge2d_fd, GE2D_SYNC_DEVICE, &dma_fd))  {
        E_GE2D("failed GE2D_SYNC_DEVICE\n");
        return -1;
    }
    return 0;
}

int dmabuf_sync_for_cpu(int ge2d_fd, int dma_fd)
{
    if (ioctl(ge2d_fd, GE2D_SYNC_CPU, &dma_fd))  {
        E_GE2D("failed GE2D_SYNC_CPU\n");
        return -1;
    }
    return 0;
}