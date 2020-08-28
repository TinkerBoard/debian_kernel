/*
 * ch934x.c
 *
 * Copyright (c) 2019 SoldierJazz    <zhangj@wch.cn>
 *
 * USB driver for USB to serial chip ch934x
 *
 * Sponsored by SuSE
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#define DEBUG
#define VERBOSE_DEBUG

#undef DEBUG
#undef VERBOSE_DEBUG

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/serial.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/usb/cdc.h>
#include <asm/byteorder.h>
#include <asm/unaligned.h>
#include <linux/idr.h>
#include <linux/list.h>
#include <linux/version.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0))
#include <linux/sched/signal.h>
#endif

#include "ch934x.h"


#define DRIVER_AUTHOR "SoldierJazz"
#define DRIVER_DESC "USB driver for USB to serial chip ch9342, ch9344, etc."


static struct usb_driver ch934x_driver;
static struct tty_driver *ch934x_tty_driver;

static DEFINE_IDR(ch934x_minors);
static DEFINE_MUTEX(ch934x_minors_lock);


static void ch934x_tty_set_termios(struct tty_struct *tty,
				struct ktermios *termios_old);

static int ch934x_get_portnum(int index);

/*
 * ch934x_minors accessors
 */

/*
 * Look up an CH934X structure by minor. If found and not disconnected, increment
 * its refcount and return it with its mutex held.
 */
static struct ch934x *ch934x_get_by_index(unsigned int index)
{
	struct ch934x *ch934x;

	mutex_lock(&ch934x_minors_lock);
	ch934x = idr_find(&ch934x_minors, index / NUMSTEP);
	if (ch934x) {
		mutex_lock(&ch934x->mutex);
		if (ch934x->disconnected) {
			mutex_unlock(&ch934x->mutex);
			ch934x = NULL;
		} else {
			tty_port_get(&ch934x->ttyport[ch934x_get_portnum(index)].port);
			mutex_unlock(&ch934x->mutex);
		}
	}
	mutex_unlock(&ch934x_minors_lock);
	return ch934x;
}

/*
 * Try to find an available minor number and if found, associate it with 'ch934x'.
 */
static int ch934x_alloc_minor(struct ch934x *ch934x)
{
	int minor;

	mutex_lock(&ch934x_minors_lock);
	minor = idr_alloc(&ch934x_minors, ch934x, 0, CH934X_TTY_MINORS, GFP_KERNEL);
	mutex_unlock(&ch934x_minors_lock);

	return minor;
}

/* Release the minor number associated with 'ch934x'. */
static void ch934x_release_minor(struct ch934x *ch934x)
{
	mutex_lock(&ch934x_minors_lock);
	idr_remove(&ch934x_minors, ch934x->minor);
	mutex_unlock(&ch934x_minors_lock);
}

static int ch934x_get_portnum(int index) {
	return index % NUMSTEP;
}

/*
 * Functions for CH934X control messages.
 */
/*
static int ch934x_control_out(struct ch934x *ch934x, u8 request,
                u16 value, u16 index)
{	
    int retval;

    retval = usb_control_msg(ch934x->dev, usb_sndctrlpipe(ch934x->dev, 0),
        request, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_DIR_OUT,
        value, index, NULL, 0, DEFAULT_TIMEOUT);

    dev_vdbg(&ch934x->data->dev,
           "ch934x_control_out(%02x,%02x,%04x,%04x)\n",
            USB_DIR_OUT|0x40, request, value, index);

	return retval < 0 ? retval : 0;
}
*/
static int ch934x_control_in(struct ch934x *ch934x, 
                u8 request, u16 value, u16 index,
                char *buf, unsigned bufsize)
{	
    int retval;
    
	retval = usb_control_msg(ch934x->dev, usb_rcvctrlpipe(ch934x->dev, 0), request,
        USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_DIR_IN,
        value, index, buf, bufsize, DEFAULT_TIMEOUT);
    
    dev_vdbg(&ch934x->data->dev,
        "ch934x_control_in(%02x,%02x,%04x,%04x,%p,%u)\n",
        USB_DIR_IN|0x40, (int)request, (int)value, (int)index, buf,
        (int)bufsize);

	return retval;
}


/*
 * Functions for CH934X cmd messages.
 */

