/*
 *  tee_driver.c - TEE core kernel module.
 *
 */

/* #define DEBUG */
#define DEBUG	1	//wliang
//#include <mach/eint.h> //wliang
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/idr.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/uaccess.h>
#include <asm-generic/ioctl.h>
#include <linux/sched.h>

#include "linux_nutlet/tee_core.h"
#include "linux_nutlet/tee_ioc.h"

#include "tee_core_priv.h"

#include "tee_sysfs.h"
#include "tee_debugfs.h"
#include "tee_shm.h"
#include "tee_supp_com.h"

#define _TEE_CORE_FW_VER "1:0.1"

static char *_tee_supp_app_name = "tee-supplicant";		//wliang

/* Store the class misc reference */
static struct class *misc_class;

#ifdef CONFIG_COMPAT		// zhuhy, for 32bit process

//TEEC_TempMemoryReference
static void tee_op_tmr_32_to_64(TEEC_TempMemoryReference * mr64, TEEC_TempMemoryReference_32 * mr32)
{
	memset(mr64, 0, sizeof(TEEC_TempMemoryReference));

//	mr64->buffer = (void*)mr32->buffer;
	memcpy(&mr64->buffer, &mr32->buffer, sizeof(mr32->buffer));
	mr64->size = (size_t)mr32->size;

	
//	pr_err("%s:%d - %p, %x\n", __func__, __LINE__,mr64->buffer, (uint32_t)mr64->size);
}

static void tee_op_rmr_32_to_64(TEEC_RegisteredMemoryReference * mr64, TEEC_RegisteredMemoryReference_32 * mr32)
{
	memset(mr64, 0, sizeof(TEEC_RegisteredMemoryReference));

//	mr64->parent = (TEEC_SharedMemory *)mr32->parent;
	memcpy(&mr64->parent, &mr32->parent, sizeof(mr32->parent));

	mr64->size = (size_t)mr32->size;	
	mr64->offset = (size_t)mr32->offset;
	
//	pr_err("%s:%d - %x, %x\n", __func__, __LINE__, (uint32_t)mr64->offset, (uint32_t)mr64->size);
}


static void tee_op_param_32_to_64(TEEC_Parameter * pm64, TEEC_Parameter_32 * pm32)
{
	memset(pm64, 0, sizeof(TEEC_Parameter));

	pm64->value.a = pm32->value.a;
	pm64->value.b = pm32->value.b;

	tee_op_tmr_32_to_64(&pm64->tmpref, &pm32->tmpref);
	tee_op_rmr_32_to_64(&pm64->memref, &pm32->memref);
}

void tee_op_shm_32_to_64(TEEC_SharedMemory * sm64, TEEC_SharedMemory_32 * sm32)
{
	memset(sm64, 0, sizeof(TEEC_SharedMemory));

//	sm64->buffer = (void*)sm32->buffer;
	memcpy(&sm64->buffer, &sm32->buffer, sizeof(sm32->buffer));

	sm64->size = (size_t)sm32->size;
	sm64->flags = (uint32_t)sm32->flags;
	sm64->d.fd = (int)sm32->d.fd;
	sm64->registered = sm32->registered;
}

int tee_is_ioctl32(struct tee_context * ctx)
{
	return ctx->client_is_32bit;
}

int tee_op_32_to_64(TEEC_Operation* op_64, TEEC_Operation_32* op_32)
{
	int i = 0;
	
	if (NULL == op_64 || NULL == op_32)
	{		
		pr_err("%s:%d - invalid pointer\n", __func__, __LINE__);
		return -EINVAL;
	}
	
	memset(op_64, 0, sizeof(TEEC_Operation));

	op_64->started = op_32->started;	
	op_64->paramTypes = op_32->paramTypes;
//	op_64->session = op_32->session;
	memcpy(&op_64->session, &op_32->session, sizeof(op_32->session));

	for (i = 0; i < TEEC_CONFIG_PAYLOAD_REF_COUNT; i++)
	{
		tee_op_param_32_to_64(&op_64->params[i], &op_32->params[i]);
	}
	for (i = 0; i < TEEC_CONFIG_PAYLOAD_REF_COUNT; i++)
	{
		tee_op_shm_32_to_64(&op_64->memRefs[i], &op_32->memRefs[i]);
	}
	op_64->flags = op_32->flags;

	return 0;	
}

