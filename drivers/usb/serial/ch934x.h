/*
 *
 * Includes for ch934x.c
 *
 */

/*
 * Baud rate and default timeout
 */
#define DEFAULT_BAUD_RATE 9600
#define DEFAULT_TIMEOUT   1000

/*
 * CMSPAR, some architectures can't have space and mark parity.
 */

#ifndef CMSPAR
#define CMSPAR			0
#endif

/*
 * Major and minor numbers.
 */

#define CH934X_TTY_MAJOR		168
#define CH934X_TTY_MINORS		256

/*
 * Output control lines.
 */

#define CH934X_CTRL_DTR		0x01
#define CH934X_CTRL_RTS		0x02

/*
 * Input control lines and line errors.
 */

#define CH934X_CTRL_DCD		0x01
#define CH934X_CTRL_DSR		0x02
#define CH934X_CTRL_BRK		0x04
#define CH934X_CTRL_RI		0x08

#define CH934X_CTRL_FRAMING	0x10
#define CH934X_CTRL_PARITY		0x20
#define CH934X_CTRL_OVERRUN	0x40

//Vendor define
#define VENDOR_WRITE_TYPE		0xC0
#define VENDOR_INIT_TYPE		0x80
#define VENDOR_SET_TYPE		    0x20

/*
 * Internal driver structures.
 */

/*
 * The only reason to have several buffers is to accommodate assumptions
 * in line disciplines. They ask for empty space amount, receive our URB size,
 * and proceed to issue several 1-character writes, assuming they will fit.
 * The very first write takes a complete URB. Fortunately, this only happens
 * when processing onlcr, so we only need 2 buffers. These values must be
 * powers of 2.
 */
#define CH934X_NW  16
#define CH934X_NR  16

#define MAXDEV 4
#define MAXPORT 4
#define NUMSTEP 4

struct ch934x_wb {
	unsigned char *buf;
	dma_addr_t dmah;
	int len;
	int use;
	struct urb		*urb;
	struct ch934x		*instance;
};

struct ch934x_rb {
	int			size;
	unsigned char		*base;
	dma_addr_t		dma;
	int			index;
	struct ch934x		*instance;
};

struct ch934x_ttyport {
	struct tty_port port;
	int portnum;
	void *portdata;
};

struct usb_ch934x_line_coding {
	__le32	dwDTERate;
	__u8	bCharFormat;
#define USB_CH934X_1_STOP_BITS			0
#define USB_CH934X_1_5_STOP_BITS			1
#define USB_CH934X_2_STOP_BITS			2

	__u8	bParityType;
#define USB_CH934X_NO_PARITY			0
#define USB_CH934X_ODD_PARITY			1
#define USB_CH934X_EVEN_PARITY			2
#define USB_CH934X_MARK_PARITY			3
#define USB_CH934X_SPACE_PARITY			4

	__u8	bDataBits;
} __attribute__ ((packed));

struct ch934x {
	struct usb_device *dev;				/* the corresponding usb device */
	struct usb_interface *control;			/* control interface */
	struct usb_interface *data;			/* data interface */
	unsigned int num_ports;
	struct ch934x_ttyport ttyport[MAXPORT];			 	/* our tty port data */
	
	struct urb *cmdreadurb;				/* urbs */
	u8 *cmdread_buffer;				/* buffers of urbs */
	dma_addr_t cmdread_dma;				/* dma handles of buffers */
	
	struct ch934x_wb wb[CH934X_NW];
	unsigned long read_urbs_free;
	struct urb *read_urbs[CH934X_NR];
	struct ch934x_rb read_buffers[CH934X_NR];
	int rx_buflimit;
	int rx_endpoint;
    int tx_endpoint;
    int cmdtx_endpoint;
    int cmdrx_endpoint;
    int ctrl_endpoint;
	
	spinlock_t read_lock;
	int write_used;					/* number of non-empty write buffers */
	int transmitting;
	bool write_empty[MAXPORT];
	spinlock_t write_lock;
	struct mutex mutex;
	bool disconnected;
	struct usb_ch934x_line_coding line[CH934X_NR];		/* bits, stop, parity */
	struct work_struct work;			/* work queue entry for line discipline waking up */
	unsigned int ctrlin;				/* input control lines (DCD, DSR, RI, break, overruns) */
	unsigned int ctrlout;				/* output control lines (DTR, RTS) */
	struct async_icount iocount;			/* counters for control line changes */
	struct async_icount oldcount;			/* for comparison of counter */
	wait_queue_head_t wioctl;			/* for ioctl */
	unsigned int writesize;				/* max packet size for the output bulk endpoint */
	unsigned int readsize, cmdsize;			/* buffer sizes for freeing */
	unsigned int ctrlsize;
	unsigned int minor;				    /* ch934x minor number */
	unsigned char clocal;				/* termios CLOCAL */
	unsigned int susp_count;			/* number of suspended interfaces */
	unsigned int throttled:1;			/* actually throttled */
	unsigned int throttle_req:1;			/* throttle requested */
	u8 bInterval;
	struct usb_anchor delayed;			/* writes queued for a device about to be woken */
	unsigned long quirks;
};


/* constants describing various quirks and errors */
#define SINGLE_RX_URB			BIT(1)
#define NO_DATA_INTERFACE		BIT(4)
#define IGNORE_DEVICE			BIT(5)
#define QUIRK_CONTROL_LINE_STATE	BIT(6)
#define CLEAR_HALT_CONDITIONS		BIT(7)


