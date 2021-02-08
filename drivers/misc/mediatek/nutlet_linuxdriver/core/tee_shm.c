
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/dma-buf.h>

#include <linux/sched.h>
#include <linux/mm.h>

#include "tee_core_priv.h"
#include "tee_shm.h"



#define INMSG dev_dbg(_DEV(tee), "%s: >\n", __func__)
#define OUTMSG(val) \
	dev_dbg(_DEV(tee), "%s: < %lld\n", __func__, \
		(long long int)(uintptr_t)val)

/* TODO
#if (sizeof(TEEC_SharedMemory) != sizeof(tee_shm))
#error "sizeof(TEEC_SharedMemory) != sizeof(tee_shm))"
#endif
*/

struct tee_context *tee_context_ext;


struct tee_shm *tee_shm_alloc_from_rpc(struct tee *tee, size_t size,
				       uint32_t flags)
{
	struct tee_shm *shm;

	INMSG;

	shm = tee->ops->alloc(tee, size, flags);
	if (IS_ERR_OR_NULL(shm)) {
		dev_err(_DEV(tee),
			"%s: allocation failed (s=%d,flags=0x%08x) err=%ld\n",
			__func__, (int)size, flags, PTR_ERR(shm));
		shm = NULL;
	} else {
		mutex_lock(&tee->lock);
		tee_inc_stats(&tee->stats[TEE_STATS_SHM_IDX]);
		list_add_tail(&shm->entry, &tee->list_rpc_shm);
		mutex_unlock(&tee->lock);
		shm->ctx = NULL;
		shm->tee = tee;
	}

	OUTMSG(shm);
	return shm;
}
EXPORT_SYMBOL(tee_shm_alloc_from_rpc);

void tee_shm_free_from_rpc(struct tee_shm *shm)
{
	if (shm == NULL)
		return;
	if (shm->ctx == NULL) {
		mutex_lock(&shm->tee->lock);
		tee_dec_stats(&shm->tee->stats[TEE_STATS_SHM_IDX]);
		list_del(&shm->entry);
		mutex_unlock(&shm->tee->lock);
		printk( "%s: shm=%p, begin to invoke tee->ops->free\n", __func__, shm );		//wliang
		shm->tee->ops->free(shm);
	} else {
		printk( "%s: shm=%p, begin to invoke tee_shm_free\n", __func__, shm );		//wliang
		tee_shm_free(shm);
		}
}
EXPORT_SYMBOL(tee_shm_free_from_rpc);


struct tee_shm *tee_shm_alloc(struct tee_context *ctx, size_t size,
			      uint32_t flags)
{
	struct tee_shm *shm;
	struct tee *tee;

	BUG_ON(!ctx);
	BUG_ON(!ctx->tee);

	tee = ctx->tee;

	INMSG;

	if (!ctx->usr_client)
		flags |= TEE_SHM_FROM_KAPI;

	shm = tee->ops->alloc(tee, size, flags);
	if (IS_ERR_OR_NULL(shm)) {
		dev_err(_DEV(tee),
			"%s: allocation failed (s=%d,flags=0x%08x) err=%ld\n",
			__func__, (int)size, flags, PTR_ERR(shm));
	} else {
		shm->ctx = ctx;
		shm->tee = tee;

		dev_dbg(_DEV(ctx->tee), "%s: shm=%p, paddr=%p,s=%d/%d app=\"%s\" pid=%d\n",
			 __func__, shm, (void *)shm->paddr, (int)shm->size_req,
			 (int)shm->size_alloc, current->comm, current->pid);
	}



	OUTMSG(shm);
	return shm;
}

void tee_shm_free(struct tee_shm *shm)
{
	struct tee *tee;

	if (IS_ERR_OR_NULL(shm))
		return;
	tee = shm->tee;
	if (tee == NULL)
		pr_warn("invalid call to tee_shm_free(%p): NULL tee\n", shm);
	else if (shm->ctx == NULL)
		dev_warn(_DEV(tee), "tee_shm_free(%p): NULL context\n", shm);
	else if (shm->ctx->tee == NULL)
		dev_warn(_DEV(tee), "tee_shm_free(%p): NULL tee\n", shm);
	else {
		printk( "%s: shm=%p, begin to invoke tee->ops->free, tz_free\n", __func__, shm );	//wliang
		shm->ctx->tee->ops->free(shm);
	}
}