int tee_cmd_io_32_to_64(struct tee_cmd_io* io_64, struct tee_cmd_io_32* io_32)
{
	if (NULL == io_64 || NULL == io_32)
	{		
		pr_err("%s:%d - invalid pointer\n", __func__, __LINE__);
		return -EINVAL;
	}

	memset(io_64, 0, sizeof(struct tee_cmd_io));
	
	io_64->err = (TEEC_Result)io_32->err;	
	io_64->origin = (uint32_t)io_32->origin;
	io_64->cmd = (uint32_t)io_32->cmd;
	memcpy(&io_64->uuid, &io_32->uuid, sizeof(io_32->uuid));
	memcpy(&io_64->data, &io_32->data, sizeof(io_32->data));
	io_64->data_size = (uint32_t)io_32->data_size;
	memcpy(&io_64->op, &io_32->op, sizeof(io_32->op));
	io_64->fd_sess = (int)io_32->fd_sess;

	return 0;
}

int tee_shm_io_32_to_64(struct tee_shm_io* io_64, struct tee_shm_io_32* io_32)
{
	if (NULL == io_64 || NULL == io_32)
	{		
		pr_err("%s:%d - invalid pointer\n", __func__, __LINE__);
		return -EINVAL;
	}

	memset(io_64, 0, sizeof(struct tee_shm_io));

	memcpy(&io_64->buffer, &io_32->buffer, sizeof(io_32->buffer));
	io_64->size = (size_t)io_32->size;
	io_64->flags = (uint32_t)io_32->flags;
	io_64->fd_shm = (int)io_32->fd_shm;
	io_64->registered = (uint8_t)io_32->registered;

	return 0;
}
#endif

static int device_match(struct device *device, const void *devname)
{
	struct tee *tee = dev_get_drvdata(device);
	int ret = strncmp(devname, tee->name, sizeof(tee->name));
	BUG_ON(!tee);
	if (ret == 0)
		return 1;
	else
		return 0;
}

/*
 * For the kernel api.
 * Get a reference on a device tee from the device needed
 */
struct tee *tee_get_tee(const char *devname)
{
	struct device *device;
	if (!devname)
		return NULL;
	device = class_find_device(misc_class, NULL, devname, device_match);
	if (!device) {
		pr_err("%s:%d - can't find device [%s]\n", __func__, __LINE__,
		       devname);
		return NULL;
	}

	return dev_get_drvdata(device);
}

void tee_inc_stats(struct tee_stats_entry *entry)
{
	entry->count++;
	if (entry->count > entry->max)
		entry->max = entry->count;
}

void tee_dec_stats(struct tee_stats_entry *entry)
{
	entry->count--;
}

/**
 * tee_get - increases refcount of the tee
 * @tee:	[in]	tee to increase refcount of
 *
 * @note: If tee.ops.start() callback function is available,
 * it is called when refcount is equal at 1.
 */
int tee_get(struct tee *tee)
{
	int ret = 0;

	BUG_ON(!tee);

	if (atomic_inc_return(&tee->refcount) == 1) {
		BUG_ON(!try_module_get(tee->ops->owner));
		dev_dbg(_DEV(tee), "%s: refcount=1 call %s::start()...\n",
			__func__, tee->name);
		get_device(tee->dev);
		if (tee->ops->start)
			ret = tee->ops->start(tee);
	}
	if (ret) {
		put_device(tee->dev);
		module_put(tee->ops->owner);
		dev_err(_DEV(tee), "%s: %s::start() failed, err=%d\n",
			__func__, tee->name, ret);
		atomic_dec(&tee->refcount);
	} else {
		int count = (int)atomic_read(&tee->refcount);
		dev_dbg(_DEV(tee), "%s: refcount=%d\n", __func__, count);
		if (count > tee->max_refcount)
			tee->max_refcount = count;
	}
	return ret;
}

