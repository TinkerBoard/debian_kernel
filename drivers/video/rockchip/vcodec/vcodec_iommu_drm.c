/*
 * Copyright (C) 2016 Fuzhou Rockchip Electronics Co., Ltd
 * author: Jung Zhao jung.zhao@rock-chips.com
 *         Randy Li, randy.li@rock-chips.com
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/dma-iommu.h>

#include <linux/dma-buf.h>
#include <drm/drmP.h>
#include <drm/drm_atomic.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_sync_helper.h>
#include <drm/rockchip_drm.h>
#include <linux/dma-mapping.h>
#include <linux/rockchip-iovmm.h>
#include <linux/pm_runtime.h>
#include <linux/memblock.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_graph.h>
#include <linux/component.h>
#include <linux/fence.h>
#include <linux/console.h>
#include <linux/kref.h>
#include <linux/fdtable.h>

#include "vcodec_iommu_ops.h"

struct vcodec_drm_buffer {
	struct list_head buffer;
	struct dma_buf *dma_buf;
	union {
		unsigned long iova;
		unsigned long phys;
	};
	u8 *cpu_addr;
	unsigned long size;
	int fd;
	int index;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	struct page **pages;
	struct kref ref;
};

struct vcodec_iommu_drm_info {
	struct iommu_domain *domain;
};

static void vcodec_drm_dump_fd_refs(struct vcodec_iommu_info *iommu_info,
				    int fd,
				    const char *caller)
{
	struct files_struct *files = current->files;
	struct file *file;

	if (!files)
		return;

	rcu_read_lock();
	file = fcheck_files(files, fd);
	if (file)
		vpu_iommu_debug(iommu_info->debug_level, DEBUG_IOMMU_OPS_DUMP,
				"caller %s fd %d ref is %d\n", caller, fd,
				get_file_rcu(file));
	else
		vpu_iommu_debug(iommu_info->debug_level, DEBUG_IOMMU_OPS_DUMP,
				"caller %s can not find fd %d\n", caller, fd);
	rcu_read_unlock();
}

static struct vcodec_drm_buffer *
vcodec_drm_get_buffer(struct vcodec_iommu_info *iommu_info, int idx)
{
	struct vcodec_drm_buffer *drm_buffer = NULL, *n;

	list_for_each_entry_safe(drm_buffer, n, &iommu_info->buffer_list,
				 buffer) {
		if (drm_buffer->index == idx)
			return drm_buffer;
	}

	return NULL;
}

static void vcodec_drm_detach(struct vcodec_iommu_info *iommu_info)
{
	struct vcodec_iommu_drm_info *drm_info = iommu_info->private;
	struct device *dev = iommu_info->dev;
	struct iommu_domain *domain = drm_info->domain;

	/*
	 * FIXME we should free it first, but it would make kernel crash
	 * down.
	 */
	dev->dma_parms = NULL;

	iommu_detach_device(domain, dev);
}

static int vcodec_drm_attach(struct vcodec_iommu_info *iommu_info)
{
	struct vcodec_iommu_drm_info *drm_info = iommu_info->private;
	struct device *dev = iommu_info->dev;
	struct iommu_domain *domain = drm_info->domain;
	int ret;

	if (!dev->dma_parms)
		dev->dma_parms = devm_kzalloc
			(dev, sizeof(*dev->dma_parms), GFP_KERNEL);
	if (!dev->dma_parms)
		return -ENOMEM;

	ret = dma_set_coherent_mask(dev, DMA_BIT_MASK(32));
	if (ret)
		return ret;

	dma_set_max_seg_size(dev, DMA_BIT_MASK(32));
	ret = iommu_attach_device(domain, dev);
	if (ret) {
		dev_err(dev, "Failed to attach iommu device\n");
		return ret;
	}

	if (!common_iommu_setup_dma_ops(dev, 0x10000000, SZ_2G, domain->ops)) {
		dev_err(dev, "Failed to set dma_ops\n");
		iommu_detach_device(domain, dev);
		ret = -ENODEV;
	}

	return ret;
}

static u8 *vcodec_drm_sgt_map_kernel(struct vcodec_drm_buffer *drm_buffer)
{
	struct scatterlist *sgl, *sg;
	int nr_pages = PAGE_ALIGN(drm_buffer->size) >> PAGE_SHIFT;
	int i = 0, j = 0, k = 0;
	struct page *page;

	drm_buffer->pages = kmalloc_array(nr_pages, sizeof(*drm_buffer->pages),
					  GFP_KERNEL);
	if (!(drm_buffer->pages)) {
		pr_err("can not alloc pages\n");

		return NULL;
	}

	sgl = drm_buffer->sgt->sgl;

	for_each_sg(sgl, sg, drm_buffer->sgt->nents, i) {
		page = sg_page(sg);
		for (j = 0; j < sg->length / PAGE_SIZE; j++)
			drm_buffer->pages[k++] = page++;
	}

	return vmap(drm_buffer->pages, nr_pages, VM_MAP, PAGE_KERNEL);
}