/*
 * tee_shm dma_buf operations
 */
static struct sg_table *_tee_shm_dmabuf_map_dma_buf(struct dma_buf_attachment
						    *attach,
						    enum dma_data_direction dir)
{
	return NULL;
}

static void _tee_shm_dmabuf_unmap_dma_buf(struct dma_buf_attachment *attach,
					  struct sg_table *table,
					  enum dma_data_direction dir)
{
	return;
}

static void _tee_shm_dmabuf_release(struct dma_buf *dmabuf)
{
	struct tee_shm *shm = dmabuf->priv;
	struct device *dev;
	struct tee_context *ctx;
	struct tee *tee;
	BUG_ON(!shm);
	BUG_ON(!shm->ctx);
	BUG_ON(!shm->ctx->tee);
	tee = shm->ctx->tee;

	INMSG;

	ctx = shm->ctx;
	dev = shm->dev;
	dev_dbg(_DEV(ctx->tee), "%s: shm=%p, paddr=%p,s=%d/%d app=\"%s\" pid=%d\n",
		 __func__, shm, (void *)shm->paddr, (int)shm->size_req,
		 (int)shm->size_alloc, current->comm, current->pid);

	mutex_lock(&ctx->tee->lock);
	tee_dec_stats(&tee->stats[TEE_STATS_SHM_IDX]);
	list_del(&shm->entry);
	mutex_unlock(&ctx->tee->lock);
	
	printk("%s begin to invoke tee_shm_free\n", __func__);
	tee_shm_free(shm);
	tee_put(ctx->tee);
	tee_context_put(ctx);
	if (dev)
		put_device(dev);

	OUTMSG(0);
}

static int _tee_shm_dmabuf_mmap(struct dma_buf *dmabuf,
				struct vm_area_struct *vma)
{
	struct tee_shm *shm = dmabuf->priv;
	size_t size = vma->vm_end - vma->vm_start;
	struct tee *tee;
	int ret;
	pgprot_t prot;
	BUG_ON(!shm);
	BUG_ON(!shm->ctx);
	BUG_ON(!shm->ctx->tee);
	tee = shm->ctx->tee;

	INMSG;

	if (shm->flags & TEE_SHM_CACHED)
		prot = vma->vm_page_prot;
	else
		prot = pgprot_noncached(vma->vm_page_prot);

	ret =
	    remap_pfn_range(vma, vma->vm_start, shm->paddr >> PAGE_SHIFT, size,
			    prot);
	if (!ret)
		vma->vm_private_data = (void *)shm;

	dev_dbg(_DEV(shm->ctx->tee), "%s: map the shm (p@=%p,s=%dKiB) => %x\n",
		__func__, (void *)shm->paddr, (int)size / 1024,
		(unsigned int)vma->vm_start);

	OUTMSG(ret);
	return ret;
}

static void *_tee_shm_dmabuf_kmap_atomic(struct dma_buf *dmabuf,
					 unsigned long pgnum)
{
	return NULL;
}

static void *_tee_shm_dmabuf_kmap(struct dma_buf *dmabuf, unsigned long pgnum)
{
	return NULL;
}

struct dma_buf_ops _tee_shm_dma_buf_ops = {
	.map_dma_buf = _tee_shm_dmabuf_map_dma_buf,
	.unmap_dma_buf = _tee_shm_dmabuf_unmap_dma_buf,
	.release = _tee_shm_dmabuf_release,
	.kmap_atomic = _tee_shm_dmabuf_kmap_atomic,
	.kmap = _tee_shm_dmabuf_kmap,
	.mmap = _tee_shm_dmabuf_mmap,
};

