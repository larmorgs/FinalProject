#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/spi/spi.h>

#define DRIVER_NAME "spi-lpd8806"

#define STRAND_LEN 160 // Length of data memory

#define SPI_BUS_NUM 2
#define SPI_BUS_SPEED 500000
#define SPI_CS 0

static struct spi_device *device;

struct lpd8806_obj {
  struct kobject kobj;
  unsigned char rgb[3];
  unsigned char data[STRAND_LEN][3];
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
    int count;
    for (i = 0; i < STRAND_LEN; i++) {
      count = sprintf(buf, "%s%d [%hhu %hhu %hhu]\n", buf, i, obj->data[i][0], obj->data[i][1], obj->data[i][2]);  
    }
    return count;
  } else {
    return 0;
  }
}

static ssize_t lpd8806_store(struct lpd8806_obj *obj, struct lpd8806_attr *attr, const char *buf, size_t count) {
  if (strcmp(attr->attr.name, "rgb") == 0) {
    int i;
    sscanf(buf, "%hhu %hhu %hhu", &obj->rgb[0], &obj->rgb[1], &obj->rgb[2]);
    for (i = STRAND_LEN - 1; i > 0; i--) {
      obj->data[i][0] = obj->data[i-1][0];
      obj->data[i][1] = obj->data[i-1][1];
      obj->data[i][2] = obj->data[i-1][2];
    }
    obj->data[0][0] = obj->rgb[0];
    obj->data[0][1] = obj->rgb[1];
    obj->data[0][2] = obj->rgb[2];
    return count;
  } else if (strcmp(attr->attr.name, "data") == 0) {
    int i = 0;
    char *temp;
    char *tok;
    temp = kzalloc(strlen(buf), GFP_KERNEL);
    if (temp) {
      strcpy(temp, buf);
      tok = strsep(&temp, " ");      
      while (tok) {
        if (sscanf(tok, "%hhu", &obj->data[0][0]+i) == 1) {
          i++;
          if (i == 3*STRAND_LEN) {
            break;
          }
        }
        tok = strsep(&temp, " ");
      }
      obj->rgb[0] = obj->data[0][0];
      obj->rgb[1] = obj->data[0][1];
      obj->rgb[2] = obj->data[0][2];
      
      kfree(temp);
      return count;
    }
  }
  return 0;
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
static struct lpd8806_obj *device_obj;

static struct lpd8806_obj *create_lpd8806_obj(const char *name) {
  struct lpd8806_obj *obj;
  int retval;
  int i, j;
  
  obj = kzalloc(sizeof(*obj), GFP_KERNEL);
  if (!obj) {
    return NULL;
  }
    
  obj->kobj.kset = lpd8806_kset;

  for (i = 0; i < 3; i++) {
    obj->rgb[i] = 0;
  }
  
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

static void destroy_lpd8806_obj(struct lpd8806_obj *obj) {
  kobject_put(&obj->kobj);
}

static int __init lpd8806_init(void) {
  int status;
  struct spi_master *master;
  struct device *temp;
  char buff[20];
  
  lpd8806_kset = kset_create_and_add(DRIVER_NAME, NULL, firmware_kobj);
  if (!lpd8806_kset) {
    kset_unregister(lpd8806_kset);
    printk("LPD8806 Driver Init Failed: %d\n", -ENOMEM);
    return -ENOMEM;
  }
  device_obj = create_lpd8806_obj("device");
  if (!device_obj) {
    destroy_lpd8806_obj(device_obj);
    kset_unregister(lpd8806_kset);
    printk("LPD8806 Driver Init Failed: %d\n", -EINVAL);
    return -EINVAL;
  }
  
  master = spi_busnum_to_master(SPI_BUS_NUM);
  if (!master) {
    destroy_lpd8806_obj(device_obj);
    kset_unregister(lpd8806_kset);
    printk("LPD8806 Driver Init Failed: Bad Master\n");
    return -EINVAL;
  }
  
  device = spi_alloc_device(master);
  if (!device) {
    put_device(&master->dev);
    destroy_lpd8806_obj(device_obj);
    kset_unregister(lpd8806_kset);
    printk("LPD8806 Driver Init Failed: Bad Device\n");
    return -EINVAL;
  }
  
  device->chip_select = SPI_CS;
  snprintf(buff, sizeof(buff), "%s.%u", dev_name(&device->master->dev), device->chip_select);
  
  temp = bus_find_device_by_name(device->dev.bus, NULL, buff);
  if (temp) {
    spi_unregister_device(to_spi_device(temp));
  }
  
  device->max_speed_hz = SPI_BUS_SPEED;
  device->mode = SPI_MODE_0;
  device->bits_per_word = 8;
  device->irq = -1;
  device->controller_state = NULL;
  device->controller_data = NULL;
  strcpy(device->modalias, DRIVER_NAME);
  
  status = spi_add_device(device);
  
  if (status < 0) {
    spi_dev_put(device);
    destroy_lpd8806_obj(device_obj);
    kset_unregister(lpd8806_kset);
    printk("LPD8806 Driver Init Failed: Add Device Failed\n");
    return status;
  }
  
  printk("LPD8806 Driver Added\n");
  return 0;
}

static void __exit lpd8806_exit(void) {
  spi_unregister_device(device);
  destroy_lpd8806_obj(device_obj);
  kset_unregister(lpd8806_kset);
  printk("LPD8806 Driver Removed\n");
}

module_init(lpd8806_init);
module_exit(lpd8806_exit);

MODULE_DESCRIPTION("LPD8806 DRIVER");
MODULE_AUTHOR("GREG LARMORE & SEAN RICHARDSON");
MODULE_LICENSE("GPL");
