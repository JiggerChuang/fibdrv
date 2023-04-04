#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>

/* copy_to_user */
#include <linux/uaccess.h>
/* k_time */
#include <linux/ktime.h>
/* string */
#include <linux/string.h>

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

static ktime_t kt;

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 500

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);

static void char_swap(char *a, char *b)
{
    char temp = *a;
    *a = *b;
    *b = temp;
}

static void reverse_string(char *l, char *r)
{
    --r;
    while (l < r) {
        char_swap(l, r);
        ++l;
        --r;
    }
}

static void string_number_add(char *b, char *a, char *res, size_t size)
{
    int carry = 0;

    for (int i = 0; i < size; i++) {
        int temp = (b[i] - '0') + (a[i] - '0') + carry;
        carry = temp / 10;
        temp = temp % 10;
        res[i] = temp + '0';
    }
}

static void string_number_sub(char *b, char *a, char *res, size_t size)
{
    int borrow = 0;

    // printk(KERN_INFO "[string sub] b: %s", b);
    // printk(KERN_INFO "[string sub] a: %s", a);

    for (int i = 0; i < size; i++) {
        int temp = (b[i] - '0') - (a[i] - '0') - borrow;

        // printk(KERN_INFO "[string sub] temp: %d\n", temp);

        if (temp < 0) {
            borrow = 1;
            temp += 10;
        } else {
            borrow = 0;
        }
        res[i] = temp + '0';
    }

    // printk(KERN_INFO "[string sub] res: %s", res);
}

static void string_number_mul(char *b, char *a, char *res, size_t size)
{
    int temp[2 * size];
    for (int i = 0; i < 2 * size; i++)
        temp[i] = 0;

    // printk(KERN_INFO "[string mul] b: %s", b);
    // printk(KERN_INFO "[string mul] a: %s", a);

    for (int i = 0; i < size; i++) {
        if (a[i] - '0' == 0)
            continue;
        for (int j = 0; j < size; j++) {
            temp[i + j] += (a[i] - '0') * (b[j] - '0');

            if (temp[i + j] >= 10) {
                temp[i + j + 1] += temp[i + j] / 10;
                temp[i + j] %= 10;
            }
        }
    }

    for (int i = 0; i < 2 * size; i++)
        res[i] = temp[i] + '0';

    // printk(KERN_INFO "[string mul] res: %s", res);
}

static void calc_even_fib(char *b, char *a, char *res, size_t size)
{
    size_t mul_size = 2 * size;
    char temp_mul[mul_size + 1];
    char temp_sub[mul_size + 1];
    char multiplicand[mul_size + 1];

    memset(temp_mul, '0', mul_size);
    memset(temp_sub, '0', mul_size);
    memset(multiplicand, '0', mul_size);
    temp_mul[mul_size] = '\0';
    temp_sub[mul_size] = '\0';
    multiplicand[mul_size] = '\0';
    multiplicand[0] = '2';

    string_number_mul(b, multiplicand, temp_mul, size); /* 2 * b */
    string_number_sub(temp_mul, a, temp_sub, size);     /* 2 * b - a */
    string_number_mul(temp_sub, a, res, size); /* t1 = a * (2 * b - a) */

    // printk(KERN_INFO "[Calc even fib] res: %s", res);
}

static void calc_odd_fib(char *b, char *a, char *res, size_t size)
{
    size_t mul_size = 2 * size;
    char temp_b[mul_size + 1];
    char temp_a[mul_size + 1];
    memset(temp_b, '0', mul_size);
    memset(temp_a, '0', mul_size);
    temp_b[mul_size] = '\0';
    temp_a[mul_size] = '\0';

    string_number_mul(b, b, temp_b, size);        /* b^2 */
    string_number_mul(a, a, temp_a, size);        /* a^2 */
    string_number_add(temp_b, temp_a, res, size); /* t2 = b^2 + a^2 */
}

static int get_efficient_digit(long long k)
{
    if (k < 0)
        return 0;

    int res = 0;

    while (k != 0) {
        res++;
        k /= 2;
    }

    return res;
}