static void vcodec_drm_sgt_unmap_kernel(struct vcodec_drm_buffer *drm_buffer)
{
	vunmap(drm_buffer->cpu_addr);
	kfree(drm_buffer->pages);
}

static void vcodec_drm_clear_map(struct kref *ref)
{
	struct vcodec_drm_buffer *drm_buffer =
		container_of(ref, struct vcodec_drm_buffer, ref);

	if (drm_buffer->cpu_addr) {
		vcodec_drm_sgt_unmap_kernel(drm_buffer);
		drm_buffer->cpu_addr = NULL;
	}

	if (drm_buffer->attach) {
		dma_buf_unmap_attachment(drm_buffer->attach, drm_buffer->sgt,
					 DMA_BIDIRECTIONAL);
		dma_buf_detach(drm_buffer->dma_buf, drm_buffer->attach);
		dma_buf_put(drm_buffer->dma_buf);
		drm_buffer->attach = NULL;
	}
}

static void vcdoec_drm_dump_info(struct vcodec_iommu_info *iommu_info)
{
	struct vcodec_drm_buffer *drm_buffer = NULL, *n;

	vpu_iommu_debug(iommu_info->debug_level, DEBUG_IOMMU_OPS_DUMP,
			"still there are below buffers stored in list\n");
	list_for_each_entry_safe(drm_buffer, n, &iommu_info->buffer_list,
				 buffer) {
		vpu_iommu_debug(iommu_info->debug_level, DEBUG_IOMMU_OPS_DUMP,
				"index %d drm_buffer fd %d cpu_addr %p\n",
				drm_buffer->index,
				drm_buffer->fd, drm_buffer->cpu_addr);
	}
}

static int vcodec_drm_free(struct vcodec_iommu_info *iommu_info, int idx)
{
	/* please double-check all maps have been release */
	struct vcodec_drm_buffer *drm_buffer;

	mutex_lock(&iommu_info->list_mutex);
	drm_buffer = vcodec_drm_get_buffer(iommu_info, idx);

	if (!drm_buffer) {
		pr_err("%s can not find %d buffer in list\n", __func__, idx);
		mutex_unlock(&iommu_info->list_mutex);

		return -EINVAL;
	}

	if (atomic_read(&drm_buffer->ref.refcount) == 0) {
		dma_buf_put(drm_buffer->dma_buf);
		list_del_init(&drm_buffer->buffer);
		vcodec_drm_dump_fd_refs(iommu_info, drm_buffer->fd, __func__);
		kfree(drm_buffer);
	}
	mutex_unlock(&iommu_info->list_mutex);

	return 0;
}

static int vcodec_drm_unmap_iommu(struct vcodec_iommu_info *iommu_info, int idx)
{
	struct vcodec_drm_buffer *drm_buffer;

	/* Force to flush iommu table */
	rockchip_iovmm_invalidate_tlb(iommu_info->mmu_dev);

	mutex_lock(&iommu_info->list_mutex);
	drm_buffer = vcodec_drm_get_buffer(iommu_info, idx);
	mutex_unlock(&iommu_info->list_mutex);

	if (!drm_buffer) {
		pr_err("%s can not find %d buffer in list\n", __func__, idx);
		return -EINVAL;
	}

	kref_put(&drm_buffer->ref, vcodec_drm_clear_map);

	return 0;
}

static int vcodec_drm_map_iommu(struct vcodec_iommu_info *iommu_info, int idx,
				unsigned long *iova, unsigned long *size)
{
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	struct vcodec_drm_buffer *drm_buffer;
	struct device *dev = iommu_info->dev;
	int ret = 0;

	/* Force to flush iommu table */
	rockchip_iovmm_invalidate_tlb(iommu_info->mmu_dev);

	mutex_lock(&iommu_info->list_mutex);
	drm_buffer = vcodec_drm_get_buffer(iommu_info, idx);
	mutex_unlock(&iommu_info->list_mutex);

	if (!drm_buffer) {
		pr_err("%s can not find %d buffer in list\n", __func__, idx);
		return -EINVAL;
	}

	if (drm_buffer->attach) {
		atomic_inc_return(&drm_buffer->ref.refcount);
		if (iova)
			*iova = drm_buffer->iova;
		if (size)
			*size = drm_buffer->size;
		return 0;
	}

	attach = dma_buf_attach(drm_buffer->dma_buf, dev);
	if (IS_ERR(attach))
		return -ENOMEM;

	get_dma_buf(drm_buffer->dma_buf);

	sgt = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(sgt)) {
		ret = -ENOMEM;
		goto fail_detach;
	}

	drm_buffer->iova = sg_dma_address(sgt->sgl);
	if (iova)
		*iova = drm_buffer->iova;

	drm_buffer->size = drm_buffer->dma_buf->size;
	if (size)
		*size = drm_buffer->size;
	drm_buffer->attach = attach;
	drm_buffer->sgt = sgt;

	atomic_inc_return(&drm_buffer->ref.refcount);

	return 0;

