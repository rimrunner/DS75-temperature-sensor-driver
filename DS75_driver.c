#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/of.h>
#include <linux/uaccess.h>

struct DS75_dev {
  struct i2c_client *client;
  struct miscdevice DS75_miscdevice;
  char name[8];
  u8 address;
  u8 conf_reg;
  u8 reg_pointer;
};


static u8 change_address(u8 input) {
  u8 adr;
  adr = 0;

  //fixed part
  adr |= 1 << 3;
  adr |= 1 << 6;

  //Setting address bits
  if(input == 1) {
    adr |= 1 << 0;
  }
  else if(input == 2) {
    adr |= 1 << 1;
  }
  else if(input == 3) {
    adr |= 1 << 0;
    adr |= 1 << 1;
  }
  else if(input == 4) {
    adr |= 1 << 2;
  }
  else if(input == 5) {
    adr |= 1 << 0;
    adr |= 1 << 2;
  }
  else if(input == 6) {
    adr |= 1 << 1;
    adr |= 1 << 2;
  }
  else if(input == 7) {
    adr |= 1 << 0;
    adr |= 1 << 1;
    adr |= 1 << 2;
  }
  return adr;
}

static ssize_t DS75_read_file(struct file *file, char __user *userbuf, size_t count, loff_t *ppos) {

  int bytes_recvd;
  char read_buf[2];
  struct DS75_dev *DS75;
  u8 command = 0;
  DS75 = container_of(file->private_data, struct DS75_dev, DS75_miscdevice);

  command = change_address(DS75->address);
  command |= 1 << 7;

  DS75->client->addr = command;
  bytes_recvd = i2c_master_recv(DS75->client, read_buf, 2);

  if(bytes_recvd < 0) return -EFAULT;

  if(*ppos == 0) {
    if(copy_to_user(userbuf, read_buf, 2)) { 
      pr_info("Failed to return value to user space\n");
      return -EFAULT;
    }
    *ppos += 1;
    return bytes_recvd;
  }
  return 0;
}


/* Writing from the terminal command line, \n is added */
static ssize_t DS75_write_file(struct file *file, const char __user *userbuf, size_t count, loff_t *ppos) {

  int smbret;
  char buf[4];
  char transfer_buf[2];
  u8 transfer_bytes;
  int comfound; //1 = user sent command is found, 2 = command to read configuration register is found
  struct DS75_dev *DS75;
  u8 p_byte;
  u8 a_byte;
  comfound = 0;
  p_byte = 0;
  transfer_bytes = 2;
  transfer_buf[0] = '\0';
  transfer_buf[1] = '\0';

  DS75 = container_of(file->private_data, struct DS75_dev, DS75_miscdevice);

  a_byte = change_address(DS75->address);

  dev_info(&DS75->client->dev, "DS75_write_file entered on %s\n", DS75->name);
  dev_info(&DS75->client->dev, "we have written %zu characters\n", count); 

  if(copy_from_user(buf, userbuf, count)) { //Parameters: dest, source, size
    dev_err(&DS75->client->dev, "Bad copied value\n");
    return -EFAULT;
  }

  if(buf[0] == 'p') {
    if(buf[1] == '0') {
      p_byte &= ~(1 << 0);
      p_byte &= ~(1 << 1);
      comfound = 1;
      DS75->reg_pointer = 0;
    }
    else if(buf[1] == '1') {
      p_byte |= 1 << 0;
      p_byte &= ~(1 << 1);
      comfound = 1;
      DS75->reg_pointer = 1;
    }
    else if(buf[1] == '2') {
      p_byte |= 1 << 1;
      p_byte &= ~(1 << 0);
      comfound = 1;
      DS75->reg_pointer = 2;
    }
    else if(buf[1] == '3') {
      p_byte |= 1 << 0;
      p_byte |= 1 << 1;
      comfound = 1;
      DS75->reg_pointer = 3;
    }

    if(comfound) {
      a_byte |= 1 << 7; //R/W -bit
      DS75->client->addr = a_byte;
      smbret = i2c_smbus_read_i2c_block_data(DS75->client, p_byte, 1, transfer_buf);
    }
  }

  //Note: conf_reg is not to be reseted at any point after starting the device
  else if(buf[0] == 'r') { //Switching temperature resolution...
    if(buf[1] == '0') { //to 9-bit
      DS75->conf_reg &= ~(1 << 6);
      DS75->conf_reg &= ~(1 << 5);
      comfound = 2;
    }
    else if(buf[1] == '1') { //to 10-bit
      DS75->conf_reg &= ~(1 << 6);
      DS75->conf_reg |= 1 << 5;
      comfound = 2;
    }
    else if(buf[1] == '2') { //to 11-bit
      DS75->conf_reg |= 1 << 6;
      DS75->conf_reg &= ~(1 << 5);
      comfound = 2;
    }
    else if(buf[1] == '3') { //to 12-bit
      DS75->conf_reg |= 1 << 5;
      DS75->conf_reg |= 1 << 6;
      comfound = 2;
    }
  }

  else if(buf[0] == 'f') { //Switching fault tolerance...
    if(buf[1] == '1') { //to 1
      DS75->conf_reg &= ~(1 << 4);
      DS75->conf_reg &= ~(1 << 3);
      comfound = 2;
    }
    else if(buf[1] == '2') { //to 2
      DS75->conf_reg &= ~(1 << 4);
      DS75->conf_reg |= 1 << 3;
      comfound = 2;
    }
    else if(buf[1] == '4') { //to 4
      DS75->conf_reg |= 1 << 4;
      DS75->conf_reg &= ~(1 << 3);
      comfound = 2;
    }
    else if(buf[1] == '6') { //to 6
      DS75->conf_reg |= 1 << 4;
      DS75->conf_reg |= 1 << 3;
      comfound = 2;
    }
  }

  else if(buf[0] == 'o') { //Switching thermostat output (O. S.) polarity
    if(buf[1] == '0') { //to active low
      DS75->conf_reg &= ~(1 << 2);
      comfound = 2;
    }
    else if(buf[1] == '1') { //to active high
      DS75->conf_reg |= 1 << 2;
      comfound = 2;
    }
  }

  //Switching thermostat operating mode
  else if(buf[0] == 'm') {
    if(buf[1] == '0') { //to comparator mode
      DS75->conf_reg &= ~(1 << 1);
      comfound = 2;
    }
    else if(buf[1] == '1') { //to interrupt mode
      DS75->conf_reg |= 1 << 1;
      comfound = 2;
    }
  }

  //Shutdown mode
  else if(buf[0] == 's') {
    if(buf[1] == '0') { //Active conversion and thermostat operation
      DS75->conf_reg &= ~(1 << 0);
      comfound = 2;
    }
    else if(buf[1] == '1') { //Shutdown mode
      DS75->conf_reg |= 1 << 0;
      comfound = 2;
    }
  }

  //If comfound == 2 at this point, configuration register will be written
  if(comfound == 2) {
      p_byte |= 1 << 0;
      p_byte &= ~(1 << 1);
      DS75->reg_pointer = 1;
      transfer_buf[0] = DS75->conf_reg;
      transfer_bytes = 1;
  }
  
  //T_HYST
  else if(buf[0] == 't') {
    p_byte |= 1 << 1;
    p_byte &= ~(1 << 0);
    transfer_buf[0] = buf[1];
    transfer_buf[1] = buf[2];
    comfound = 1;
    DS75->reg_pointer = 2;
  }

  //T_OS
  else if(buf[0] == 'x') {
    p_byte |= 1 << 0;
    p_byte |= 1 << 1;
    transfer_buf[0] = buf[1];
    transfer_buf[1] = buf[2];
    comfound = 1;
    DS75->reg_pointer = 3;
  }
    
  if(comfound > 0) {
    DS75->client->addr = a_byte;
    smbret = i2c_smbus_write_i2c_block_data(DS75->client, p_byte, transfer_bytes, transfer_buf);
  }

  //User changes device's address
  else if(buf[0] == 'a') {
    if(buf[1] >= 0 && buf[1] <= 7) {
      DS75->address = buf[1];
      comfound = 1;
    }
  }
  
  else if(!comfound) pr_info("User has sent an invalid DS75 command"); //Tuolla read-funktiossa oli kai joku parempi tapa ilmoittaa virheistä
  if(smbret < 0) return -EFAULT;
  
  if(copy_to_user((void*)userbuf, transfer_buf, transfer_bytes)) {
    pr_info("Failed to return value to user space\n");
    return -EFAULT;
  }
  
  buf[count-1] = '\0';

  return count;
}

