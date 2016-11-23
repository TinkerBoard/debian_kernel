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

#include <linux/rockchip_ion.h>
#include <linux/rockchip-iovmm.h>
#include <linux/slab.h>

#include "vcodec_iommu_ops.h"

struct vcodec_ion_buffer {
	struct list_head buffer;
	struct ion_handle *handle;
	union {
		unsigned long iova;
		unsigned long phys;
	};
	u8 *virtual_addr;
	unsigned long size;
	int fd;
	int index;
	struct kref ref;
};

struct vcodec_iommu_ion_info {
	struct ion_client *ion_client;
};

static struct vcodec_ion_buffer *
vcodec_ion_get_buffer(struct vcodec_iommu_info *iommu_info, int idx)
{
	struct vcodec_ion_buffer *ion_buffer = NULL, *n;

	list_for_each_entry_safe(ion_buffer, n,
				 &iommu_info->buffer_list, buffer) {
		if (ion_buffer->index == idx)
			return ion_buffer;
	}

	return NULL;
}

static struct vcodec_ion_buffer *
vcodec_ion_get_buffer_fd(struct vcodec_iommu_info *iommu_info, int fd)
{
	struct vcodec_ion_buffer *ion_buffer = NULL, *n;

	list_for_each_entry_safe(ion_buffer, n,
				 &iommu_info->buffer_list, buffer) {
		if (ion_buffer->fd == fd)
			return ion_buffer;
	}

	return NULL;
}

static void vcodec_ion_clear_map(struct kref *ref)
{
	/* do nothing */
}

static int vcodec_ion_destroy(struct vcodec_iommu_info *iommu_info)
{
	struct vcodec_iommu_ion_info *ion_info = iommu_info->private;

	rockchip_iovmm_deactivate(iommu_info->mmu_dev);

	kfree(ion_info);
	iommu_info->private = NULL;

	return 0;
}

static int vcodec_ion_free_fd(struct vcodec_iommu_info *iommu_info, int fd)
{
	struct vcodec_ion_buffer *ion_buffer;
	struct vcodec_iommu_ion_info *ion_info = iommu_info->private;

	mutex_lock(&iommu_info->list_mutex);
	ion_buffer = vcodec_ion_get_buffer(iommu_info, idx);

	if (!ion_buffer) {
		pr_err("can not find %d buffer in list\n", idx);

		return -EINVAL;
	}

	ion_free(ion_info->ion_client, ion_buffer->handle);

	list_del_init(&ion_buffer->buffer);
	mutex_unlock(&iommu_info->list_mutex);
	kfree(ion_buffer);

	return 0;
}

static int vcodec_ion_free(struct vcodec_iommu_info *iommu_info, int idx)
{
	struct vcodec_ion_buffer *ion_buffer;
	struct vcodec_iommu_ion_info *ion_info = iommu_info->private;

	mutex_lock(&iommu_info->list_mutex);
	ion_buffer = vcodec_ion_get_buffer(iommu_info, idx);

	if (!ion_buffer) {
		mutex_unlock(&iommu_info->list_mutex);
		pr_err("can not find %d buffer in list\n", idx);

		return -EINVAL;
	}

	ion_free(ion_info->ion_client, ion_buffer->handle);

	list_del_init(&ion_buffer->buffer);
	mutex_unlock(&iommu_info->list_mutex);
	kfree(ion_buffer);

	return 0;
}

static int vcodec_ion_unmap_iommu(struct vcodec_iommu_info *iommu_info, int idx)
{
	struct vcodec_ion_buffer *ion_buffer;

	mutex_lock(&iommu_info->list_mutex);
	ion_buffer = vcodec_ion_get_buffer(iommu_info, idx);
	mutex_unlock(&iommu_info->list_mutex);

	if (!ion_buffer) {
		pr_err("can not find %d buffer in list\n", idx);

		return -EINVAL;
	}

	kref_put(&ion_buffer->ref, vcodec_ion_clear_map);

	return 0;
}

static int vcodec_ion_map_iommu(struct vcodec_iommu_info *iommu_info, int idx,
				unsigned long *iova, unsigned long *size)
{
	struct vcodec_ion_buffer *ion_buffer;
	struct device *dev = iommu_info->dev;
	struct vcodec_iommu_ion_info *ion_info = iommu_info->private;
	int ret;

	/* Force to flush iommu table */
	if (of_machine_is_compatible("rockchip,rk3288"))
		rockchip_iovmm_invalidate_tlb(iommu_info->mmu_dev);

	mutex_lock(&iommu_info->list_mutex);
	ion_buffer = vcodec_ion_get_buffer(iommu_info, idx);
	mutex_unlock(&iommu_info->list_mutex);

	if (!ion_buffer) {
		pr_err("can not find %d buffer in list\n", idx);

		return -EINVAL;
	}

	if (ion_buffer->iova != 0) {
		kref_get(&ion_buffer->ref);
		*iova = ion_buffer->iova;
		*size = ion_buffer->size;

		return 0;
	}

	if (iommu_info->mmu_dev)
		ret = ion_map_iommu(dev, ion_info->ion_client,
				    ion_buffer->handle, iova, size);
	else
		ret = ion_phys(ion_info->ion_client, ion_buffer->handle,
			       iova, size);

	if (ret < 0)
		return  ret;

	kref_get(&ion_buffer->ref);

	return 0;
}