/**
 * tee_put - decreases refcount of the tee
 * @tee:	[in]	tee to reduce refcount of
 *
 * @note: If tee.ops.stop() callback function is available,
 * it is called when refcount is equal at 0.
 */
int tee_put(struct tee *tee)
{
	int ret = 0;
	int count;

	BUG_ON(!tee);

	if (atomic_dec_and_test(&tee->refcount)) {
		dev_dbg(_DEV(tee), "%s: refcount=0 call %s::stop()...\n",
			__func__, tee->name);
		if (tee->ops->stop)
			ret = tee->ops->stop(tee);
		module_put(tee->ops->owner);
		put_device(tee->dev);
	}
	if (ret) {
		dev_err(_DEV(tee), "%s: %s::stop() has failed, ret=%d\n",
			__func__, tee->name, ret);
	}

	count = (int)atomic_read(&tee->refcount);
	dev_dbg(_DEV(tee), "%s: refcount=%d\n", __func__, count);
	return ret;
}

static int tee_supp_open(struct tee *tee)
{
	int ret = 0;
	dev_dbg(_DEV(tee), "%s: appclient=\"%s\" pid=%d, _tee_supp_app_name:%s\n", __func__,
		current->comm, current->pid, _tee_supp_app_name);
	printk("%s: appclient=\"%s\" pid=%d, _tee_supp_app_name:%s\n", __func__,
		current->comm, current->pid, _tee_supp_app_name);		//wliang
	
	BUG_ON(!tee->rpc);

	if (strncmp(_tee_supp_app_name, current->comm,
			strlen(_tee_supp_app_name)) == 0) {
		dev_dbg(_DEV(tee), "current->comm and tee-supplicant is equal pid=%d\n",  current->pid);
		if (atomic_add_return(1, &tee->rpc->used) > 1) {
			ret = -EBUSY;
			dev_err(tee->dev, "%s: ERROR Only one Supplicant is allowed\n",
					__func__);
			atomic_sub(1, &tee->rpc->used);
		}
	}

	return ret;
}

static void tee_supp_release(struct tee *tee)
{
	dev_dbg(_DEV(tee), "%s: appclient=\"%s\" pid=%d\n", __func__,
		current->comm, current->pid);

	BUG_ON(!tee->rpc);

	if ((atomic_read(&tee->rpc->used) == 1) &&
			(strncmp(_tee_supp_app_name, current->comm,
					strlen(_tee_supp_app_name)) == 0))
		atomic_sub(1, &tee->rpc->used);
}

//extern int mt_eint_set_deint(int eint_num, int irq_num);
static int tee_ctx_open(struct inode *inode, struct file *filp)
{
	struct tee_context *ctx;
	struct tee *tee;
	int ret;

	tee = container_of(filp->private_data, struct tee, miscdev);

	BUG_ON(!tee);
	BUG_ON(tee->miscdev.minor != iminor(inode));

	dev_dbg(_DEV(tee), "%s: > name=\"%s\"\n", __func__, tee->name);
	//printk("begin to invoke mt_eint_set_deint\n");
	//mt_eint_set_deint(6, 189);
	//printk("end of mt_eint_set_deint\n");
	ret = tee_supp_open(tee);
	dev_dbg(_DEV(tee), "%s: > tee_supp_open ended ret:%d\n", __func__, ret);		//wlaing
	if (ret)
		return ret;

	ctx = tee_context_create(tee);
	if (IS_ERR_OR_NULL(ctx))
	{
		dev_dbg(_DEV(tee), "%s: > tee_context_create error \n", __func__);		//wlaing
		return PTR_ERR(ctx);
	}
	dev_dbg(_DEV(tee), "%s: > tee_context_create ended right\n", __func__);		//wlaing
	
	ctx->usr_client = 1;
	filp->private_data = ctx;
#ifdef CONFIG_COMPAT
	ctx->client_is_32bit = 0;
#endif
	dev_dbg(_DEV(tee), "%s: < ctx=%p is created\n", __func__, (void *)ctx);

	return 0;
}

