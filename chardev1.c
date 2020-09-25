/*Prosty moduł urządzenia znakowego*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h> //file_operation
#include <linux/types.h>//dev_t
#include <asm/uaccess.h>
#include <linux/kdev_t.h>//MKDEV
#include <linux/cdev.h>//cdev

//prototypy funkcji
static int __init chardev_init(void);
static void __exit chardev_exit(void);
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);

#define SUCCESS 0
#define DEVICE_NAME "chardev"
#define BUF_LEN 100

static int Major = 122;
static struct cdev *chcdev;
static char msg[BUF_LEN];
static char *kursor;
static int licznik = 0;

static DEFINE_MUTEX(Device_Open);

//inicjalizacja struktury file_operation
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .release = device_release
};

static int __init chardev_init(void){
    int ret;
    dev_t num;
    num = MKDEV(Major, 0);//tworzy urządzenie znakowe o numerze głównym Major i pobocznym 0
    ret = register_chrdev_region(num,3,DEVICE_NAME);
    if(ret < 0){
        printk(KERN_ALERT "Nieudana proba przydzialu obszaru urzadzenia w jadrze - zwrocony numer %d\n",ret);
    
    return ret;//zwraca zainicjalizowane urządzenie znakowe
    }

    //rejestracja sterownika urządzenia znakowego
    chcdev = cdev_alloc();
    chcdev->owner = THIS_MODULE;
    chcdev->ops = &fops;

    ret = cdev_add(chcdev,num,3);

    if(ret < 0){
        printk(KERN_ALERT "Nieudana proba zarejestowania urzadzenia w jadrze - zwrocony numer %d\n", ret);
        unregister_chrdev_region(num, 3);
        return ret;
    }

    printk(KERN_INFO "Pomyslna inicjalizacja sterownika w jadrze\n");
    return SUCCESS;
}


static void __exit chardev_exit(void){
    dev_t num;
    num = MKDEV(Major, 0);
    
    cdev_del(chcdev);//Funkcja wyrejestrowująca urządzenie znakowe z jądra
    unregister_chrdev_region(num, 3);//Funkcja zwalniająca zarezerwowany obszar urządzeń
    printk(KERN_INFO "Sterownik urzadzenia usuniety\n");
}

module_init(chardev_init);
module_exit(chardev_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("vieczorkamil");
MODULE_DESCRIPTION("Przykladowy modul z plikiem urzadzenia");
MODULE_SUPPORTED_DEVICE(DEVICE_NAME);


static int device_open(struct inode *in, struct file *fp){
    printk(KERN_INFO "Otwarcie pliku urzadzenia o numerze pobocznym %d\n", iminor(in));
    
    if(mutex_lock_interruptible(&Device_Open)){
        printk(KERN_INFO " Proba przejecia semafora przerwana");
        return -ERESTARTSYS;
    }
    
    //zwiększenie licznika użycia modułu co uniemożliwi jego usunięcie z jądra
    try_module_get(THIS_MODULE);
    
    //zapisanie komunikatu, ktory zostanie przepisany do pamięci użytkownika w momencie odczytu
    sprintf(msg, "Hello world! Mowie po raz %d\n", ++licznik);
    kursor = msg;
    
    return SUCCESS;
}


static ssize_t device_read(struct file *fp, char *buforUz, size_t dlugosc, loff_t *offset){
    int odczytano = 0;
    //jeśli osiągnięto koniec bufora \0 koniec łańcucha znakowego
    if(*kursor==0){
        return 0;
    }
    //dopóki długość bufora jest >0 i coś czytamy
    while(dlugosc && *kursor){
        put_user(*(kursor++),buforUz++);//funkcja służąca do przepisania danych z pamięci jądra do pamięci użytkownika
        dlugosc--;
        odczytano++;
    }
    *offset += odczytano;
    return odczytano;//zwracamy liczbę przepisanych danych
}


static ssize_t device_write(struct file *fp, const char *bufor, size_t dlugosc, loff_t *offset){
    //kod póki co trywialny
    printk(KERN_ALERT "To urządzenie nie obsluguje zapisu!\n");
    return -EINVAL;
}


static int device_release(struct inode *in, struct file* fp){
    
    mutex_unlock(&Device_Open);//odblokowanie dostępu
    module_put(THIS_MODULE);//zmniejszenie licznika użycia modułu
    return 0;
}
