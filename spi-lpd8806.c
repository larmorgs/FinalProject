#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>

#define STRAND_LEN 160 // Length of data memory
#define DEVICE "/dev/spidev2.0"
#define BYTESIZE 8
#define SPEED 5000000

struct lpd8806_obj {
  struct kobject kobj;
  unsigned char *rgb;
  unsigned char **data;
};
#define to_lpd8806_obj(x) container_of(x, struct lpd8806_obj, kobj)

struct lpd8806_attr {
  struct attribute attr;
  ssize_t (*show)(struct lpd8806_obj *obj, struct lpd8806_attr *attr, char *buf);
  ssize_t (*store)(struct lpd8806_obj *obj, struct lpd8806_attr *attr, const char *buf, size_t count);
};
#define to_lpd8806_attr(x) container_of(x, struct lpd8806_attr, attr)

static ssize_t lpd8806_attr_show(struct kobject *kobj, struct attribute *attr, char *buf) {
  struct lpd8806_attr *attribute;
  struct lpd8806_obj *obj;
  
  attribute = to_lpd8806_attr(attr);
  obj = to_lpd8806_obj(kobj);
  
  if (!attribute->show) {
    return -EIO;
  }
  return attribute->show(obj, attribute, buf);
}

static ssize_t lpd8806_attr_store(struct kobject *kobj, struct attribute *attr, const char *buf, size_t len) {
  struct lpd8806_attr *attribute;
  struct lpd8806_obj *obj;
  
  attribute = to_lpd8806_attr(attr);
  obj = to_lpd8806_obj(kobj);
  
  if (!attribute->store) {
    return -EIO;
  }
  return attribute->store(obj, attribute, buf, len);
}

static struct sysfs_ops lpd8806_sysfs_ops = {
  .show = lpd8806_attr_show,
  .store = lpd8806_attr_store
};

static void lpd8806_release(struct kobject *kobj) {
  struct lpd8806_obj *obj;
  obj = to_lpd8806_obj(kobj);
  kfree(obj);
}

static ssize_t lpd8806_show(struct lpd8806_obj *obj, struct lpd8806_attr *attr, char *buf) {
  if (strcmp(attr->attr.name, "rgb") == 0) {
    return sprintf(buf, "[%hhu %hhu %hhu]\n", obj->rgb[0], obj->rgb[1], obj->rgb[2]);
  } else if (strcmp(attr->attr.name, "data") == 0) {
    int i;
    int count = 0;
    for (i = 0; i < STRAND_LEN; i++) {
      count += sprintf(buf, "%s[%hhu %hhu %hhu]\n", buf, obj->data[i][0], obj->data[i][1], obj->data[i][2]);  
    }
    return count;
  } else {
    return 0;
  }
}

static ssize_t lpd8806_store(struct lpd8806_obj *obj, struct lpd8806_attr *attr, const char *buf, size_t count) {
  if (strcmp(attr->attr.name, "rgb") == 0) {
    sscanf(buf, "%hhu %hhu %hhu", &obj->rgb[0], &obj->rgb[1], &obj->rgb[2]);
    transfer(&obj->rgb[0], 3);
    return count;
  } else if (strcmp(attr->attr.name, "data") == 0) {
    // Add in recursive data setting for entire strand
    return count;
  } else {
    return 0;
  }
}

static struct lpd8806_attr rgb_attr = __ATTR(rgb, 0666, lpd8806_show, lpd8806_store);
static struct lpd8806_attr data_attr = __ATTR(data, 0666, lpd8806_show, lpd8806_store);

static struct attribute *lpd8806_default_attrs[] = {
  &rgb_attr.attr,
  &data_attr.attr,
  NULL
};

static struct kobj_type lpd8806_ktype = {
  .sysfs_ops = &lpd8806_sysfs_ops,
  .release = lpd8806_release,
  .default_attrs = lpd8806_default_attrs
};

static struct kset *lpd8806_kset;
static struct lpd8806_obj *rgb_obj;

static struct lpd8806_obj *create_lpd8806_obj(const char *name) {
  struct lpd8806_obj *obj;
  int retval;
  
  obj = kzalloc(sizeof(*obj), GFP_KERNEL);
  if (!obj) {
    return NULL;
  }
    
  obj->kobj.kset = lpd8806_kset;

  obj->rgb = kmalloc(3, GFP_KERNEL);
  if (!obj->rgb) {
    return NULL;
  }
  memset(obj->rgb, 0, 3);
  
  int i;
  int j;
  for (i = 0; i < STRAND_LEN; i++) {
    for (j = 0; j < 3; j++) {
      obj->data[i][j] = 0;
    }
  }
  
  retval = kobject_init_and_add(&obj->kobj, &lpd8806_ktype, NULL, "%s", name);
  if (retval) {
    kobject_put(&obj->kobj);
    return NULL;
  }
  
  kobject_uevent(&obj->kobj, KOBJ_ADD);
  
  return obj;
}

static void transfer(char *tx, int size)
{
    int ret;
    char rx[size] = {0, };
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = size,
        .delay_usecs = 0,
        .speed_hz = SPEED,
        .bits_per_word = BYTESIZE,
    };

    ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
    if (ret < 1)
        pabort("can't send spi message");

    for (ret = 0; ret < size; ret++) {
        if (!(ret % 6))
            puts("");
        printf("%.2X ", rx[ret]);
    }
    puts("");
}

static void destroy_lpd8806_obj(struct lpd8806_obj *obj) {
  kfree(&obj->rgb);
  kobject_put(&obj->kobj);
}

static int __init lpd8806_init(void) {
  lpd8806_kset = kset_create_and_add("lpd8806", NULL, firmware_kobj);
  int ret = 0;
  int fd;
  int mode;

  fd = open(DEVICE, O_RDWR);
  if (fd < 0)
    pabort("can't open device");

    /*
     * spi mode
     */
  ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
  if (ret == -1)
      pabort("can't set spi mode");

  ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);
  if (ret == -1)
      pabort("can't get spi mode");

  /*
   * bits per word
   */
  ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
  if (ret == -1)
      pabort("can't set bits per word");

  ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
  if (ret == -1)
      pabort("can't get bits per word");

  /*
   * max speed hz
   */
  ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
  if (ret == -1)
      pabort("can't set max speed hz");

  ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
  if (ret == -1)
      pabort("can't get max speed hz");

  printf("spi mode: %d\n", mode);
  printf("bits per word: %d\n", bits);
  printf("max speed: %d Hz (%d KHz)\n", speed, speed/1000);

  if (!lpd8806_kset) {
    kset_unregister(lpd8806_kset);
    printk("LPD8806 Driver Init Failed: %d\n", -ENOMEM);
    return -ENOMEM;
  }
  rgb_obj = create_lpd8806_obj("device");
  if (!rgb_obj) {
    destroy_lpd8806_obj(rgb_obj);
    kset_unregister(lpd8806_kset);
    printk("LPD8806 Driver Init Failed: %d\n", -EINVAL);
    return -EINVAL;
  }
  printk("LPD8806 Driver Added\n");
  return 0;
}

static void __exit lpd8806_exit(void) {
  close(fd);
  destroy_lpd8806_obj(rgb_obj);
  kset_unregister(lpd8806_kset);
  printk("LPD8806 Driver Removed\n");
}

module_init(lpd8806_init);
module_exit(lpd8806_exit);

MODULE_DESCRIPTION("LPD8806 DRIVER");
MODULE_AUTHOR("GREG LARMORE & SEAN RICHARDSON");
MODULE_LICENSE("GPL");