static int tee_ctx_release(struct inode *inode, struct file *filp)
{
	struct tee_context *ctx = filp->private_data;
	struct tee *tee;

	if (!ctx)
		return -EINVAL;

	BUG_ON(!ctx->tee);
	tee = ctx->tee;
	BUG_ON(tee->miscdev.minor != iminor(inode));

	dev_dbg(_DEV(tee), "%s: > ctx=%p\n", __func__, ctx);

	tee_context_destroy(ctx);
	tee_supp_release(tee);

	dev_dbg(_DEV(tee), "%s: < ctx=%p is destroyed\n", __func__, ctx);
	return 0;
}

static int tee_do_create_session(struct tee_context *ctx,
				 struct tee_cmd_io __user *u_cmd)
{
	int ret = -EINVAL;
	struct tee_cmd_io k_cmd;
	struct tee *tee;

	tee = ctx->tee;
	BUG_ON(!ctx->usr_client);

	dev_dbg(_DEV(tee), "%s: >\n", __func__);

	if (copy_from_user(&k_cmd, (void *)u_cmd, sizeof(struct tee_cmd_io))) {
		dev_err(_DEV(tee), "%s: copy_from_user failed\n", __func__);
		goto exit;
	}

	if (k_cmd.fd_sess > 0) {
		dev_err(_DEV(tee), "%s: invalid fd_sess %d\n", __func__,
			k_cmd.fd_sess);
		goto exit;
	}

	if ((k_cmd.op == NULL) || (k_cmd.uuid == NULL) ||
	    ((k_cmd.data != NULL) && (k_cmd.data_size == 0)) ||
	    ((k_cmd.data == NULL) && (k_cmd.data_size != 0))) {
		dev_err(_DEV(tee),
			"%s: op or/and data parameters are not valid\n",
			__func__);
		goto exit;
	}

	ret = tee_session_create_fd(ctx, &k_cmd);
	put_user(k_cmd.err, &u_cmd->err);
	put_user(k_cmd.origin, &u_cmd->origin);
	if (ret)
		goto exit;

	put_user(k_cmd.fd_sess, &u_cmd->fd_sess);

exit:
	dev_dbg(_DEV(tee), "%s: < ret=%d, sessfd=%d\n", __func__, ret,
		k_cmd.fd_sess);
	return ret;
}

static int tee_do_shm_alloc(struct tee_context *ctx,
			    struct tee_shm_io __user *u_shm)
{
	int ret = -EINVAL;
	struct tee_shm_io k_shm;
	struct tee *tee = ctx->tee;
	BUG_ON(!ctx->usr_client);

	dev_dbg(_DEV(tee), "< %s: \n", __func__);

	if (copy_from_user(&k_shm, (void *)u_shm, sizeof(struct tee_shm_io))) {
		dev_err(_DEV(tee), "%s: copy_from_user failed\n", __func__);
		goto exit;
	}
    dev_dbg(_DEV(tee), "< %s: k_shm.flags:%x\n", __func__, k_shm.flags);

	if ((k_shm.buffer != NULL) || (k_shm.fd_shm != 0) ||
	    /*(k_shm.flags & ~(tee->shm_flags)) ||*/
	    ((k_shm.flags & tee->shm_flags) == 0) || (k_shm.registered != 0)) {
		dev_err(_DEV(tee),
			"%s: shm parameters are not valid %p %d %08x %08x %d\n",
			__func__, (void *)k_shm.buffer, k_shm.fd_shm,
			(unsigned int)k_shm.flags, (unsigned int)tee->shm_flags,
			k_shm.registered);
		goto exit;
	}

