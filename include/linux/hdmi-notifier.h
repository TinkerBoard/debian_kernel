#ifndef LINUX_HDMI_NOTIFIER_H
#define LINUX_HDMI_NOTIFIER_H

#include <linux/types.h>
#include <linux/notifier.h>
#include <linux/kref.h>

enum {
	HDMI_CONNECTED,
	HDMI_DISCONNECTED,
	HDMI_NEW_EDID,
	HDMI_NEW_ELD,
};

struct device;

struct hdmi_notifier {
	struct mutex lock;
	struct list_head head;
	struct kref kref;
	struct blocking_notifier_head notifiers;
	struct device *dev;

	/* Current state */
	bool connected;
	bool has_eld;
	unsigned char eld[128];
	void *edid;
	size_t edid_size;
	size_t edid_allocated_size;
};

struct hdmi_notifier *hdmi_notifier_get(struct device *dev);
void hdmi_notifier_put(struct hdmi_notifier *n);
int hdmi_notifier_register(struct hdmi_notifier *n, struct notifier_block *nb);
int hdmi_notifier_unregister(struct hdmi_notifier *n, struct notifier_block *nb);

void hdmi_event_connect(struct hdmi_notifier *n);
void hdmi_event_disconnect(struct hdmi_notifier *n);
int hdmi_event_new_edid(struct hdmi_notifier *n, const void *edid, size_t size);
void hdmi_event_new_eld(struct hdmi_notifier *n, const u8 eld[128]);

#endif
