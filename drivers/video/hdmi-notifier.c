#include <linux/export.h>
#include <linux/hdmi-notifier.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/list.h>

static LIST_HEAD(hdmi_notifiers);
static DEFINE_MUTEX(hdmi_notifiers_lock);

struct hdmi_notifier *hdmi_notifier_get(struct device *dev)
{
	struct hdmi_notifier *n;

	mutex_lock(&hdmi_notifiers_lock);
	list_for_each_entry(n, &hdmi_notifiers, head) {
		if (n->dev == dev) {
			mutex_unlock(&hdmi_notifiers_lock);
			kref_get(&n->kref);
			return n;
		}
	}
	n = kzalloc(sizeof(*n), GFP_KERNEL);
	if (!n)
		goto unlock;
	n->dev = dev;
	mutex_init(&n->lock);
	BLOCKING_INIT_NOTIFIER_HEAD(&n->notifiers);
	kref_init(&n->kref);
	list_add_tail(&n->head, &hdmi_notifiers);
unlock:
	mutex_unlock(&hdmi_notifiers_lock);
	return n;
}
EXPORT_SYMBOL_GPL(hdmi_notifier_get);

static void hdmi_notifier_release(struct kref *kref)
{
	struct hdmi_notifier *n =
		container_of(kref, struct hdmi_notifier, kref);

	mutex_lock(&hdmi_notifiers_lock);
	list_del(&n->head);
	mutex_unlock(&hdmi_notifiers_lock);
	kfree(n->edid);
	kfree(n);
}

void hdmi_notifier_put(struct hdmi_notifier *n)
{
	kref_put(&n->kref, hdmi_notifier_release);
}
EXPORT_SYMBOL_GPL(hdmi_notifier_put);

int hdmi_notifier_register(struct hdmi_notifier *n, struct notifier_block *nb)
{
	int ret = blocking_notifier_chain_register(&n->notifiers, nb);

	if (ret)
		return ret;
	kref_get(&n->kref);
	mutex_lock(&n->lock);
	if (n->connected) {
		blocking_notifier_call_chain(&n->notifiers, HDMI_CONNECTED, n);
		if (n->edid_size)
			blocking_notifier_call_chain(&n->notifiers, HDMI_NEW_EDID, n);
		if (n->has_eld)
			blocking_notifier_call_chain(&n->notifiers, HDMI_NEW_ELD, n);
	}
	mutex_unlock(&n->lock);
	return 0;
}
EXPORT_SYMBOL_GPL(hdmi_notifier_register);

int hdmi_notifier_unregister(struct hdmi_notifier *n, struct notifier_block *nb)
{
	int ret = blocking_notifier_chain_unregister(&n->notifiers, nb);

	if (ret == 0)
		hdmi_notifier_put(n);
	return ret;
}
EXPORT_SYMBOL_GPL(hdmi_notifier_unregister);

void hdmi_event_connect(struct hdmi_notifier *n)
{
	mutex_lock(&n->lock);
	n->connected = true;
	blocking_notifier_call_chain(&n->notifiers, HDMI_CONNECTED, n);
	mutex_unlock(&n->lock);
}
EXPORT_SYMBOL_GPL(hdmi_event_connect);

void hdmi_event_disconnect(struct hdmi_notifier *n)
{
	mutex_lock(&n->lock);
	n->connected = false;
	n->has_eld = false;
	n->edid_size = 0;
	blocking_notifier_call_chain(&n->notifiers, HDMI_DISCONNECTED, n);
	mutex_unlock(&n->lock);
}
EXPORT_SYMBOL_GPL(hdmi_event_disconnect);

int hdmi_event_new_edid(struct hdmi_notifier *n, const void *edid, size_t size)
{
	mutex_lock(&n->lock);
	if (n->edid_allocated_size < size) {
		void *p = kmalloc(size, GFP_KERNEL);

		if (p == NULL) {
			mutex_unlock(&n->lock);
			return -ENOMEM;
		}
		kfree(n->edid);
		n->edid = p;
		n->edid_allocated_size = size;
	}
	memcpy(n->edid, edid, size);
	n->edid_size = size;
	blocking_notifier_call_chain(&n->notifiers, HDMI_NEW_EDID, n);
	mutex_unlock(&n->lock);
	return 0;
}
EXPORT_SYMBOL_GPL(hdmi_event_new_edid);

void hdmi_event_new_eld(struct hdmi_notifier *n, const u8 eld[128])
{
	mutex_lock(&n->lock);
	memcpy(n->eld, eld, sizeof(n->eld));
	n->has_eld = true;
	blocking_notifier_call_chain(&n->notifiers, HDMI_NEW_ELD, n);
	mutex_unlock(&n->lock);
}
EXPORT_SYMBOL_GPL(hdmi_event_new_eld);
