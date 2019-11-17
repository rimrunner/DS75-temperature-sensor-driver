#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/uaccess.h> //?
#include <linux/gpio/consumer.h> //?

//maskit I2C-devicen komennoille
#define CMD_READ   0

/* This structure will represent single device */
//Tämä struct esittää laitekohtaista I/O -informaatiota
struct DS75_dev {
  struct i2c_client *client;
  struct miscdevice DS75_miscdevice;  //Tämä misc subsystem automaattisesti käsittelee open() -funktion. Automaattisesti luodussa open() -funktiossa se sitoo luomasi struct miscdevicen yksityiseen struct DS75_deviin avatulle tiedostolle. Näin voi read/write kernel callback -funktioissa recoverata miscdevice structin, jolla pääsee struct i2c_clientiin, joka includataan struct DS75_deviin. Kun saa struct i2c_clientin, voi kirjoittaa/lukea jokaiseen I2C-spesifiin laitteeseen SMBus-funktioilla
  struct gpio_desc *display_cs; //TULEEKO TÄTÄ? TÄMÄ ON LED-AJURISTA
  char name[8]; //I2C I/O devicen nimeä kantava struct, tähän 4?
  u8 command[3]; //Mikä tuo 3 on? Viittaako tämä #definellä määriteltyihin komentoihin?
};

/* User is reading data from /dev/DS75XX */
//kernelin callback-funktio, joka lukee laitteen inputin ja lähettää arvot käyttäjätilaan
static ssize_t DS75_read_file(struct file *file, char __user *userbuf, size_t count, loff_t *ppos) {

  int expval, size;
  char buf[3];
  struct DS75_dev *DS75;

  DS75 = container_of(file->private_data, struct DS75_dev, DS75_miscdevice);

  /* read IO expander input to expval */
  expval = i2c_smbus_read_byte(DS75->client);
  if (expval < 0) return -EFAULT;

  /* 
   * converts expval in 2 characters (2bytes) + null value (1byte)
   * The values converted are char values (FF) that match with the hex
   * int(s32) value of the expval variable.
   * if we want to get the int value again, we have to
   * do Kstrtoul(). We convert 1 byte int value to
   * 2 bytes char values. For instance 255 (1 int byte) = FF (2 char bytes).
   */
  size = sprintf(buf, "%02x", expval);

  /* 
   * replace NULL by \n. It is not needed to have the char array
   * ended with \0 character.
   */
  buf[size] = '\n';

  /* send size+1 to include the \n character */
  if(*ppos == 0){
    if(copy_to_user(userbuf, buf, size+1)){
      pr_info("Failed to return led_value to user space\n");
      return -EFAULT;
    }
    *ppos+=1;
    return size+1;
  }
  return 0;
}

//TARVITAANKO TÄTÄ STRUCTIA?? TÄSSÄHÄN EI KIRJOITELLA KÄYTTÄJÄTILASTA
/* Writing from the terminal command line, \n is added */
//Tätä kutsutaan aina, kun käyttäjätilan kirjoitusoperaatio tapahtuu jollekin chardeviceistä
//Misc devicejä rekisteröidessä ei laitettu mitään pointteria laitteen private structiin
//Mutta struct miscdeviceen pääsee file->private_datan kautta ja se on sen private structin jäsen,
//joten voi käyttää container_of() -makroa laskemaan private structin osoite ja saada struct i2c_client siitä
/*
copy_from_user() -funktio hakee char arrayn käyttäjätilasta arvoilla 0-255 ja tämä arvo konvertoidaan unsigned longiksi
ja sen kirjoitat private structiin käytämällä i2c_smbus_write_byte() -SMBus-funktiota. 
 */