	ret = tee_shm_alloc_fd(ctx, &k_shm);
	if (ret)
		goto exit;

	put_user(k_shm.fd_shm, &u_shm->fd_shm);

exit:
	dev_dbg(_DEV(tee), "%s: < ret=%d, shmfd=%d\n", __func__, ret,
		k_shm.fd_shm);
	return ret;
}

static int tee_do_get_fd_for_rpc_shm(struct tee_context *ctx,
				     struct tee_shm_io __user *u_shm)
{
	int ret = -EINVAL;
	struct tee_shm_io k_shm;
	struct tee *tee = ctx->tee;

	dev_dbg(_DEV(tee), "%s: >\n", __func__);
	BUG_ON(!ctx->usr_client);

	if (copy_from_user(&k_shm, (void *)u_shm, sizeof(struct tee_shm_io))) {
		dev_err(_DEV(tee), "%s: copy_from_user failed\n", __func__);
		goto exit;
	}

	if ((k_shm.buffer == NULL) || (k_shm.size == 0) || (k_shm.fd_shm != 0)
	    || (k_shm.flags & ~(tee->shm_flags))
	    || ((k_shm.flags & tee->shm_flags) == 0)
	    || (k_shm.registered != 0)) {
		dev_err(_DEV(tee), "%s: shm parameters are not valid\n",
			__func__);
		goto exit;
	}

	ret = tee_shm_get_fd(ctx, &k_shm);
	if (ret)
		goto exit;

	put_user(k_shm.fd_shm, &u_shm->fd_shm);

exit:
	dev_dbg(_DEV(tee), "%s: < ret=%d, shmfd=%d\n", __func__, ret,
		k_shm.fd_shm);
	return ret;
}

static long tee_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = -EINVAL;
	struct tee_context *ctx = filp->private_data;

	BUG_ON(!ctx);
	BUG_ON(!ctx->tee);

	dev_dbg(_DEV(ctx->tee), "%s: > cmd nr=%d\n", __func__, _IOC_NR(cmd));

	switch (cmd) {
	case TEE_OPEN_SESSION_IOC:
		ret =
		    tee_do_create_session(ctx, (struct tee_cmd_io __user *)arg);
		break;
	case TEE_ALLOC_SHM_IOC:
		ret = tee_do_shm_alloc(ctx, (struct tee_shm_io __user *)arg);
		break;
	case TEE_GET_FD_FOR_RPC_SHM_IOC:
		ret =
		    tee_do_get_fd_for_rpc_shm(ctx,
					      (struct tee_shm_io __user *)arg);
		break;
	default:
		ret = -ENOSYS;
		break;
	}

	dev_dbg(_DEV(ctx->tee), "%s: < cmd = %d,ret=%d\n", __func__,_IOC_NR(cmd), ret);

	return ret;
}

#ifdef CONFIG_COMPAT		// zhuhy, for 32bit process

static int tee_do_create_session_32(struct tee_context *ctx,
				 struct tee_cmd_io_32 __user *u_cmd)
{
	int ret = -EINVAL;

	struct tee_cmd_io k_cmd;
	struct tee_cmd_io_32 k_cmd_32;
	struct tee *tee;

	tee = ctx->tee;
	BUG_ON(!ctx->usr_client);

	dev_dbg(_DEV(tee), "%s: >\n", __func__);

//	printk("[%s] zhuhy, %d\n", __func__, __LINE__);

	if (copy_from_user(&k_cmd_32, (void *)u_cmd, sizeof(struct tee_cmd_io_32))) {
		dev_err(_DEV(tee), "%s: copy_from_user failed\n", __func__);
		goto exit;
	}
//	printk("[%s] zhuhy, %d\n", __func__, __LINE__);