static const struct file_operations DS75_fops = {
	.owner = THIS_MODULE,
	.read = DS75_read_file,
	.write = DS75_write_file,
};

static int DS75_probe(struct i2c_client *client, const struct i2c_device_id *id) {
  
	static int counter = 0;

	struct DS75_dev *DS75;

	DS75 = devm_kzalloc(&client->dev, sizeof(struct DS75_dev), GFP_KERNEL);

	i2c_set_clientdata(client, DS75);

	DS75->client = client;

	sprintf(DS75->name, "DS75%02d", counter++);
	dev_info(&client->dev, "DS75_probe is entered on %s\n", DS75->name);

	DS75->DS75_miscdevice.name = DS75->name;
	DS75->DS75_miscdevice.minor = MISC_DYNAMIC_MINOR;
	DS75->DS75_miscdevice.fops = &DS75_fops;

	//Device's default values for its registers
	DS75->conf_reg = 0;
	DS75->reg_pointer = 0;
	DS75->address = 0;

	return misc_register(&DS75->DS75_miscdevice);

	dev_info(&client->dev, "DS75_probe is exited on %s\n", DS75->name);

	return 0;
}

static int DS75_remove(struct i2c_client *client) {
  struct DS75_dev *DS75;

  DS75 = i2c_get_clientdata(client);

  dev_info(&client->dev, "DS75_remove is entered on %s\n", DS75->name);

  misc_deregister(&DS75->DS75_miscdevice);

  dev_info(&client->dev, "DS75_remove is exited on %s\n", DS75->name);

  return 0;
}

static const struct of_device_id DS75_dt_ids[] = {
	{ .compatible = "DS75", },
	{ }
};
MODULE_DEVICE_TABLE(of, DS75_dt_ids);

static const struct i2c_device_id i2c_ids[] = {
	{ .name = "DS75", },
	{ }
};
MODULE_DEVICE_TABLE(i2c, i2c_ids);

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
MODULE_AUTHOR("J. Sihvola");
MODULE_DESCRIPTION("DS75 temperature sensor driver");