static int get_fd(struct tee *tee, struct tee_shm *shm)
{
	struct dma_buf *dmabuf;
	int fd = -1;

	dmabuf = dma_buf_export(shm, &_tee_shm_dma_buf_ops, shm->size_alloc, O_RDWR, NULL);
	//dmabuf = dma_buf_export(shm, &_tee_shm_dma_buf_ops, shm->size_alloc, O_RDWR);	//wliang
//#define dma_buf_export(priv, ops, size, flags)   dma_buf_export_named(priv, ops, size, flags, __FILE__)


	if (IS_ERR_OR_NULL(dmabuf)) {
		dev_err(_DEV(tee), "%s: dmabuf: couldn't export buffer (%ld)\n",
			__func__, PTR_ERR(dmabuf));
		goto out;
	}

	fd = dma_buf_fd(dmabuf, O_CLOEXEC);

out:
	OUTMSG(fd);
	return fd;
}

int tee_shm_alloc_fd(struct tee_context *ctx, struct tee_shm_io *shm_io)
{
	struct tee_shm *shm;
	struct tee *tee = ctx->tee;
	int ret;

	INMSG;

	shm_io->fd_shm = 0;

	shm = tee_shm_alloc(ctx, shm_io->size, shm_io->flags);
	if (IS_ERR_OR_NULL(shm)) {
		dev_err(_DEV(tee), "%s: buffer allocation failed (%ld)\n",
			__func__, PTR_ERR(shm));
		return PTR_ERR(shm);
	}

	shm_io->fd_shm = get_fd(tee, shm);
	if (shm_io->fd_shm <= 0) {
		printk("%s begin to invoke tee_shm_free\n", __func__);
		tee_shm_free(shm);
		ret = -ENOMEM;
		goto out;
	}
	shm->dev = get_device(tee->dev);
	ret = tee_get(tee);
	BUG_ON(ret);		/* tee_core_get must not issue */
	tee_context_get(ctx);

	mutex_lock(&tee->lock);
	tee_inc_stats(&tee->stats[TEE_STATS_SHM_IDX]);
	list_add_tail(&shm->entry, &ctx->list_shm);
	mutex_unlock(&tee->lock);
out:
	OUTMSG(ret);
	return ret;
}
int tee_context_ext_init(void)
{
    int ret = 0;
	tee_context_ext = (struct tee_context *)kzalloc(sizeof(struct tee_context), GFP_KERNEL);	
	
	if(!tee_context_ext)
	{
		printk("tee_context_ext_init failed\n");
        ret = -1;
	}
	#ifdef SHM_LIST_DEBUG
	    printk("%s,init success\n" , __func__);
	#endif
	INIT_LIST_HEAD(&(tee_context_ext->entry));	
	INIT_LIST_HEAD(&(tee_context_ext->list_sess));	
	INIT_LIST_HEAD(&(tee_context_ext->list_shm));
// zhuhy@nutlet, add lock for shm,20160221	
	mutex_init(&(tee_context_ext->lock));
	return ret;

}
int tee_context_ext_free(void)
{
    int ret = 0;
    kfree(tee_context_ext);
	
    #ifdef SHM_LIST_DEBUG
	    printk("%s,free success\n" , __func__);
    #endif
	return ret;
}
int tee_get_shm_flag(void *ptr , size_t size)
{
    
	struct tee_shm *shm ;
	//struct tee_context *entry = NULL;
	//struct tee_context *ctx = NULL;
	int ret = 0;
    int paddr = *(uint32_t*)ptr;
    int shmpaddr;
    int shmsize;
	
    #ifdef SHM_LIST_DEBUG
	    printk("%s,addr:%x,size:%x\n" ,__func__, paddr, (int)size );
    #endif
	//(void *)shm->paddr
	#if 1
	list_for_each_entry(shm, &tee_context_ext->list_shm, entry) {
	    shmpaddr = (int)(shm->paddr);
        shmsize = (uint32_t)shm->size_alloc;
		
        #ifdef SHM_LIST_DEBUG
            printk("shm->paddr:%x\n", shmpaddr);
            printk("shm->paddr:%p\n", (void*)shm->paddr);
            printk("shm->size_alloc:%x\n", shmsize);
		#endif
        
		if(( paddr >= shmpaddr) && ( (paddr + size) <= ( shmpaddr + shmsize) ) )
		{
		
            #ifdef SHM_LIST_DEBUG
		        printk("node kaddr:%x, paddr:%x,size:%x\n", *(uint32_t*) (shm->kaddr),(int)(shm->paddr), (int)(shm->size_alloc));
            #endif
			ret = shm->flags;
			
		}


	}
	
    #ifdef SHM_LIST_DEBUG
	    printk("%s ,ret= :0x%x,over\n", __func__, ret);
    #endif
	#endif
	return ret;
	

}
int tee_context_ext_del(struct tee_shm *shm)
{
   int ret = -1;
    struct tee_shm *shm1;	
    struct tee_shm *shm2;
	
    #ifdef SHM_LIST_DEBUG
	    printk("%s", __func__); 	
	    printk("realse %s,kaddr:%d,paddr:%x,size:%x\n" ,__func__, *(uint32_t*)shm->kaddr, (int)shm->paddr,(int)shm->size_alloc);
    #endif
	tee_context_ext_print();
	
	list_for_each_entry_safe(shm1, shm2,&tee_context_ext->list_shm, entry) {
	   // printk("<rease addr:%x,resize:%x>,curaddr:%x,curlen:%x\n", *(uint32_t *)shm->paddr, (int)shm->size_req,*(uint32_t *)shm1->paddr,(int)shm1->size_alloc);
		if((shm->paddr >= shm1->paddr)&&(shm->paddr + shm->size_req <= shm1->paddr+ shm1->size_alloc))
		{		    
		
            #ifdef SHM_LIST_DEBUG
		        printk("node kaddr:%x, paddr:%x,size:%x\n", *(uint32_t*) (shm1->kaddr),(int)(shm1->paddr), (int)(shm1->size_alloc));
            #endif
			if((void *)(&shm1->entry)!= NULL)
			{
	  			list_del(&shm1->entry);
				
                #ifdef SHM_LIST_DEBUG
	  			   printk("release node\n");
				#endif
			}
			ret = 0;
		}

	}
	tee_context_ext_print();
	return ret;

}