	if (tee_cmd_io_32_to_64(&k_cmd, &k_cmd_32)){
		dev_err(_DEV(tee), "%s: tee_cmd_io_32_to_64 failed\n", __func__);
		goto exit;
	}	

/*
	printk("[%s] zhuhy, %d\n", __func__, __LINE__);
	dev_err(_DEV(tee),
		"%s: k_cmd.op = %p\n",
		__func__,
		k_cmd.op);
	dev_err(_DEV(tee),
		"%s: k_cmd.data = %p\n",
		__func__,
		k_cmd.data);	
	dev_err(_DEV(tee),
		"%s: k_cmd.uuid = %p\n",
		__func__,
		k_cmd.uuid);
	dev_err(_DEV(tee),
		"%s: k_cmd_32.uuid = %x\n",
		__func__,
		k_cmd_32.uuid);
*/
	if (k_cmd.fd_sess > 0) {
		dev_err(_DEV(tee), "%s: invalid fd_sess %d\n", __func__,
			k_cmd.fd_sess);
		goto exit;
	}

	if ((k_cmd.op == NULL) || (k_cmd.uuid == NULL) ||
	    ((k_cmd.data != NULL) && (k_cmd.data_size == 0)) ||
	    ((k_cmd.data == NULL) && (k_cmd.data_size != 0))) {
		dev_err(_DEV(tee),
			"%s: op or/and data parameters are not valid\n",
			__func__);
		goto exit;
	}

	ret = tee_session_create_fd(ctx, &k_cmd);
	put_user(k_cmd.err, &u_cmd->err);
	put_user(k_cmd.origin, &u_cmd->origin);
	if (ret)
		goto exit;

	put_user(k_cmd.fd_sess, &u_cmd->fd_sess);

exit:
	dev_dbg(_DEV(tee), "%s: < ret=%d, sessfd=%d\n", __func__, ret,
		k_cmd.fd_sess);
	return ret;
}

static int tee_do_shm_alloc_32(struct tee_context *ctx,
			    struct tee_shm_io_32 __user *u_shm)
{
	int ret = -EINVAL;
	struct tee_shm_io k_shm;
	struct tee_shm_io_32 k_shm_32;

	struct tee *tee = ctx->tee;
	BUG_ON(!ctx->usr_client);

	dev_dbg(_DEV(tee), "%s: >\n", __func__);
	if (copy_from_user(&k_shm_32, (void *)u_shm, sizeof(struct tee_shm_io))) {
		dev_err(_DEV(tee), "%s: copy_from_user failed\n", __func__);
		goto exit;
	}
	if (tee_shm_io_32_to_64(&k_shm, &k_shm_32)){
		dev_err(_DEV(tee), "%s: tee_cmd_io_32_to_64 failed\n", __func__);
		goto exit;
	}

	if ((k_shm.buffer != NULL) || (k_shm.fd_shm != 0) ||
	    /*(k_shm.flags & ~(tee->shm_flags)) ||*/
	    ((k_shm.flags & tee->shm_flags) == 0) || (k_shm.registered != 0)) {
		dev_err(_DEV(tee),
			"%s: shm parameters are not valid %p %d %08x %08x %d\n",
			__func__, (void *)k_shm.buffer, k_shm.fd_shm,
			(unsigned int)k_shm.flags, (unsigned int)tee->shm_flags,
			k_shm.registered);
		goto exit;
	}

	ret = tee_shm_alloc_fd(ctx, &k_shm);
	if (ret)
		goto exit;

	put_user(k_shm.fd_shm, &u_shm->fd_shm);

exit:
	dev_dbg(_DEV(tee), "%s: < ret=%d, shmfd=%d\n", __func__, ret,
		k_shm.fd_shm);
	return ret;
}