static int vcodec_ion_unmap_kernel(struct vcodec_iommu_info *iommu_info,
				   int idx)
{
	struct vcodec_ion_buffer *ion_buffer;

	/* Force to flush iommu table */
	if (of_machine_is_compatible("rockchip,rk3288"))
		rockchip_iovmm_invalidate_tlb(iommu_info->mmu_dev);

	mutex_lock(&iommu_info->list_mutex);
	ion_buffer = vcodec_ion_get_buffer(iommu_info, idx);
	mutex_unlock(&iommu_info->list_mutex);

	if (!ion_buffer) {
		pr_err("can not find %d buffer in list\n", idx);

		return -EINVAL;
	}

	kref_put(&ion_buffer->ref, vcodec_ion_clear_map);

	return 0;
}

static u8 *vcodec_ion_map_kernel(struct vcodec_iommu_info *iommu_info, int idx)
{
	struct vcodec_ion_buffer *ion_buffer;
	struct vcodec_iommu_ion_info *ion_info = iommu_info->private;

	mutex_lock(&iommu_info->list_mutex);
	ion_buffer = vcodec_ion_get_buffer(iommu_info, idx);
	mutex_unlock(&iommu_info->list_mutex);

	if (!ion_buffer) {
		pr_err("can not find %d buffer in list\n", idx);

		return NULL;
	}

	if (!ion_buffer->virtual_addr) {
		ion_buffer->virtual_addr = ion_map_kernel(ion_info->ion_client,
							  ion_buffer->handle);
	}

	kref_get(&ion_buffer->ref);

	return ion_buffer->virtual_addr;
}

static int vcodec_ion_import(struct vcodec_iommu_info *iommu_info, int fd)
{
	struct vcodec_ion_buffer *ion_buffer = NULL, *n;
	struct vcodec_iommu_ion_info *ion_info = iommu_info->private;

	list_for_each_entry_safe(ion_buffer, n, &iommu_info->buffer_list,
				 buffer) {
		if (ion_buffer->fd == fd)
			return ion_buffer->index;
	}

	ion_buffer = kzalloc(sizeof(*ion_buffer), GFP_KERNEL);
	if (!ion_buffer)
		return -ENOMEM;

	ion_buffer->handle = ion_import_dma_buf(ion_info->ion_client, fd);
	ion_buffer->fd = fd;

	kref_init(&ion_buffer->ref);
	INIT_LIST_HEAD(&ion_buffer->buffer);
	mutex_lock(&iommu_info->list_mutex);
	ion_buffer->index = iommu_info->max_idx;
	list_add_tail(&ion_buffer->buffer, &iommu_info->buffer_list);
	iommu_info->max_idx++;
	if (iommu_info->max_idx % 0x100000000L == 0)
		iommu_info->max_idx = 0;
	mutex_unlock(&iommu_info->list_mutex);

	return ion_buffer->index;
}

static int vcodec_ion_create(struct vcodec_iommu_info *iommu_info)
{
	struct vcodec_iommu_ion_info *ion_info;

	iommu_info->private = kmalloc(sizeof(*ion_info), GFP_KERNEL);

	ion_info = iommu_info->private;
	if (!ion_info)
		return -ENOMEM;

	ion_info->ion_client = rockchip_ion_client_create("vpu");

	return IS_ERR(ion_info->ion_client) ? -1 : 0;
}

static struct vcodec_iommu_ops ion_ops = {
	.create = vcodec_ion_create,
	.destroy = vcodec_ion_destroy,
	.import = vcodec_ion_import,
	.free = vcodec_ion_free,
	.free_fd = vcodec_ion_free_fd,
	.map_kernel = vcodec_ion_map_kernel,
	.unmap_kernel = vcodec_ion_unmap_kernel,
	.map_iommu = vcodec_ion_map_iommu,
	.unmap_iommu = vcodec_ion_unmap_iommu,
	.dump = NULL,
	.attach = NULL,
	.detach = NULL,
};

void vcodec_iommu_ion_set_ops(struct vcodec_iommu_info *iommu_info)
{
	if (!iommu_info)
		return;
	iommu_info->ops = &ion_ops;
}