static long long fib_sequence(long long k, char *buf, size_t size)
{
    kt = ktime_get();

    /* FIXME: C99 variable-length array (VLA) is not allowed in Linux kernel. */
    size_t mul_size = 2 * size;
    char a[mul_size + 1];
    char b[mul_size + 1];
    char res[mul_size + 1];
    char t1[mul_size + 1];
    char t2[mul_size + 1];
    long long digits = mul_size;

    memset(a, '0', mul_size);
    memset(b, '0', mul_size);
    memset(t1, '0', mul_size);
    memset(t2, '0', mul_size);
    memset(res, '0', mul_size);
    a[mul_size] = '\0';
    b[mul_size] = '\0';
    t1[mul_size] = '\0';
    t2[mul_size] = '\0';
    res[mul_size] = '\0';

    a[0] = '0'; /* a for f[i - 2] */
    b[0] = '1'; /* b for f[i - 1] */

    if (k == 0)
        strncpy(res, a, 1);
    if (k == 1)
        strncpy(res, b, 1);

    /* Baseline Fib calculating
    for (int i = 2; i <= k; i++) {
        string_number_add(b, a, res, size);

        strncpy(a, b, size);
        strncpy(b, res, size);
    }*/
    /* End Baseline Fib calculating */

    /* Fast-doubling : Iterative */
    int efficient_digit = get_efficient_digit(k);
    for (; efficient_digit > 0; efficient_digit--) {
        calc_even_fib(b, a, t1, size);
        calc_odd_fib(b, a, t2, size);

        strncpy(a, t1, mul_size);
        strncpy(b, t2, mul_size);

        if (((k >> (efficient_digit - 1)) & 1) == 1) { /* n is odd */
            string_number_add(a, b, t1, mul_size);
            strncpy(a, b, mul_size);
            strncpy(b, t1, mul_size);
        }
    }
    strncpy(res, a, mul_size);
    /* End Fast-doubling : Iterative */

    /* get digits of res */
    for (int i = mul_size - 1; i > 0; i--) {
        if (res[i] != '0')
            break;
        digits--;
    }

    char out[digits + 1];
    strncpy(out, res, digits);
    out[digits] = '\0';
    reverse_string(&out[0], &out[digits]);
    copy_to_user(buf, out, digits);

    kt = ktime_sub(ktime_get(), kt);

    return digits;
}

static int fib_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&fib_mutex)) {
        printk(KERN_ALERT "fibdrv is in use");
        return -EBUSY;
    }
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&fib_mutex);
    return 0;
}

/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    return (ssize_t) fib_sequence(*offset, buf, size);
}

/* write operation is skipped */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    return ktime_to_ns(kt);
}

static loff_t fib_device_lseek(struct file *file, loff_t offset, int orig)
{
    loff_t new_pos = 0;
    switch (orig) {
    case 0: /* SEEK_SET: */
        new_pos = offset;
        break;
    case 1: /* SEEK_CUR: */
        new_pos = file->f_pos + offset;
        break;
    case 2: /* SEEK_END: */
        new_pos = MAX_LENGTH - offset;
        break;
    }

    if (new_pos > MAX_LENGTH)
        new_pos = MAX_LENGTH;  // max case
    if (new_pos < 0)
        new_pos = 0;        // min case
    file->f_pos = new_pos;  // This is what we'll use now
    return new_pos;
}

const struct file_operations fib_fops = {
    .owner = THIS_MODULE,
    .read = fib_read,
    .write = fib_write,
    .open = fib_open,
    .release = fib_release,
    .llseek = fib_device_lseek,
};

static int __init init_fib_dev(void)
{
    int rc = 0;

    mutex_init(&fib_mutex);

    // Let's register the device
    // This will dynamically allocate the major number
    rc = alloc_chrdev_region(&fib_dev, 0, 1, DEV_FIBONACCI_NAME);

    if (rc < 0) {
        printk(KERN_ALERT
               "Failed to register the fibonacci char device. rc = %i",
               rc);
        return rc;
    }

    fib_cdev = cdev_alloc();
    if (fib_cdev == NULL) {
        printk(KERN_ALERT "Failed to alloc cdev");
        rc = -1;
        goto failed_cdev;
    }
    fib_cdev->ops = &fib_fops;
    rc = cdev_add(fib_cdev, fib_dev, 1);

    if (rc < 0) {
        printk(KERN_ALERT "Failed to add cdev");
        rc = -2;
        goto failed_cdev;
    }

    fib_class = class_create(THIS_MODULE, DEV_FIBONACCI_NAME);

    if (!fib_class) {
        printk(KERN_ALERT "Failed to create device class");
        rc = -3;
        goto failed_class_create;
    }

    if (!device_create(fib_class, NULL, fib_dev, NULL, DEV_FIBONACCI_NAME)) {
        printk(KERN_ALERT "Failed to create device");
        rc = -4;
        goto failed_device_create;
    }
    return rc;
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
    cdev_del(fib_cdev);
failed_cdev:
    unregister_chrdev_region(fib_dev, 1);
    return rc;
}

static void __exit exit_fib_dev(void)
{
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    cdev_del(fib_cdev);
    unregister_chrdev_region(fib_dev, 1);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);