fail_detach:
	dma_buf_detach(drm_buffer->dma_buf, attach);
	dma_buf_put(drm_buffer->dma_buf);

	return ret;
}

static int vcodec_drm_unmap_kernel(struct vcodec_iommu_info *iommu_info,
				   int idx)
{
	struct vcodec_drm_buffer *drm_buffer;

	mutex_lock(&iommu_info->list_mutex);
	drm_buffer = vcodec_drm_get_buffer(iommu_info, idx);
	mutex_unlock(&iommu_info->list_mutex);

	if (!drm_buffer) {
		pr_err("%s can not find %d buffer in list\n", __func__, idx);

		return -EINVAL;
	}

	kref_put(&drm_buffer->ref, vcodec_drm_clear_map);
	return 0;
}

static u8 *vcodec_drm_map_kernel(struct vcodec_iommu_info *iommu_info, int idx)
{
	struct vcodec_drm_buffer *drm_buffer;

	mutex_lock(&iommu_info->list_mutex);
	drm_buffer = vcodec_drm_get_buffer(iommu_info, idx);
	mutex_unlock(&iommu_info->list_mutex);

	if (!drm_buffer) {
		pr_err("can not find %d buffer in list\n", idx);
		return NULL;
	}

	atomic_inc_return(&drm_buffer->ref.refcount);
	if  (!drm_buffer->attach) {
		vcodec_drm_map_iommu(iommu_info, idx, NULL, NULL);
		kref_put(&drm_buffer->ref, vcodec_drm_clear_map);
	}

	if (!drm_buffer->cpu_addr) {
		drm_buffer->cpu_addr =
			vcodec_drm_sgt_map_kernel(drm_buffer);
	}

	return drm_buffer->cpu_addr;
}

static int vcodec_drm_import(struct vcodec_iommu_info *iommu_info, int fd)
{
	struct vcodec_drm_buffer *drm_buffer = NULL, *n;

	mutex_lock(&iommu_info->list_mutex);
	list_for_each_entry_safe(drm_buffer, n,
				 &iommu_info->buffer_list, buffer) {
		if (drm_buffer->fd == fd) {
			mutex_unlock(&iommu_info->list_mutex);
			return drm_buffer->index;
		}
	}
	mutex_unlock(&iommu_info->list_mutex);

	drm_buffer = kzalloc(sizeof(*drm_buffer), GFP_KERNEL);
	if (!drm_buffer)
		return 0;

	vcodec_drm_dump_fd_refs(iommu_info, fd, __func__);
	drm_buffer->dma_buf = dma_buf_get(fd);
	drm_buffer->fd = fd;

	atomic_set(&drm_buffer->ref.refcount, 0);
	INIT_LIST_HEAD(&drm_buffer->buffer);
	mutex_lock(&iommu_info->list_mutex);
	drm_buffer->index = iommu_info->max_idx;
	list_add_tail(&drm_buffer->buffer, &iommu_info->buffer_list);
	iommu_info->max_idx++;
	if (iommu_info->max_idx % 0x100000000L == 0)
		iommu_info->max_idx = 0;
	mutex_unlock(&iommu_info->list_mutex);

	return drm_buffer->index;
}

static int vcodec_drm_create(struct vcodec_iommu_info *iommu_info)
{
	struct vcodec_iommu_drm_info *drm_info;
	int ret;

	iommu_info->private = kzalloc(sizeof(*drm_info),
				      GFP_KERNEL);
	drm_info = iommu_info->private;
	if (!drm_info)
		return -ENOMEM;

	drm_info->domain = iommu_domain_alloc(&platform_bus_type);
	if (!drm_info->domain)
		return -ENOMEM;

	ret = iommu_get_dma_cookie(drm_info->domain);
	if (ret)
		goto err_free_domain;

	vcodec_drm_attach(iommu_info);

	return 0;

err_free_domain:
	iommu_domain_free(drm_info->domain);

	return ret;
}

static int vcodec_drm_destroy(struct vcodec_iommu_info *iommu_info)
{
	struct vcodec_iommu_drm_info *drm_info = iommu_info->private;

	vcodec_drm_detach(iommu_info);
	iommu_put_dma_cookie(drm_info->domain);
	iommu_domain_free(drm_info->domain);

	kfree(drm_info);
	iommu_info->private = NULL;

	return 0;
}

static struct vcodec_iommu_ops drm_ops = {
	.create = vcodec_drm_create,
	.import = vcodec_drm_import,
	.free = vcodec_drm_free,
	.map_kernel = vcodec_drm_map_kernel,
	.unmap_kernel = vcodec_drm_unmap_kernel,
	.map_iommu = vcodec_drm_map_iommu,
	.unmap_iommu = vcodec_drm_unmap_iommu,
	.destroy = vcodec_drm_destroy,
	.dump = vcdoec_drm_dump_info,
	.attach = vcodec_drm_attach,
	.detach = vcodec_drm_detach,
};

void vcodec_iommu_drm_set_ops(struct vcodec_iommu_info *iommu_info)
{
	if (!iommu_info)
		return;
	iommu_info->ops = &drm_ops;
}