static int ch934x_cmd_out(struct ch934x *ch934x, u8 *buf,
                int count)
{	
    int retval, actual_len;

    retval = usb_bulk_msg(ch934x->dev, ch934x->cmdtx_endpoint,
                buf,
                min((unsigned int)count, ch934x->cmdsize),
                &actual_len, DEFAULT_TIMEOUT);

	if (retval ) {
		dev_dbg(&ch934x->data->dev,
			"usb_bulk_msg(send) failed, err %i\n", retval);
		return retval;
	}

	if (actual_len != count) {
		dev_dbg(&ch934x->data->dev, "only wrote %d of %d bytes\n",
			actual_len, count);
		return -1;
	}

    dev_dbg(&ch934x->data->dev, "ch934x_cmd_out--->\n");

	return actual_len;
}
/*
static int ch934x_cmd_in(struct ch934x *ch934x,
            u8 *buf, int count)
{	
	int retval, actual_len;

	if (buf == NULL)
		return -EINVAL;

	retval = usb_bulk_msg(ch934x->dev, ch934x->cmdrx_endpoint,
                buf,
                min((unsigned int)count, ch934x->cmdsize),
                &actual_len, DEFAULT_TIMEOUT);

	if (retval) {
		dev_dbg(&ch934x->data->dev,
			"usb_bulk_msg(recv) failed, err %i\n", retval);
		return retval;
	}

	if (actual_len != count) {
		dev_dbg(&ch934x->data->dev, "only read %d of %d bytes\n",
			actual_len, count);
		return -1;
	}
	
    dev_dbg(&ch934x->data->dev, "ch934x_cmd_in--->\n");
	
	return actual_len;
}
*/
static inline int ch934x_enum_portnum(struct ch934x *ch934x)
{
	char *buffer;
	int retval;
	const unsigned size = 4;

	buffer = kmalloc(size, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	retval = ch934x_control_in(ch934x, 0x96, 0x0000, 0, buffer, size);
	if (retval < 0)
		goto out;

	if (retval == 4) {
		retval = buffer[1] & 0x3f;
		dev_dbg(&ch934x->data->dev, "%s - portnum:%d\n", __func__, retval);
	} else
		retval = -EPROTO;

out:	
    kfree(buffer);
	return retval;
}

/* -------------------------------------------------------------------------- */

static int ch934x_configure(struct ch934x *ch934x, int portnum)
{
	char *buffer;
	int ret;
	u8 request;
	u8 regaddr;

	buffer = kmalloc(ch934x->cmdsize, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

    request = VENDOR_WRITE_TYPE;
    regaddr = 0x10 * portnum + 0x08;
    
    buffer[0] = request;
    buffer[1] = regaddr + 0x02;
    buffer[2] = 0x87;
    ret = ch934x_cmd_out(ch934x, buffer, 0x03);
    if (ret < 0)
        goto out;

    buffer[1] = regaddr + 0x03;
    buffer[2] = 0x03;
    ret = ch934x_cmd_out(ch934x, buffer, 0x03);
    if (ret < 0)
        goto out;

    buffer[1] = regaddr + 0x01;
    buffer[2] = 0x07;
    ret = ch934x_cmd_out(ch934x, buffer, 0x03);
    if (ret < 0)
        goto out;

    buffer[1] = regaddr + 0x04;
    buffer[2] = 0x08;
    ret = ch934x_cmd_out(ch934x, buffer, 0x03);
    if (ret < 0)
        goto out;
    
out:	
	kfree(buffer);
	return ret < 0 ? ret : 0;
}

/*
 * Write buffer management.
 * All of these assume proper locks taken by the caller.
 */

static int ch934x_wb_alloc(struct ch934x *ch934x)
{
	int i, wbn;
	struct ch934x_wb *wb;

	wbn = 0;
	i = 0;
	for (;;) {
		wb = &ch934x->wb[wbn];
		if (!wb->use) {
			wb->use = 1;
			return wbn;
		}
		wbn = (wbn + 1) % CH934X_NW;
		if (++i >= CH934X_NW)
			return -1;
	}
}

static int ch934x_wb_is_avail(struct ch934x *ch934x)
{
	int i, n;
	unsigned long flags;

	n = CH934X_NW;
	spin_lock_irqsave(&ch934x->write_lock, flags);
	for (i = 0; i < CH934X_NW; i++)
		n -= ch934x->wb[i].use;
	spin_unlock_irqrestore(&ch934x->write_lock, flags);
	return n;
}

/*
 * Finish write. Caller must hold ch934x->write_lock
 */
static void ch934x_write_done(struct ch934x *ch934x, struct ch934x_wb *wb)
{
	wb->use = 0;
	ch934x->transmitting--;
	usb_autopm_put_interface_async(ch934x->data);
}

/*
 * Poke write.
 *
 * the caller is responsible for locking
 */

static int ch934x_start_wb(struct ch934x *ch934x, struct ch934x_wb *wb)
{
	int rc;

	ch934x->transmitting++;

	wb->urb->transfer_buffer = wb->buf;
	wb->urb->transfer_dma = wb->dmah;
	wb->urb->transfer_buffer_length = wb->len;
	wb->urb->dev = ch934x->dev;

	rc = usb_submit_urb(wb->urb, GFP_ATOMIC);
	if (rc < 0) {
		dev_err(&ch934x->data->dev,
			"%s - usb_submit_urb(write bulk) failed: %d\n",
			__func__, rc);
		ch934x_write_done(ch934x, wb);
	}
	return rc;
}

/*
 * Status handlers for device responses
 */

/* bulk interface reports status changes with "bulk" transfers */
static void ch934x_cmd_irq(struct urb *urb)
{
	struct ch934x *ch934x = urb->context;
	unsigned char *data = urb->transfer_buffer;
	int len = urb->actual_length;
	int retval;
	int status = urb->status;
	int i;
	int portnum;

    dev_vdbg(&ch934x->data->dev, "%s, len:%d--->\n", __func__, len);
    
	switch (status) {
	case 0:
		/* success */
		for (i = 0; i < len; i += 3) {
			if ((*(data + i + 1) == 0x02) && (*(data + i + 2) == 0x00)) {
				portnum = (*(data + i) & 0x0f);
				if ((portnum > 0x03) || (portnum < 0x08)) {
					portnum -= 0x04;
					ch934x->write_empty[portnum] = true;
					dev_vdbg(&ch934x->data->dev, "%s portnum:%d\n", __func__, portnum);
					wake_up_interruptible(&ch934x->wioctl);
				} else
					break;
			} else {
				dev_dbg(&ch934x->data->dev,
						"%s - wrong status received",
						__func__);
			}
		}
		break;
	case -ECONNRESET:
	case -ENOENT:
	case -ESHUTDOWN:
		/* this urb is terminated, clean up */
		dev_dbg(&ch934x->data->dev,
				"%s - urb shutting down with status: %d\n",
				__func__, status);
		return;
	default:
		dev_dbg(&ch934x->data->dev,
				"%s - nonzero urb status received: %d\n",
				__func__, status);
		goto exit;
	}

	usb_mark_last_busy(ch934x->dev);

exit:
	retval = usb_submit_urb(urb, GFP_ATOMIC);
	if (retval && retval != -EPERM)
		dev_err(&ch934x->data->dev, "%s - usb_submit_urb failed: %d\n",
							__func__, retval);
}

static int ch934x_submit_read_urb(struct ch934x *ch934x, int index, gfp_t mem_flags)
{
	int res;

	if (!test_and_clear_bit(index, &ch934x->read_urbs_free))
		return 0;

	dev_vdbg(&ch934x->data->dev, "%s - urb %d\n", __func__, index);

	res = usb_submit_urb(ch934x->read_urbs[index], mem_flags);
	if (res) {
		if (res != -EPERM) {
			dev_err(&ch934x->data->dev,
					"%s - usb_submit_urb failed: %d\n",
					__func__, res);
		}
		set_bit(index, &ch934x->read_urbs_free);
		return res;
	}

	return 0;
}

static int ch934x_submit_read_urbs(struct ch934x *ch934x, gfp_t mem_flags)
{
	int res;
	int i;

	for (i = 0; i < ch934x->rx_buflimit; ++i) {
		res = ch934x_submit_read_urb(ch934x, i, mem_flags);
		if (res)
			return res;
	}

	return 0;
}

static void ch934x_process_read_urb(struct ch934x *ch934x, struct urb *urb)
{
	int size;
	char *buffer;
	int i = 0;
	int portnum;
	u8 usblen;

	if (!urb->actual_length)
		return;
	size = urb->actual_length;
	buffer = kmalloc(size, GFP_KERNEL);
	if (!buffer)
	    return;

	memcpy(buffer, urb->transfer_buffer, urb->actual_length);

    for (i = 0; i < size; i += 32) {
        portnum = *(buffer + i);
        if (portnum < 0x04 || portnum > 0x07) {
            break;
        }
        portnum -= 0x04;
        usblen = *(buffer + i + 1);
        if (usblen > 30) {
            break;
        }
        tty_insert_flip_string(&ch934x->ttyport[portnum].port, buffer + i + 2,
			usblen);
	    tty_flip_buffer_push(&ch934x->ttyport[portnum].port);
    }
    kfree(buffer);
}

static void ch934x_read_bulk_callback(struct urb *urb)
{
	struct ch934x_rb *rb = urb->context;
	struct ch934x *ch934x = rb->instance;
	unsigned long flags;
	int status = urb->status;

	dev_vdbg(&ch934x->data->dev, "%s - urb %d, len %d\n", __func__,
					rb->index, urb->actual_length);

	if (!ch934x->dev) {
		set_bit(rb->index, &ch934x->read_urbs_free);
		dev_dbg(&ch934x->data->dev, "%s - disconnected\n", __func__);
		return;
	}

	if (status) {
		set_bit(rb->index, &ch934x->read_urbs_free);
		dev_dbg(&ch934x->data->dev, "%s - non-zero urb status: %d\n",
							__func__, status);
		return;
	}

	usb_mark_last_busy(ch934x->dev);

	ch934x_process_read_urb(ch934x, urb);
	/*
	 * Unthrottle may run on another CPU which needs to see events
	 * in the same order. Submission has an implict barrier
	 */
	smp_mb__before_atomic();
	set_bit(rb->index, &ch934x->read_urbs_free);

	/* throttle device if requested by tty */
	spin_lock_irqsave(&ch934x->read_lock, flags);
	ch934x->throttled = ch934x->throttle_req;
	if (!ch934x->throttled) {
		spin_unlock_irqrestore(&ch934x->read_lock, flags);
		ch934x_submit_read_urb(ch934x, rb->index, GFP_ATOMIC);
	} else {
		spin_unlock_irqrestore(&ch934x->read_lock, flags);
	}
}

/* data interface wrote those outgoing bytes */
static void ch934x_write_bulk(struct urb *urb)
{
	struct ch934x_wb *wb = urb->context;
	struct ch934x *ch934x = wb->instance;
	unsigned long flags;
	int status = urb->status;

	if (status || (urb->actual_length != urb->transfer_buffer_length))
		dev_vdbg(&ch934x->data->dev, "%s - len %d/%d, status %d\n",
			__func__,
			urb->actual_length,
			urb->transfer_buffer_length,
			status);

	spin_lock_irqsave(&ch934x->write_lock, flags);
	ch934x_write_done(ch934x, wb);
	spin_unlock_irqrestore(&ch934x->write_lock, flags);
//	schedule_work(&ch934x->work);
}

static void ch934x_softint(struct work_struct *work)
{
	struct ch934x *ch934x = container_of(work, struct ch934x, work);

	dev_vdbg(&ch934x->data->dev, "%s\n", __func__);

	tty_port_tty_wakeup(&ch934x->ttyport[0].port);
}

/*
 * TTY handlers
 */

static int ch934x_tty_install(struct tty_driver *driver, struct tty_struct *tty)
{
	struct ch934x *ch934x;
	int retval;

	dev_dbg(tty->dev, "%s, index:%d\n", __func__, tty->index);

	ch934x = ch934x_get_by_index(tty->index);
	if (!ch934x)
		return -ENODEV;

	retval = tty_standard_install(driver, tty);
	if (retval)
		goto error_init_termios;

	tty->driver_data = ch934x;

	return 0;

error_init_termios:
	tty_port_put(&ch934x->ttyport[ch934x_get_portnum(tty->index)].port);
	return retval;
}


static int ch934x_tty_open(struct tty_struct *tty, struct file *filp)
{
	struct ch934x *ch934x = tty->driver_data;
	return tty_port_open(&ch934x->ttyport[ch934x_get_portnum(tty->index)].port, tty, filp);
}

static inline void tty_set_portdata(struct ch934x_ttyport *port, void *data)
{
	port->portdata = data;
}

static inline void *tty_get_portdata(struct ch934x_ttyport *port)
{
	return (port->portdata);
}

static void ch934x_port_dtr_rts(struct tty_port *port, int raise)
{
	struct ch934x_ttyport *ttyport = container_of(port, struct ch934x_ttyport, port);
	struct ch934x *ch934x = tty_get_portdata(ttyport);
	int val;

	dev_dbg(&ch934x->data->dev, "%s\n", __func__);
	return;

	if (raise)
		val = CH934X_CTRL_DTR | CH934X_CTRL_RTS;
	else
		val = 0;

	// FIXME: add missing ctrlout locking throughout driver 
	ch934x->ctrlout = val;
}

static int ch934x_port_activate(struct tty_port *port, struct tty_struct *tty)
{
	struct ch934x_ttyport *ttyport = container_of(port, struct ch934x_ttyport, port);
	struct ch934x *ch934x = tty_get_portdata(ttyport);
	int portnum = ttyport->portnum;
	int retval = -ENODEV;

	dev_dbg(&ch934x->data->dev, "%s\n", __func__);

	mutex_lock(&ch934x->mutex);
	if (ch934x->disconnected)
		goto disconnected;


	retval = usb_autopm_get_interface(ch934x->data);
	if (retval)
		goto error_get_interface;

	// FIXME: Why do we need this? Allocating 64K of physically contiguous
	// memory is really nasty...
	
	set_bit(TTY_NO_WRITE_SPLIT, &tty->flags);
	ch934x->data->needs_remote_wakeup = 1;

	retval = ch934x_configure(ch934x, portnum);
	if (retval)
		goto error_configure;

//	ch934x_tty_set_termios(tty, NULL);

	// Unthrottle device in case the TTY was closed while throttled.
	
	spin_lock_irq(&ch934x->read_lock);
	ch934x->throttled = 0;
	ch934x->throttle_req = 0;
	spin_unlock_irq(&ch934x->read_lock);

	usb_autopm_put_interface(ch934x->data);

	mutex_unlock(&ch934x->mutex);

	return 0;

error_configure:
	usb_autopm_put_interface(ch934x->data);
error_get_interface:
disconnected:
	mutex_unlock(&ch934x->mutex);

	return usb_translate_errors(retval);
}

static void ch934x_port_destruct(struct tty_port *port)
{
	struct ch934x_ttyport *ttyport = container_of(port, struct ch934x_ttyport, port);
	struct ch934x *ch934x = tty_get_portdata(ttyport);
	int portnum = ttyport->portnum;

	dev_dbg(&ch934x->data->dev, "%s, portnum:%d\n", __func__, portnum);

    if (portnum == (ch934x->num_ports - 1)) {
    	ch934x_release_minor(ch934x);
    	usb_put_intf(ch934x->data);
    	kfree(ch934x);
    }
}

static void ch934x_port_shutdown(struct tty_port *port)
{
	struct ch934x_ttyport *ttyport = container_of(port, struct ch934x_ttyport, port);
	struct ch934x *ch934x = tty_get_portdata(ttyport);
	int portnum = ttyport->portnum;

	dev_dbg(&ch934x->data->dev, "%s, portnum:%d\n", __func__, portnum); 
}

static void ch934x_tty_cleanup(struct tty_struct *tty)
{
	struct ch934x *ch934x = tty->driver_data;
	dev_dbg(&ch934x->data->dev, "%s\n", __func__);
	tty_port_put(&ch934x->ttyport[ch934x_get_portnum(tty->index)].port);
}

static void ch934x_tty_hangup(struct tty_struct *tty)
{
	struct ch934x *ch934x = tty->driver_data;
	dev_dbg(&ch934x->data->dev, "%s\n", __func__);
	tty_port_hangup(&ch934x->ttyport[ch934x_get_portnum(tty->index)].port);
}

static void ch934x_tty_close(struct tty_struct *tty, struct file *filp)
{
	struct ch934x *ch934x = tty->driver_data;
	dev_dbg(&ch934x->data->dev, "%s\n", __func__);
	tty_port_close(&ch934x->ttyport[ch934x_get_portnum(tty->index)].port, tty, filp);
}

static int ch934x_tty_write(struct tty_struct *tty,
					const unsigned char *buf, int count)
{
	struct ch934x *ch934x = tty->driver_data;
	int stat;
	unsigned long flags;
	int wbn;
	struct ch934x_wb *wb;
	int portnum = ch934x_get_portnum(tty->index);
	int timeout;
	DECLARE_WAITQUEUE(wait, current);

	if (!count)
		return 0;

	dev_vdbg(&ch934x->data->dev, "%s - count:%d\n", __func__, count);

	spin_lock_irqsave(&ch934x->write_lock, flags);
	wbn = ch934x_wb_alloc(ch934x);
	if (wbn < 0) {
		spin_unlock_irqrestore(&ch934x->write_lock, flags);
		return 0;
	}
	wb = &ch934x->wb[wbn];

	if (!ch934x->dev) {
		wb->use = 0;
		spin_unlock_irqrestore(&ch934x->write_lock, flags);
		return -ENODEV;
	}

	count = (count > ch934x->writesize - 3) ? ch934x->writesize - 3 : count;

    // add deal
	*wb->buf = 0x04 + ch934x_get_portnum(tty->index);
	*(wb->buf + 1) = count;
	*(wb->buf + 2) = count >> 8;
	
	memcpy(wb->buf + 3, buf, count);
	wb->len = count + 3;

	stat = usb_autopm_get_interface_async(ch934x->data);
	if (stat) {
		wb->use = 0;
		spin_unlock_irqrestore(&ch934x->write_lock, flags);
		return stat;
	}

	if (ch934x->susp_count) {
		usb_anchor_urb(wb->urb, &ch934x->delayed);
		spin_unlock_irqrestore(&ch934x->write_lock, flags);
		return count;
	}

	ch934x->write_empty[portnum] = false;
	stat = ch934x_start_wb(ch934x, wb);
	spin_unlock_irqrestore(&ch934x->write_lock, flags);

	if (stat < 0) {
		ch934x->write_empty[portnum] = true;
		return stat;
	}

	set_current_state(TASK_INTERRUPTIBLE);
	add_wait_queue(&ch934x->wioctl, &wait);
	spin_lock_irqsave(&ch934x->write_lock, flags);
	if (!ch934x->write_empty[portnum]) {
		spin_unlock_irqrestore(&ch934x->write_lock, flags);
		timeout = schedule_timeout(DEFAULT_TIMEOUT);
	} else {
		spin_unlock_irqrestore(&ch934x->write_lock, flags);
		set_current_state(TASK_RUNNING);
	}
	remove_wait_queue(&ch934x->wioctl, &wait);

	if (timeout <= 0) {
		dev_dbg(&ch934x->data->dev, "%s - schedule_timeout\n", __func__);
		return -ETIMEDOUT;
	} 

	if (signal_pending(current)) {
		dev_dbg(&ch934x->data->dev, "%s - signal_pending\n", __func__);
		return -EINTR;
	}

	return count;
}

static int ch934x_tty_write_room(struct tty_struct *tty)
{
	struct ch934x *ch934x = tty->driver_data;
	/*
	 * Do not let the line discipline to know that we have a reserve,
	 * or it might get too enthusiastic.
	 */
	return ch934x_wb_is_avail(ch934x) ? ch934x->writesize : 0;
}

static int ch934x_tty_chars_in_buffer(struct tty_struct *tty)
{
	struct ch934x *ch934x = tty->driver_data;
	/*
	 * if the device was unplugged then any remaining characters fell out
	 * of the connector ;)
	 */
	if (ch934x->disconnected)
		return 0;
	/*
	 * This is inaccurate (overcounts), but it works.
	 */
	return (CH934X_NW - ch934x_wb_is_avail(ch934x)) * ch934x->writesize;
}

static void ch934x_tty_throttle(struct tty_struct *tty)
{
	struct ch934x *ch934x = tty->driver_data;

	spin_lock_irq(&ch934x->read_lock);
	ch934x->throttle_req = 1;
	spin_unlock_irq(&ch934x->read_lock);
}

static void ch934x_tty_unthrottle(struct tty_struct *tty)
{
	struct ch934x *ch934x = tty->driver_data;
	unsigned int was_throttled;

	spin_lock_irq(&ch934x->read_lock);
	was_throttled = ch934x->throttled;
	ch934x->throttled = 0;
	ch934x->throttle_req = 0;
	spin_unlock_irq(&ch934x->read_lock);

	if (was_throttled)
		ch934x_submit_read_urbs(ch934x, GFP_KERNEL);
}

static int ch934x_get_baud_rate(int clockRate, int baudRate,
    unsigned char *baud1, unsigned char *baud2)
{
    int division, x2;
    
    if (baudRate <= 0 || baudRate >= 2000000) {
        return -1;
    }
    //caculate division from baudRate
    division = 10 * clockRate / 16 / baudRate;
    x2 = division % 10;
    division /= 10;
    if (x2 >= 5)
        division++;

    *baud1 = division;
    *baud2 = (unsigned char)(division >> 8);
    
    return 0;
}

static void ch934x_tty_set_termios(struct tty_struct *tty,
						struct ktermios *termios_old)
{
	struct ch934x *ch934x = tty->driver_data;
	struct ktermios *termios = &tty->termios;
	struct usb_cdc_line_coding newline;
	int newctrl = ch934x->ctrlout;
	int ret;

	unsigned char baud1 = 0;
    unsigned char baud2 = 0;
    unsigned char databit = 0, paritybit = 0, stopbit = 0;
    unsigned char pendent = 0x00;
    int clockrate = 1843200;

    u8 regaddr;
	char *buffer;
	buffer = kzalloc(ch934x->cmdsize, GFP_KERNEL);
	if (!buffer)
		return;

	dev_dbg(tty->dev, "%s\n", __func__);

	newline.dwDTERate = tty_get_baud_rate(tty);

	if (newline.dwDTERate > 115200) {
        pendent = 0x01;
        clockrate = 44236800;
    }
    ch934x_get_baud_rate(clockrate, newline.dwDTERate, &baud1, &baud2);

	newline.bCharFormat = termios->c_cflag & CSTOPB ? 2 : 1;
	if (newline.bCharFormat == 2)
	    stopbit = 0x04;
	    
	newline.bParityType = termios->c_cflag & PARENB ?
				(termios->c_cflag & PARODD ? 1 : 2) +
				(termios->c_cflag & CMSPAR ? 2 : 0) : 0;

	switch (newline.bParityType) {
    case 0x01:
        paritybit = 0x09;
        dev_vdbg(&ch934x->data->dev, "parity = odd");
        break;
    case 0x02:
		paritybit = (0x01<<4) + 0x09;
		dev_vdbg(&ch934x->data->dev, "parity = even");
        break;
    case 0x03:
		paritybit = (0x02<<4) + 0x09;
		dev_vdbg(&ch934x->data->dev, "parity = mark");
        break;
    case 0x04:
		paritybit = (0x03<<4) + 0x09;
		dev_vdbg(&ch934x->data->dev, "parity = space");
        break;
    default:
        paritybit = 0x00;
        dev_vdbg(&ch934x->data->dev, "parity = none");
        break;
	}
		
	switch (termios->c_cflag & CSIZE) {
	case CS5:
		newline.bDataBits = 5;
		databit = 0x00;
		break;
	case CS6:
		newline.bDataBits = 6;
		databit = 0x01;
		break;
	case CS7:
		newline.bDataBits = 7;
		databit = 0x02;
		break;
	case CS8:
	default:
		newline.bDataBits = 8;
		databit = 0x03;
		break;
	}
	
	/* FIXME: Needs to clear unsupported bits in the termios */
	ch934x->clocal = ((termios->c_cflag & CLOCAL) != 0);

	if (C_BAUD(tty) == B0) {
		newline.dwDTERate = ch934x->line[ch934x_get_portnum(tty->index)].dwDTERate;
		newctrl &= ~(CH934X_CTRL_DTR | CH934X_CTRL_RTS);
	} else if (termios_old && (termios_old->c_cflag & CBAUD) == B0) {
		newctrl |=  CH934X_CTRL_DTR | CH934X_CTRL_RTS;
	}

	if (memcmp(&ch934x->line[ch934x_get_portnum(tty->index)], &newline, sizeof newline) == 0)
		return;

	regaddr = 0x10 * ch934x_get_portnum(tty->index) + 0x08;

    memset(buffer, 0x00, ch934x->cmdsize);
    buffer[0] = VENDOR_INIT_TYPE;
    buffer[1] = regaddr + 0x01;
    buffer[2] = pendent + 0x50;
    ret = ch934x_cmd_out(ch934x, buffer, 0x03);
    if (ret < 0)
        goto out;

    memset(buffer, 0x00, ch934x->cmdsize);
    buffer[0] = VENDOR_SET_TYPE;
    buffer[1] = regaddr + 0x03;
    buffer[2] = baud1;
    buffer[3] = baud2;
    ret = ch934x_cmd_out(ch934x, buffer, 0x06);
    if (ret < 0)
        goto out;

    memset(buffer, 0x00, ch934x->cmdsize);
    buffer[0] = VENDOR_WRITE_TYPE;
    buffer[1] = regaddr + 0x03;
    buffer[2] = databit | paritybit | stopbit;
    ret = ch934x_cmd_out(ch934x, buffer, 0x03);
    if (ret < 0)
        goto out;

//	if (newctrl != ch934x->ctrlout)
//		ch34x_set_control(ch934x, ch934x->ctrlout = newctrl);

	if (memcmp(&ch934x->line[ch934x_get_portnum(tty->index)], &newline, sizeof newline)) {
		memcpy(&ch934x->line[ch934x_get_portnum(tty->index)], &newline, sizeof newline);
		dev_vdbg(&ch934x->data->dev, "%s - set line: %d %d %d %d\n",
			__func__,
			newline.dwDTERate,
			newline.bCharFormat, newline.bParityType,
			newline.bDataBits);
	}

//	if (termios->c_cflag & CRTSCTS)
//	    ch34x_control_out(ch934x, VENDOR_WRITE, 0x2727, 0x0101);
out:
    kfree(buffer);
    return;
}

static const struct tty_port_operations ch934x_port_ops = {
	.dtr_rts = ch934x_port_dtr_rts,
	.shutdown = ch934x_port_shutdown,
	.activate = ch934x_port_activate,
	.destruct = ch934x_port_destruct,
};

/*
 * USB probe and disconnect routines.
 */

/* Little helpers: write/read buffers free */
static void ch934x_write_buffers_free(struct ch934x *ch934x)
{
	int i;
	struct ch934x_wb *wb;
	struct usb_device *usb_dev = interface_to_usbdev(ch934x->data);

	for (wb = &ch934x->wb[0], i = 0; i < CH934X_NW; i++, wb++)
		usb_free_coherent(usb_dev, ch934x->writesize, wb->buf, wb->dmah);
}

static void ch934x_read_buffers_free(struct ch934x *ch934x)
{
	struct usb_device *usb_dev = interface_to_usbdev(ch934x->data);
	int i;

	for (i = 0; i < ch934x->rx_buflimit; i++)
		usb_free_coherent(usb_dev, ch934x->readsize,
			  ch934x->read_buffers[i].base, ch934x->read_buffers[i].dma);
}

/* Little helper: write buffers allocate */
static int ch934x_write_buffers_alloc(struct ch934x *ch934x)
{
	int i;
	struct ch934x_wb *wb;

	for (wb = &ch934x->wb[0], i = 0; i < CH934X_NW; i++, wb++) {
		wb->buf = usb_alloc_coherent(ch934x->dev, ch934x->writesize, GFP_KERNEL,
		    &wb->dmah);
		if (!wb->buf) {
			while (i != 0) {
				--i;
				--wb;
				usb_free_coherent(ch934x->dev, ch934x->writesize,
				    wb->buf, wb->dmah);
			}
			return -ENOMEM;
		}
	}
	return 0;
}

static int ch934x_probe(struct usb_interface *intf,
		     const struct usb_device_id *id)
{
	struct usb_interface *data_interface;
	struct usb_endpoint_descriptor *epcmdread = NULL;
	struct usb_endpoint_descriptor *epcmdwrite = NULL;
	struct usb_endpoint_descriptor *epread = NULL;
	struct usb_endpoint_descriptor *epwrite = NULL;
	struct usb_device *usb_dev = interface_to_usbdev(intf);
	struct ch934x *ch934x;
	int minor;
	int portnum;
	int cmdsize, readsize;
	u8 *buf;
	unsigned long quirks;
	int num_rx_buf = CH934X_NR;
	int i;
	struct device *tty_dev;
	int rv = -ENOMEM;

	/* normal quirks */
	quirks = (unsigned long)id->driver_info;

	/* handle quirks deadly to normal probing*/
	data_interface = usb_ifnum_to_if(usb_dev, 0);

//	if (usb_interface_claimed(data_interface)) {
		/* valid in this context */
//		dev_dbg(&intf->dev, "The data interface isn't available\n");
//		return -EBUSY;
//	}

	if (data_interface->cur_altsetting->desc.bNumEndpoints < 3 ||
	    data_interface->cur_altsetting->desc.bNumEndpoints == 0)
		return -EINVAL;

	epread = &data_interface->cur_altsetting->endpoint[0].desc;
	epwrite = &data_interface->cur_altsetting->endpoint[1].desc;
	epcmdread = &data_interface->cur_altsetting->endpoint[2].desc;
	epcmdwrite = &data_interface->cur_altsetting->endpoint[3].desc;

	/* workaround for switched endpoints */
	if (!usb_endpoint_dir_in(epread)) {
		/* descriptors are swapped */
		dev_dbg(&intf->dev,
			"The data interface has switched endpoints\n");
		swap(epread, epwrite);
	}

	if (!usb_endpoint_dir_in(epcmdread)) {
		/* descriptors are swapped */
		dev_dbg(&intf->dev,
			"The data interface has switched endpoints\n");
		swap(epcmdread, epcmdwrite);
	}

	dev_dbg(&intf->dev, "ch934x interfaces are valid\n");

	ch934x = kzalloc(sizeof(struct ch934x), GFP_KERNEL);
	if (ch934x == NULL)
		goto alloc_fail;
		
    minor = ch934x_alloc_minor(ch934x);
	if (minor < 0) {
		dev_err(&intf->dev, "no more free ch934x devices\n");
		kfree(ch934x);
		return -ENODEV;
	}

	cmdsize = usb_endpoint_maxp(epcmdread);
	readsize = usb_endpoint_maxp(epread);
	ch934x->writesize = usb_endpoint_maxp(epwrite) * 20;
	ch934x->data = data_interface;
	ch934x->minor = minor;
	ch934x->dev = usb_dev;
	ch934x->cmdsize = cmdsize;
	ch934x->readsize = readsize;
	ch934x->rx_buflimit = num_rx_buf;

	dev_dbg(&intf->dev, "epcmdread: %d, epcmdwrite: %d, epread: %d, epwrite: %d\n",
		usb_endpoint_maxp(epcmdread), usb_endpoint_maxp(epcmdwrite),
		usb_endpoint_maxp(epread), usb_endpoint_maxp(epwrite));

	INIT_WORK(&ch934x->work, ch934x_softint);
	init_waitqueue_head(&ch934x->wioctl);
	spin_lock_init(&ch934x->write_lock);
	spin_lock_init(&ch934x->read_lock);
	mutex_init(&ch934x->mutex);
	
	ch934x->rx_endpoint = usb_rcvbulkpipe(usb_dev, epread->bEndpointAddress);
    ch934x->tx_endpoint = usb_sndbulkpipe(usb_dev, epwrite->bEndpointAddress);
    ch934x->cmdrx_endpoint = usb_rcvbulkpipe(usb_dev, epcmdread->bEndpointAddress);
    ch934x->cmdtx_endpoint = usb_sndbulkpipe(usb_dev, epcmdwrite->bEndpointAddress);

	portnum = ch934x_enum_portnum(ch934x);
	if (portnum <= 0) {
		dev_err(&intf->dev, "wrong port number\n");
		return -ENODEV;
		goto enum_fail;
	}
    ch934x->num_ports = portnum;
	
	for (i = 0; i < portnum; i++) {
	    tty_port_init(&ch934x->ttyport[i].port);
	    ch934x->ttyport[i].port.ops = &ch934x_port_ops;
	    tty_set_portdata(&ch934x->ttyport[i], ch934x);
	    ch934x->ttyport[i].portnum = i;
	    ch934x->write_empty[i] = true;
	}
	init_usb_anchor(&ch934x->delayed);
	ch934x->quirks = quirks;


	buf = usb_alloc_coherent(usb_dev, cmdsize, GFP_KERNEL, &ch934x->cmdread_dma);
	if (!buf)
		goto alloc_fail2;
	ch934x->cmdread_buffer= buf;


	if (ch934x_write_buffers_alloc(ch934x) < 0)
		goto alloc_fail4;

	ch934x->cmdreadurb= usb_alloc_urb(0, GFP_KERNEL);
	if (!ch934x->cmdreadurb)
		goto alloc_fail5;

	for (i = 0; i < num_rx_buf; i++) {
		struct ch934x_rb *rb = &(ch934x->read_buffers[i]);
		struct urb *urb;

		rb->base = usb_alloc_coherent(ch934x->dev, readsize, GFP_KERNEL,
								&rb->dma);
		if (!rb->base)
			goto alloc_fail6;
		rb->index = i;
		rb->instance = ch934x;

		urb = usb_alloc_urb(0, GFP_KERNEL);
		if (!urb)
			goto alloc_fail6;

		urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
		urb->transfer_dma = rb->dma;
		
		usb_fill_bulk_urb(urb, ch934x->dev,
				  ch934x->rx_endpoint,
				  rb->base,
				  ch934x->readsize,
				  ch934x_read_bulk_callback, rb);

		ch934x->read_urbs[i] = urb;
		__set_bit(i, &ch934x->read_urbs_free);
	}
	for (i = 0; i < CH934X_NW; i++) {
		struct ch934x_wb *snd = &(ch934x->wb[i]);

		snd->urb = usb_alloc_urb(0, GFP_KERNEL);
		if (snd->urb == NULL)
			goto alloc_fail7;

		usb_fill_bulk_urb(snd->urb, usb_dev,
			usb_sndbulkpipe(usb_dev, epwrite->bEndpointAddress),
			NULL, ch934x->writesize, ch934x_write_bulk, snd);
		snd->urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
		snd->instance = ch934x;
	}

	usb_set_intfdata(intf, ch934x);

	usb_fill_bulk_urb(ch934x->cmdreadurb, usb_dev,
			 usb_rcvbulkpipe(usb_dev, epcmdread->bEndpointAddress),
			 ch934x->cmdread_buffer, cmdsize, ch934x_cmd_irq, ch934x);
	ch934x->cmdreadurb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
	ch934x->cmdreadurb->transfer_dma = ch934x->cmdread_dma;
			 
	dev_info(&intf->dev, "ttyWCHUSB from %d - %d: USB WCH device\n", NUMSTEP * minor,
		NUMSTEP * minor + portnum - 1);

//	ch934x->line.dwDTERate = cpu_to_le32(9600);
//	ch934x->line.bDataBits = 8;
//	ch934x_set_line(ch934x, &ch934x->line);

	usb_driver_claim_interface(&ch934x_driver, data_interface, ch934x);
	usb_set_intfdata(data_interface, ch934x);

	usb_get_intf(data_interface);

    for (i = 0; i < portnum; i++) {
	    tty_dev = tty_port_register_device(&ch934x->ttyport[i].port, ch934x_tty_driver, 
	        NUMSTEP * minor + i, &data_interface->dev);
	    if (IS_ERR(tty_dev)) {
			rv = PTR_ERR(tty_dev);
			goto alloc_fail7;
		}
    }

	if (quirks & CLEAR_HALT_CONDITIONS) {
		usb_clear_halt(usb_dev, usb_rcvbulkpipe(usb_dev, epread->bEndpointAddress));
		usb_clear_halt(usb_dev, usb_sndbulkpipe(usb_dev, epwrite->bEndpointAddress));
	}

    // deal with urb when usb plugged in
	rv = usb_submit_urb(ch934x->cmdreadurb, GFP_KERNEL);
	if (rv) {
		dev_err(&ch934x->data->dev,
			"%s - usb_submit_urb(ctrl cmd) failed\n", __func__);
		goto error_submit_urb;
	}

	rv = ch934x_submit_read_urbs(ch934x, GFP_KERNEL);
	if (rv)
		goto error_submit_read_urbs;

	dev_dbg(&intf->dev, "ch934x_probe finished!\n");

	return 0;

error_submit_read_urbs:
	for (i = 0; i < ch934x->rx_buflimit; i++)
		usb_kill_urb(ch934x->read_urbs[i]);
error_submit_urb:
	usb_kill_urb(ch934x->cmdreadurb);	
alloc_fail7:
	usb_set_intfdata(intf, NULL);
	for (i = 0; i < CH934X_NW; i++)
		usb_free_urb(ch934x->wb[i].urb);
alloc_fail6:
	for (i = 0; i < num_rx_buf; i++)
		usb_free_urb(ch934x->read_urbs[i]);
	ch934x_read_buffers_free(ch934x);
	usb_free_urb(ch934x->cmdreadurb);
alloc_fail5:
	ch934x_write_buffers_free(ch934x);
alloc_fail4:
	usb_free_coherent(usb_dev, cmdsize, ch934x->cmdread_buffer, ch934x->cmdread_dma);
enum_fail:
alloc_fail2:
	ch934x_release_minor(ch934x);
	kfree(ch934x);
alloc_fail:
	return rv;
}


static void stop_data_traffic(struct ch934x *ch934x)
{
	int i;
	struct urb *urb;
	struct ch934x_wb *wb;

	dev_dbg(&ch934x->data->dev, "%s\n", __func__);

	usb_autopm_get_interface_no_resume(ch934x->data);
	ch934x->data->needs_remote_wakeup = 0;
	usb_autopm_put_interface(ch934x->data);

	for (;;) {
		urb = usb_get_from_anchor(&ch934x->delayed);
		if (!urb)
			break;
		wb = urb->context;
		wb->use = 0;
		usb_autopm_put_interface_async(ch934x->data);
	}

	usb_kill_urb(ch934x->cmdreadurb);
	for (i = 0; i < CH934X_NW; i++)
		usb_kill_urb(ch934x->wb[i].urb);
	for (i = 0; i < ch934x->rx_buflimit; i++)
		usb_kill_urb(ch934x->read_urbs[i]);

	cancel_work_sync(&ch934x->work);
}

static void ch934x_disconnect(struct usb_interface *intf)
{
	struct ch934x *ch934x = usb_get_intfdata(intf);
	struct usb_device *usb_dev = interface_to_usbdev(intf);
	struct tty_struct *tty;
	int i;

	dev_dbg(&intf->dev, "%s\n", __func__);

	/* sibling interface is already cleaning up */
	if (!ch934x)
		return;

	mutex_lock(&ch934x->mutex);
	ch934x->disconnected = true;
	wake_up_interruptible(&ch934x->wioctl);
	usb_set_intfdata(ch934x->data, NULL);
	mutex_unlock(&ch934x->mutex);

    for (i = 0; i < ch934x->num_ports; i++) {
    	tty = tty_port_tty_get(&ch934x->ttyport[i].port);
    	if (tty) {
    		tty_vhangup(tty);
    		tty_kref_put(tty);
    	}
    }

	stop_data_traffic(ch934x);

    for (i = 0; i < ch934x->num_ports; i++) {
    	tty_unregister_device(ch934x_tty_driver, NUMSTEP * ch934x->minor + i);
    }

	usb_free_urb(ch934x->cmdreadurb);
	for (i = 0; i < CH934X_NW; i++)
		usb_free_urb(ch934x->wb[i].urb);
	for (i = 0; i < ch934x->rx_buflimit; i++)
		usb_free_urb(ch934x->read_urbs[i]);
	ch934x_write_buffers_free(ch934x);
	usb_free_coherent(usb_dev, ch934x->cmdsize, ch934x->cmdread_buffer, ch934x->cmdread_dma);
	ch934x_read_buffers_free(ch934x);

	usb_driver_release_interface(&ch934x_driver, ch934x->data);

    for (i = 0; i < ch934x->num_ports; i++)
	    tty_port_put(&ch934x->ttyport[i].port);

	dev_info(&intf->dev, "%s\n", "ch934x usb device disconnect.");
}

#ifdef CONFIG_PM
/*static int ch934x_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct ch934x *ch934x = usb_get_intfdata(intf);
	int cnt;

	spin_lock_irq(&ch934x->write_lock);
	if (PMSG_IS_AUTO(message)) {
		if (ch934x->transmitting) {
			spin_unlock_irq(&ch934x->write_lock);
			return -EBUSY;
		}
	}
	cnt = ch934x->susp_count++;
	spin_unlock_irq(&ch934x->write_lock);

	if (cnt)
		return 0;

	stop_data_traffic(ch934x);

	return 0;
}

static int ch934x_resume(struct usb_interface *intf)
{
	struct ch934x *ch934x = usb_get_intfdata(intf);
	struct urb *urb;
	int rv = 0;

	spin_lock_irq(&ch934x->write_lock);

	if (--ch934x->susp_count)
		goto out;

	if (test_bit(ASYNCB_INITIALIZED, &ch934x->ttyport[0].port.flags)) {
		rv = usb_submit_urb(ch934x->cmdreadurb, GFP_ATOMIC);

		for (;;) {
			urb = usb_get_from_anchor(&ch934x->delayed);
			if (!urb)
				break;

			ch934x_start_wb(ch934x, urb->context);
		}
*/
		/*
		 * delayed error checking because we must
		 * do the write path at all cost
		 */
/*		if (rv < 0)
			goto out;

		rv = ch934x_submit_read_urbs(ch934x, GFP_ATOMIC);
	}
out:
	spin_unlock_irq(&ch934x->write_lock);

	return rv;
}

static int ch934x_reset_resume(struct usb_interface *intf)
{
	struct ch934x *ch934x = usb_get_intfdata(intf);

	if (test_bit(ASYNCB_INITIALIZED, &ch934x->ttyport[0].port.flags))
		tty_port_tty_hangup(&ch934x->ttyport[0].port, false);

	return ch934x_resume(intf);
}
*/
#endif /* CONFIG_PM */

/*
 * USB driver structure.
 */

static const struct usb_device_id ch934x_ids[] = {
	/* quirky and broken devices */
	{ USB_DEVICE(0x1a86, 0xe019), /* ch9342 chip */
	.driver_info = CLEAR_HALT_CONDITIONS, },/* has no union descriptor */

	{ USB_DEVICE(0x1a86, 0xe018), /* ch9344 chip */
	.driver_info = CLEAR_HALT_CONDITIONS, },/* has no union descriptor */

	{ }
};

MODULE_DEVICE_TABLE(usb, ch934x_ids);

static struct usb_driver ch934x_driver = {
	.name =		"usb_wch",
	.probe =	ch934x_probe,
	.disconnect =	ch934x_disconnect,
#ifdef CONFIG_PM
//	.suspend =	ch934x_suspend,
//	.resume =	ch934x_resume,
//	.reset_resume =	ch934x_reset_resume,
#endif
	.id_table =	ch934x_ids,
#ifdef CONFIG_PM
	.supports_autosuspend = 1,
#endif
	.disable_hub_initiated_lpm = 1,
};

/*
 * TTY driver structures.
 */

static const struct tty_operations ch934x_ops = {
	.install =		ch934x_tty_install,
	.open =			ch934x_tty_open,
	.close =		ch934x_tty_close,
	.cleanup =		ch934x_tty_cleanup,
	.hangup =		ch934x_tty_hangup,
	.write =		ch934x_tty_write,
	.write_room =		ch934x_tty_write_room,
	.throttle =		ch934x_tty_throttle,
	.unthrottle =		ch934x_tty_unthrottle,
	.chars_in_buffer =	ch934x_tty_chars_in_buffer,
	.set_termios =		ch934x_tty_set_termios,
};

/*
 * Init / exit.
 */

static int __init ch934x_init(void)
{
	int retval;

	ch934x_tty_driver = alloc_tty_driver(CH934X_TTY_MINORS);
	if (!ch934x_tty_driver)
		return -ENOMEM;
	ch934x_tty_driver->driver_name = "usbch934x",
	ch934x_tty_driver->name = "ttyWCHUSB",
	ch934x_tty_driver->major = CH934X_TTY_MAJOR,
	ch934x_tty_driver->minor_start = 0,
	ch934x_tty_driver->type = TTY_DRIVER_TYPE_SERIAL,
	ch934x_tty_driver->subtype = SERIAL_TYPE_NORMAL,
	ch934x_tty_driver->flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;
	ch934x_tty_driver->init_termios = tty_std_termios;
	ch934x_tty_driver->init_termios.c_cflag = B9600 | CS8 | CREAD |
								HUPCL | CLOCAL;
	tty_set_operations(ch934x_tty_driver, &ch934x_ops);

	retval = tty_register_driver(ch934x_tty_driver);
	if (retval) {
		put_tty_driver(ch934x_tty_driver);
		return retval;
	}

	retval = usb_register(&ch934x_driver);
	if (retval) {
		tty_unregister_driver(ch934x_tty_driver);
		put_tty_driver(ch934x_tty_driver);
		return retval;
	}
	printk(KERN_INFO KBUILD_MODNAME ": " DRIVER_DESC "\n");

	return 0;
}

static void __exit ch934x_exit(void)
{
	usb_deregister(&ch934x_driver);
	tty_unregister_driver(ch934x_tty_driver);
	put_tty_driver(ch934x_tty_driver);
	idr_destroy(&ch934x_minors);
	printk(KERN_INFO KBUILD_MODNAME ": " "ch934x driver exit.\n");
}

module_init(ch934x_init);
module_exit(ch934x_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
MODULE_ALIAS_CHARDEV_MAJOR(CH934X_TTY_MAJOR);


