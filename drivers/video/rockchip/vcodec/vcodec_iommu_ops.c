/**
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

#include <linux/slab.h>

#include "vcodec_iommu_ops.h"

int vcodec_iommu_create(struct vcodec_iommu_info *iommu_info)
{
	if (!iommu_info || !iommu_info->ops->create)
		return 0;

	return iommu_info->ops->create(iommu_info);
}

int vcodec_iommu_import(struct vcodec_iommu_info *iommu_info, int fd)
{
	if (!iommu_info || !iommu_info->ops->import)
		return 0;

	return iommu_info->ops->import(iommu_info, fd);
}

int vcodec_iommu_free(struct vcodec_iommu_info *iommu_info, int idx)
{
	if (!iommu_info || !iommu_info->ops->free)
		return 0;

	return iommu_info->ops->free(iommu_info, idx);
}

uint8_t *vcodec_iommu_map_kernel(struct vcodec_iommu_info *iommu_info, int idx)
{
	if (!iommu_info || !iommu_info->ops->map_kernel)
		return 0;

	return iommu_info->ops->map_kernel(iommu_info, idx);
}

int vcodec_iommu_unmap_kernel(struct vcodec_iommu_info *iommu_info, int idx)
{
	if (!iommu_info || !iommu_info->ops->unmap_kernel)
		return 0;

	return iommu_info->ops->unmap_kernel(iommu_info, idx);
}

int vcodec_iommu_map_iommu(struct vcodec_iommu_info *iommu_info,
			   int idx,
			   unsigned long *iova,
			   unsigned long *size)
{
	if (!iommu_info || !iommu_info->ops->map_iommu)
		return 0;

	return iommu_info->ops->map_iommu(iommu_info, idx, iova, size);
}

int vcodec_iommu_unmap_iommu(struct vcodec_iommu_info *iommu_info, int idx)
{
	if (!iommu_info || !iommu_info->ops->unmap_iommu)
		return 0;

	return iommu_info->ops->unmap_iommu(iommu_info, idx);
}

int vcodec_iommu_destroy(struct vcodec_iommu_info *iommu_info)
{
	if (!iommu_info || !iommu_info->ops->destroy)
		return 0;

	return iommu_info->ops->destroy(iommu_info);
}

void vcodec_iommu_dump(struct vcodec_iommu_info *iommu_info)
{
	if (!iommu_info || !iommu_info->ops->dump)
		return;

	return iommu_info->ops->dump(iommu_info);
}

int vcodec_iommu_attach(struct vcodec_iommu_info *iommu_info)
{
	if (!iommu_info || !iommu_info->ops->attach)
		return 0;

	return iommu_info->ops->attach(iommu_info);
}

void vcodec_iommu_detach(struct vcodec_iommu_info *iommu_info)
{
	if (!iommu_info || !iommu_info->ops->detach)
		return;

	return iommu_info->ops->detach(iommu_info);
}

struct vcodec_iommu_info *
vcodec_iommu_info_create(struct device *dev,
			 struct device *mmu_dev,
			 int alloc_type)
{
	struct vcodec_iommu_info *iommu_info = NULL;

	iommu_info = kzalloc(sizeof(*iommu_info), GFP_KERNEL);
	if (!iommu_info)
		return NULL;

	iommu_info->dev = dev;
	INIT_LIST_HEAD(&iommu_info->buffer_list);
	mutex_init(&iommu_info->list_mutex);
	iommu_info->max_idx = 0;
	if (alloc_type == ALLOCATOR_USE_DRM)
		vcodec_iommu_drm_set_ops(iommu_info);
#ifdef CONFIG_ION
	else
		vcodec_iommu_ion_set_ops(iommu_info);
#else
	/* Unpported allocator */
	else
		iommu_info->ops = NULL;
#endif
	iommu_info->mmu_dev = mmu_dev;

	vcodec_iommu_create(iommu_info);

	return iommu_info;
}

int vcodec_iommu_info_destroy(struct vcodec_iommu_info *iommu_info)
{
	vcodec_iommu_destroy(iommu_info);
	kfree(iommu_info);

	return 0;
}