void tee_context_ext_print(void)
{
  #ifdef SHM_LIST_DEBUG   
   struct tee_shm *shm1;   
   int i = 0; 
   
    printk("/***************Shm List Node *************/\n");
	list_for_each_entry(shm1,&tee_context_ext->list_shm, entry)
	{
		printk("node[%d],%x|%x flags:%x....",i, (int)shm1->paddr, (int)shm1->size_alloc, shm1->flags);
		i++;		
	}
    printk("\n");
	#endif
   
}

/* Buffer allocated by rpc from fw and to be accessed by the user
 * Not need to be registered as it is not allocated by the user */
int tee_shm_get_fd(struct tee_context *ctx, struct tee_shm_io *shm_io)
{
	struct tee_shm *shm = NULL;
	struct tee *tee = ctx->tee;
	int ret;
	struct list_head *pshm;

	INMSG;

	shm_io->fd_shm = 0;

	if (!list_empty(&tee->list_rpc_shm)) {
		list_for_each(pshm, &tee->list_rpc_shm) {
			shm = list_entry(pshm, struct tee_shm, entry);
			if ((void *)shm->paddr == shm_io->buffer)
				goto found;
		}
	}

	dev_err(tee->dev, "Can't find shm for %p\n", (void *)shm_io->buffer);
	ret = -ENOMEM;
	goto out;

found:
	shm_io->fd_shm = get_fd(tee, shm);
	if (shm_io->fd_shm <= 0) {
		printk("%s begin to invoke tee_shm_free\n", __func__);
		tee_shm_free(shm);
		ret = -ENOMEM;
		goto out;
	}

	shm->ctx = ctx;
	ret = tee->ops->shm_inc_ref(shm);
	BUG_ON(!ret);		/* to do: path error */
	mutex_lock(&tee->lock);
	list_move(&shm->entry, &ctx->list_shm);
	mutex_unlock(&tee->lock);

	shm->dev = get_device(tee->dev);
	ret = tee_get(tee);
	BUG_ON(ret);
	tee_context_get(ctx);

out:
	OUTMSG(ret);
	return ret;
}