static int tee_do_get_fd_for_rpc_shm_32(struct tee_context *ctx,
				     struct tee_shm_io_32 __user *u_shm)
{
	int ret = -EINVAL;
	struct tee_shm_io k_shm;
	struct tee *tee = ctx->tee;
		struct tee_shm_io_32 k_shm_32;

	dev_dbg(_DEV(tee), "%s: >\n", __func__);
	BUG_ON(!ctx->usr_client);

	if (copy_from_user(&k_shm_32, (void *)u_shm, sizeof(struct tee_shm_io))) {
		dev_err(_DEV(tee), "%s: copy_from_user failed\n", __func__);
		goto exit;
	}
	if (tee_shm_io_32_to_64(&k_shm, &k_shm_32)){
		dev_err(_DEV(tee), "%s: tee_cmd_io_32_to_64 failed\n", __func__);
		goto exit;
	}

/*
	dev_err(_DEV(tee), "%s: shm parameters k_shm.buffer = %p\n",
		__func__,k_shm.buffer);
	dev_err(_DEV(tee), "%s: shm parameters k_shm.size = %x\n",
		__func__, (uint32_t)k_shm.size);
	dev_err(_DEV(tee), "%s: shm parameters k_shm.fd_shm = %d\n",
		__func__, k_shm.fd_shm);
*/

	if ((k_shm.buffer == NULL) || (k_shm.size == 0) || (k_shm.fd_shm != 0)
	    || (k_shm.flags & ~(tee->shm_flags))
	    || ((k_shm.flags & tee->shm_flags) == 0)
	    || (k_shm.registered != 0)) {
		dev_err(_DEV(tee), "%s: shm parameters are not valid\n",
			__func__);
		goto exit;
	}

	ret = tee_shm_get_fd(ctx, &k_shm);
	if (ret)
		goto exit;

	put_user(k_shm.fd_shm, &u_shm->fd_shm);

exit:
	dev_dbg(_DEV(tee), "%s: < ret=%d, shmfd=%d\n", __func__, ret,
		k_shm.fd_shm);
	return ret;
}

static long tee_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = -EINVAL;
//	int err = -EINVAL;
	struct tee_context *ctx = filp->private_data;
	long unsigned int cmd_64 = 0;
	cmd_64 = cmd;
	
	BUG_ON(!ctx);
	BUG_ON(!ctx->tee);

//	printk("zzzzzzzzzzzzzzzzzzzhuhy, it's ioctl32, cmd = %x TEE_OPEN_SESSION_IOC_32 = %x\n", cmd, (unsigned int)TEE_OPEN_SESSION_IOC_32);
	//...
	ctx->client_is_32bit = 1;

	switch (cmd_64) {
	case TEE_OPEN_SESSION_IOC_32:
		ret =
		    tee_do_create_session_32(ctx, (struct tee_cmd_io_32 __user *)arg);
		break;
	case TEE_ALLOC_SHM_IOC_32:
		ret = tee_do_shm_alloc_32(ctx, (struct tee_shm_io_32 __user *)arg);
		break;
	case TEE_GET_FD_FOR_RPC_SHM_IOC_32:
		ret =
		    tee_do_get_fd_for_rpc_shm_32(ctx,
					      (struct tee_shm_io_32 __user *)arg);
		break;
	default:
		ret = -ENOSYS;
		break;
	}

	dev_dbg(_DEV(ctx->tee), "%s: < ret=%d\n", __func__, ret);

	return ret;
}
#endif
const struct file_operations file_tee_fops = {
	.owner = THIS_MODULE,
	.read = tee_supp_read,
	.write = tee_supp_write,
	.open = tee_ctx_open,
	.release = tee_ctx_release,
	.unlocked_ioctl = tee_ioctl,	
#ifdef CONFIG_COMPAT
		.compat_ioctl = tee_compat_ioctl,
#endif
};

static void tee_plt_device_release(struct device *dev)
{
	pr_debug("%s: (dev=%p)....\n", __func__, dev);
}

