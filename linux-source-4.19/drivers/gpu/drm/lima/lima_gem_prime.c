// SPDX-License-Identifier: GPL-2.0 OR MIT
/* Copyright 2018 Qiang Yu <yuq825@gmail.com> */

#include <linux/dma-buf.h>
#include <drm/drm_prime.h>
#include <drm/drm_drv.h>
#include <drm/drm_file.h>

#include "lima_device.h"
#include "lima_object.h"
#include "lima_gem_prime.h"

struct drm_gem_object *lima_gem_prime_import_sg_table(
	struct drm_device *dev, struct dma_buf_attachment *attach,
	struct sg_table *sgt)
{
	struct reservation_object *resv = attach->dmabuf->resv;
	struct lima_device *ldev = to_lima_dev(dev);
	struct lima_bo *bo;

	ww_mutex_lock(&resv->lock, NULL);

	bo = lima_bo_create(ldev, attach->dmabuf->size, 0,
			    ttm_bo_type_sg, sgt, resv);
	if (IS_ERR(bo))
		goto err_out;

	ww_mutex_unlock(&resv->lock);
	return &bo->gem;

err_out:
	ww_mutex_unlock(&resv->lock);
	return (void *)bo;
}

struct reservation_object *lima_gem_prime_res_obj(struct drm_gem_object *obj)
{
        struct lima_bo *bo = to_lima_bo(obj);

	return bo->tbo.resv;
}

struct sg_table *lima_gem_prime_get_sg_table(struct drm_gem_object *obj)
{
	struct lima_bo *bo = to_lima_bo(obj);
	int npages = bo->tbo.num_pages;

	return drm_prime_pages_to_sg(bo->tbo.ttm->pages, npages);
}

int lima_gem_prime_dma_buf_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct drm_file *priv = filp->private_data;
	struct drm_device *dev = priv->minor->dev;
	struct lima_device *ldev = dev->dev_private;
	struct ttm_bo_device *bdev = &ldev->mman.bdev;
	struct drm_gem_object *obj = NULL;
	struct drm_vma_offset_node *node;
	int ret;

	drm_vma_offset_lock_lookup(&bdev->vma_manager);
	node = drm_vma_offset_exact_lookup_locked(&bdev->vma_manager,
						  vma->vm_pgoff,
						  vma_pages(vma));
	if (likely(node)) {
		struct ttm_buffer_object *tbo =
			container_of(node, struct ttm_buffer_object, vma_node);
		struct lima_bo *bo = container_of(tbo, struct lima_bo, tbo);
		obj = &bo->gem;
		/*
		 * When the object is being freed, after it hits 0-refcnt it
		 * proceeds to tear down the object. In the process it will
		 * attempt to remove the VMA offset and so acquire this
		 * mgr->vm_lock.  Therefore if we find an object with a 0-refcnt
		 * that matches our range, we know it is in the process of being
		 * destroyed and will be freed as soon as we release the lock -
		 * so we have to check for the 0-refcnted object and treat it as
		 * invalid.
		 */
		if (!kref_get_unless_zero(&obj->refcount))
			obj = NULL;
	}
	drm_vma_offset_unlock_lookup(&bdev->vma_manager);

	if (!obj)
		return -EINVAL;

	/* only for buffer imported from other device */
	if (!obj->import_attach) {
		ret = -EINVAL;
		goto out;
	}

	ret = dma_buf_mmap(obj->dma_buf, vma, 0);

out:
	drm_gem_object_put_unlocked(obj);
	return ret;
}

void *lima_gem_prime_vmap(struct drm_gem_object *obj)
{
	struct lima_bo *bo = to_lima_bo(obj);
	int ret;

	ret = ttm_bo_kmap(&bo->tbo, 0, bo->tbo.num_pages, &bo->dma_buf_vmap);
	if (ret)
		return ERR_PTR(ret);

	return bo->dma_buf_vmap.virtual;
}

void lima_gem_prime_vunmap(struct drm_gem_object *obj, void *vaddr)
{
	struct lima_bo *bo = to_lima_bo(obj);

	ttm_bo_kunmap(&bo->dma_buf_vmap);
}

int lima_gem_prime_mmap(struct drm_gem_object *obj, struct vm_area_struct *vma)
{
	struct lima_bo *bo = to_lima_bo(obj);
	struct lima_device *dev = ttm_to_lima_dev(bo->tbo.bdev);
	int ret;

	if (!vma->vm_file || !dev)
		return -ENODEV;

	/* Check for valid size. */
	if (obj->size < vma->vm_end - vma->vm_start)
		return -EINVAL;

	vma->vm_pgoff += drm_vma_node_offset_addr(&bo->tbo.vma_node) >> PAGE_SHIFT;

	/* prime mmap does not need to check access, so allow here */
	ret = drm_vma_node_allow(&obj->vma_node, vma->vm_file->private_data);
	if (ret)
		return ret;

	ret = ttm_bo_mmap(vma->vm_file, vma, &dev->mman.bdev);
	drm_vma_node_revoke(&obj->vma_node, vma->vm_file->private_data);

	return ret;
}