static ssize_t DS75_write_file(struct file *file, const char __user *userbuf, size_t count, loff_t *ppos) {

  int ret;
  unsigned long val;
  char buf[4];
  struct DS75_dev * DS75;

  DS75 = container_of(file->private_data,
		       struct DS75_dev, 
		       DS75_miscdevice);

  dev_info(&DS75->client->dev, 
	   "DS75_write_file entered on %s\n", DS75->name);

  dev_info(&DS75->client->dev,
	   "we have written %zu characters\n", count); 

  if(copy_from_user(buf, userbuf, count)) {
    dev_err(&DS75->client->dev, "Bad copied value\n");
    return -EFAULT;
  }

  buf[count-1] = '\0';

  /* convert the string to an unsigned long */
  ret = kstrtoul(buf, 0, &val);
  if (ret)
    return -EINVAL;

  dev_info(&DS75->client->dev, "the value is %lu\n", val);

  ret = i2c_smbus_write_byte(DS75->client, val);
  if (ret < 0)
    dev_err(&DS75->client->dev, "the device is not found\n");

  dev_info(&DS75->client->dev, 
	   "DS75_write_file exited on %s\n", DS75->name);

  return count;
}

//Tässä määritellään, mitä driverin funktioita kutsutaan, kun käyttäjä lukee/kirjoittaa charlaitteisiin (ilmeisesti tähän tulee muutkin komennot?)
//Tämä struct viedään misc subsystemiin, kun siihen rekisteröidään laite
static const struct file_operations DS75_fops = {
	.owner = THIS_MODULE,
	.read = DS75_read_file,
	.write = DS75_write_file,
};

//Tässä laitteen private structure (DS75_dev) allokoidaan devm_kzalloc() -funktiolla
//Jokainen misc device initialisoidaan ja rekisteröidään kerneliin misc_register() -funktiolla
//i2c_set_clientdata() -funktio kiinnittää kunkin allokoidun privaatti-structuren i2c_client -struktiin,
//jonka kautta pääsee siihen privaattidatastruktiin muista osista ajuria (esim. remove():lla ja i2c_get_cliendata():lla)
static int DS75_probe(struct i2c_client *client, const struct i2c_device_id *id) {
  
	static int counter = 0;

	struct DS75_dev *DS75;

	/* Allocate new structure representing device */
	DS75 = devm_kzalloc(&client->dev, sizeof(struct DS75_dev), GFP_KERNEL);

	/* Store pointer to the device-structure in bus device context */
	i2c_set_clientdata(client, DS75);

	/* Store pointer to I2C device/client */
	DS75->client = client;

	/* Initialize the misc device, DS75 incremented after each probe call */
	sprintf(DS75->name, "DS75%02d", counter++); 
	dev_info(&client->dev, "DS75_probe is entered on %s\n", DS75->name);

	DS75->DS75_miscdevice.name = DS75->name;
	DS75->DS75_miscdevice.minor = MISC_DYNAMIC_MINOR;
	DS75->DS75_miscdevice.fops = &DS75_fops;

	/* Register misc device */
	return misc_register(&DS75->DS75_miscdevice);

	dev_info(&client->dev, 
		 "DS75_probe is exited on %s\n", DS75->name);

	return 0;
}

static int DS75_remove(struct i2c_client *client) {
  struct DS75_dev *DS75;

  /* Get device structure from bus device context */	
  DS75 = i2c_get_clientdata(client);

  dev_info(&client->dev, 
	   "DS75_remove is entered on %s\n", DS75->name);

  /* Deregister misc device */
  misc_deregister(&DS75->DS75_miscdevice);

  dev_info(&client->dev, 
	   "DS75_remove is exited on %s\n", DS75->name);

  return 0;
}

static const struct of_device_id DS75_dt_ids[] = {
	{ .compatible = "arrow,DS75", },
	{ }
};
MODULE_DEVICE_TABLE(of, DS75_dt_ids);

static const struct i2c_device_id i2c_ids[] = {
	{ .name = "DS75", },
	{ }
};
MODULE_DEVICE_TABLE(i2c, i2c_ids);

//I2C-subsystemien määrittelemä structi, jonka jokainen i2c-device driver rekisteröi i2c bus coreen. Siinä viitataan proveen ja removeen, jotka on funktioita tässä tiedostossa
static struct i2c_driver DS75_driver = {
	.driver = {
		.name = "DS75",
		.owner = THIS_MODULE,
		.of_match_table = DS75_dt_ids,
	},
	.probe = DS75_probe,
	.remove = DS75_remove,
	.id_table = i2c_ids,
};

module_i2c_driver(DS75_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("JS");
MODULE_DESCRIPTION("DS75");