struct tee *tee_core_alloc(struct device *dev, char *name, int id,
			   const struct tee_ops *ops, size_t len)
{
	struct tee *tee;

	if (!dev || !name || !ops ||
	    !ops->open || !ops->close || !ops->alloc || !ops->free)
		return NULL;

	tee = devm_kzalloc(dev, sizeof(struct tee) + len, GFP_KERNEL);
	if (!tee) {
		dev_err(dev, "%s: kzalloc failed\n", __func__);
		return NULL;
	}

	if (!dev->release)
		dev->release = tee_plt_device_release;

	tee->dev = dev;
	tee->id = id;
	tee->ops = ops;
	tee->priv = &tee[1];

	snprintf(tee->name, sizeof(tee->name), "nutlet%s%02d", name, tee->id);
	pr_info("TEE core: Alloc the misc device \"%s\" (id=%d)\n", tee->name,
		tee->id);

	tee->miscdev.parent = dev;
	tee->miscdev.minor = MISC_DYNAMIC_MINOR;
	tee->miscdev.name = tee->name;
	tee->miscdev.fops = &file_tee_fops;

	mutex_init(&tee->lock);
	atomic_set(&tee->refcount, 0);
	INIT_LIST_HEAD(&tee->list_ctx);
	INIT_LIST_HEAD(&tee->list_rpc_shm);

	tee->state = TEE_OFFLINE;

	tee_supp_init(tee);

	return tee;
}
EXPORT_SYMBOL(tee_core_alloc);

int tee_core_add(struct tee *tee)
{
	int rc = 0;

	if (!tee)
		return -EINVAL;

	rc = misc_register(&tee->miscdev);
	if (rc != 0) {
		pr_err("TEE Core: misc_register() failed name=\"%s\"\n",
		       tee->name);
		return rc;
	}

	dev_set_drvdata(tee->miscdev.this_device, tee);

	tee_init_sysfs(tee);
	tee_create_debug_dir(tee);

	/* Register a static reference on the class misc
	 * to allow finding device by class */
	BUG_ON(!tee->miscdev.this_device->class);
	if (misc_class)
		BUG_ON(misc_class != tee->miscdev.this_device->class);
	else
		misc_class = tee->miscdev.this_device->class;

	pr_info("TEE Core: Register the misc device \"%s\" (id=%d,minor=%d)\n",
		dev_name(tee->miscdev.this_device), tee->id,
		tee->miscdev.minor);
	return rc;
}
EXPORT_SYMBOL(tee_core_add);

int tee_core_del(struct tee *tee)
{
	if (tee) {
		pr_info("TEE Core: Destroy the misc device \"%s\" (id=%d)\n",
			dev_name(tee->miscdev.this_device), tee->id);

		tee_supp_deinit(tee);

		tee_cleanup_sysfs(tee);
		tee_delete_debug_dir(tee);

		if (tee->miscdev.minor != MISC_DYNAMIC_MINOR) {
			pr_info("TEE Core: Deregister the misc device \"%s\" (id=%d)\n",
			     dev_name(tee->miscdev.this_device), tee->id);
			misc_deregister(&tee->miscdev);
		}
	}

	return 0;
}
EXPORT_SYMBOL(tee_core_del);

static int __init tee_core_init(void)
{
	pr_info("\nTEE Core Framework initialization (ver %s)\n",
		_TEE_CORE_FW_VER);
	tee_init_debugfs();

	return 0;
}

static void __exit tee_core_exit(void)
{
	tee_exit_debugfs();
	pr_info("TEE Core Framework unregistered\n");
}

module_init(tee_core_init);
module_exit(tee_core_exit);

MODULE_AUTHOR("STMicroelectronics");
MODULE_DESCRIPTION("STM Secure TEE Framework/Core TEEC v1.0");
MODULE_SUPPORTED_DEVICE("");
MODULE_VERSION(_TEE_CORE_FW_VER);
MODULE_LICENSE("GPL");