int check_shm(struct tee *tee, struct tee_shm_io *shm_io)
{
	struct vm_area_struct *vma;
	struct mm_struct *mm = current->mm;
	struct tee_shm *shm;
	int ret = 0;

	INMSG;

	if (shm_io->flags & TEE_SHM_FROM_KAPI) {
		/* TODO fixme will not work on 64-bit platform */
		shm = (struct tee_shm *)(uintptr_t)shm_io->fd_shm;
		BUG_ON(!shm);
		/* must be size_req but not in line with above test */
		if (shm->size_req < shm_io->size) {
			dev_err(tee->dev, "[%s] %p not big enough %x %zu %zu\n",
				__func__, shm_io->buffer,
				(unsigned int)shm->paddr, shm->size_req,
				shm_io->size);
			ret = -EINVAL;
		}
		goto out;
	}

	/* if the caller is the kernel api, active_mm is mm */
	if (!mm)
		mm = current->active_mm;

	vma = find_vma(mm, (unsigned long)shm_io->buffer);
	if (!vma) {
		dev_err(tee->dev, "[%s] %p can't find vma\n", __func__,
			shm_io->buffer);
		ret = -EINVAL;
		goto out;
	}

	shm = vma->vm_private_data;

	/* It's a VMA => consider it a a user address */
	if (!(vma->vm_flags & (VM_IO | VM_PFNMAP))) {
		dev_err(tee->dev, "[%s] %p not Contiguous %x\n", __func__,
			shm_io->buffer, shm ? (unsigned int)shm->paddr : 0);
		ret = -EINVAL;
		goto out;
	}

	/* Contiguous ? */
	if (vma->vm_end - vma->vm_start < shm_io->size) {
		dev_err(tee->dev, "[%s] %p not big enough %x %ld %zu\n",
			__func__, shm_io->buffer,
			shm ? (unsigned int)shm->paddr : 0,
			vma->vm_end - vma->vm_start, shm_io->size);
		ret = -EINVAL;
	}

out:
	OUTMSG(ret);
	return ret;
}

static dma_addr_t get_phy_addr(struct tee *tee, struct tee_shm_io *shm_io)
{
	struct vm_area_struct *vma;
	struct mm_struct *mm = current->mm;
	struct tee_shm *shm;

	INMSG;

	/* if the caller is the kernel api, active_mm is mm */
	if (!mm)
		mm = current->active_mm;

	vma = find_vma(mm, (unsigned long)shm_io->buffer);
	BUG_ON(!vma);
	shm = vma->vm_private_data;

	OUTMSG(shm->paddr);
	/* Consider it has been allowd by the TEE */
	return shm->paddr;
}

struct tee_shm *tee_shm_get(struct tee_context *ctx, struct tee_shm_io *shm_io)
{
	struct tee_shm *shm;
	struct list_head *pshm;
	int ret;
	dma_addr_t buffer;
	struct tee * __attribute__ ((unused)) tee  = ctx->tee;

	INMSG;

	if (shm_io->flags & TEE_SHM_FROM_KAPI) {
		/* TODO fixme will not work on 64-bit platform */
		shm = (struct tee_shm *)(uintptr_t)shm_io->fd_shm;
		BUG_ON(!shm);
		ret = ctx->tee->ops->shm_inc_ref(shm);
		BUG_ON(!ret);	/* to do: path error */
		OUTMSG(shm);
		return shm;
	}

	buffer = get_phy_addr(ctx->tee, shm_io);
	if (!buffer)
		return NULL;

	if (!list_empty(&ctx->list_shm)) {
		list_for_each(pshm, &ctx->list_shm) {
			shm = list_entry(pshm, struct tee_shm, entry);
			BUG_ON(!shm);
			/* if this ok, do not need to get_phys_addr
			 * if ((void *)shm->kaddr == shm_io->buffer) { */
			if (shm->paddr == buffer) {
				ret = ctx->tee->ops->shm_inc_ref(shm);
				BUG_ON(!ret);
				OUTMSG(shm);
				return shm;
			}
		}
	}
	BUG_ON(1);
	return NULL;
}

void tee_shm_put(struct tee_shm *shm)
{
	printk("%s begin to invoke tee_shm_free\n", __func__);
	tee_shm_free(shm);
}
