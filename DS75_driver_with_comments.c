#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/cdev.h> //mknod -rekisteröintiin
#include <linux/of.h>
#include <linux/uaccess.h> //?
#include <linux/gpio/consumer.h> //?

//muutos
//maskit I2C-devicen komennoille, tämä on LED-koodista
//#define CMD_READ   0

/* This structure will represent single device */
//Tämä struct esittää laitekohtaista I/O -informaatiota
struct DS75_dev {
  struct i2c_client *client;
  struct miscdevice DS75_miscdevice;  //Tämä misc subsystem automaattisesti käsittelee open() -funktion. Automaattisesti luodussa open() -funktiossa se sitoo luomasi struct miscdevicen yksityiseen struct DS75_deviin avatulle tiedostolle. Näin voi read/write kernel callback -funktioissa recoverata miscdevice structin, jolla pääsee struct i2c_clientiin, joka includataan struct DS75_deviin. Kun saa struct i2c_clientin, voi kirjoittaa/lukea jokaiseen I2C-spesifiin laitteeseen SMBus-funktioilla
  //struct gpio_desc *display_cs; //TULEEKO TÄTÄ? TÄMÄ ON LED-AJURISTA. Perus GPIO-kamaa, luulisi, että tuo tulee.
  char name[8]; //I2C I/O devicen nimeä kantava struct, tähän 4? Täytetään probessa

  u8 address;
  u8 conf_reg; //T_HYST & T_OS regs have no variables since they are updated in their entirety
  u8 reg_pointer;
  //reg_pointer: 0 = temperature register, 1 = configuration register, 2 = hysteresis trip-point reg., 3 = over-temperature trip-point reg.
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


//kernelin callback-funktio, joka lukee laitteen inputin ja lähettää arvot käyttäjätilaan - eli tänne? vai minne?
//Tässä funktiossa:
//DS75 -pointteri laitetaan osoittamaan oikeaa device structia (container_of -makrolla)
//sitten read_word_data
//konvertoidaan expvalin (nyt: bytes_recvd) read_wordin palauttamat jutut intiksi sprintf:llä, koko sizeen (poistettu)
static ssize_t DS75_read_file(struct file *file, char __user *userbuf, size_t count, loff_t *ppos) {
  //loff_t osoittaa käyttäjätilasta lseekillä tms. tulevaan kohtaan. Ideana on, että kun lukee/kirjoittaa, seuraavalla kerralla jatketaan samasta kohdasta. POISTETAAN???
  //Ilmeisesti nuo ppos -jutut voi poistaa kokonaan
  //huom. count -muuttujaa ei ole käytetty edes esimerkkikoodin (io_expander) vastaavassa read-funktiossa. Se antaa isoja arvoja, mitäköhän ne ovat?


  int i;
  int bytes_recvd; //i2c_master_recv():n palautusarvo, monta byteä on luettu. Vaikka tämä on vakio, niin palautetaan nyt sitten, koska tämä fnktion määritelmä palautuksineen on annettu ylhäältä.
  //int size; //read_word_data():n palautusarvo, bufferin koko
  char buf[3];
  char read_buf[2];
  struct DS75_dev *DS75;
  u8 command = 0; //Kaikki bitit ovat 0
  //Address Byte in DS75:
  //Bit 0 = R/W, read = 1
  //Bit 1-3 = slave address, selectable part (decimals 0-7)
  //Bit 4-7 = slave address, fixed part (1001)

  printk("DS75_read_file\n");
  printk("count: %d\n", count);

  
  
  DS75 = container_of(file->private_data, struct DS75_dev, DS75_miscdevice);
  //dev = container_of(inode->i_cdev, struct scull_dev, cdev);  //container_of -makrolla haetaan tässä oikea device struct.
  //Tämän scull_open -funktion inode -argumentti sisältää tarvittavan datan i_cdev -kentässään, jossa on aiemmin pystyttämämme cdev struct. Kuitenkin haluamme sen scull_dev structin itse, joka sisältää cdev structin. Container of -makro ottaa pointterin (1. parametri), jonka tyyppi annetaan 3. parametrissä ja joka on 2. parametrin osoittamassa strutcissa. 


  //Asetetaan ne bitit, jotka eivät ole 0. HUOM. ORrilla ei voi asettaa bittejä 0:ksi, vain 1:ksi
  //pitäisikö nämä laittaa optionaalisiksi, että mikä se osoite on...
  //HUOM. silloin kun read-funktio (pointterin asetus)
  command = change_address(DS75->address);
  /*
  command |= 1 << 3; //fixed part
  command |= 1 << 6;
  */
  command |= 1 << 7; //Read-bit on

  //S Addr Wr [A] Comm [A] S Addr Rd [A] [DataLow] A [DataHigh] NA P
  //Start bit, Addr (7 bit), Wr bit, [Accept bit sent by device], Stop bit, Addr (7 bit)
  //https://manpages.debian.org/jessie-backports/linux-manual-4.8/i2c_smbus_read_word_data.9.en.html
  //https://www.includehelp.com/c/how-to-set-clear-and-toggle-a-single-bit-in-c-language.aspx

  //Printataan tuo command
  printk("command:\n");
  for(i = 7; i != -1; i--) {
    printk("%c", (command & (1 << i)) ? '1' : '0');
  }
  printk("\n");
  
  DS75->client->addr = command; //Osoite ja lukubitti i2c_client structiin, josta seuraava funktio osaa hakea ne itse
  bytes_recvd = i2c_master_recv(DS75->client, read_buf, 2);  //parametres: pointer to i2c_client, receiving buffer, how many bytes to red

  printk("bytes_recvd: %d\n", bytes_recvd);
  printk("i2c_client.name: %s\n", DS75->client->name);

  if(bytes_recvd < 0) return -EFAULT;

  short int testi1;
  testi1 = 0;

  //HUOM. memcpy:llä saisi ilmeisesti fiksummin siirrettyä nuo bitit toiseen datatyyppiin
  short int r1;
  r1 = (short int)read_buf[0];
  short int r2;
  r2 = (short int)read_buf[1];

  for(i = 7; i != -1; i--) {
    printk("%c", (r1 & (1 << i)) ? '1' : '0');
  }
  printk("\n");

  for(i = 7; i != -1; i--) {
    printk("%c", (r2 & (1 << i)) ? '1' : '0');
  }
  printk("\n");
  
  //  buf[size] = '\n';
  printk("read_buf: %s\n", read_buf);
  //  unsigned short luku = (unsigned short)read_buf;
  //printk("read_buf as 16-bit: %d\n", luku);  //Tämä antaa luvun, jossa ei ole nollia lopussa. Johtuuko siitä, että tavut on kääntyneet? Vai meneekö tuo konversio jotenkin pieleen?

  printk("r1: %d\n", r1);
  printk("r2: %d\n", r2);

  if(*ppos == 0) {  //jos pointterin positio on nolla...
    if(copy_to_user(userbuf, read_buf, 2)) {  //1. user-spacen target-bufferi, 2. kernel-spacen source bufferi 3. miten paljon kopioidaan)
      //Yllä oli alun perin parametrit userbuf, buf, size+1
      //Eli tässä nyt kopioidaan suoraan tuo char bufferi ilman mitään muokkausta, kuten #kernel-kanavalla joku neuvoi
      pr_info("Failed to return value to user space\n");
      //pr_info on printk, jossa on KERN_INFO -prioriteetti päällä
      return -EFAULT;
    }
    *ppos += 1;
    return bytes_recvd;
  }
  return 0;
}


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
  int smbret;
  unsigned long val;
  char buf[4];
  char transfer_buf[2];
  u8 transfer_bytes;
  int comfound; //1 = user sent command is found, 2 = command to read configuration register is found
  struct DS75_dev *DS75;
  u8 p_byte;
  u8 a_byte; //Kaikki bitit ovat 0
  int i; //DEBUG!!
  comfound = 0;
  p_byte = 0;
  transfer_bytes = 2;
  transfer_buf[0] = '\0'; //nollataan varmuuden vuoksi
  transfer_buf[1] = '\0';

  //TÄHÄN PITÄÄ LAITTAA, ETTÄ OSOITE OTETAAN JOSTAIN MUUALTA

  //DS75 -muuttuja osoittamaan i2c_client structiin
  DS75 = container_of(file->private_data, struct DS75_dev, DS75_miscdevice);

  a_byte = change_address(DS75->address);
  /*
  a_byte |= 1 << 3; //fixed part (?)
  a_byte |= 1 << 6; //
  */

  dev_info(&DS75->client->dev, "DS75_write_file entered on %s\n", DS75->name);
  dev_info(&DS75->client->dev, "we have written %zu characters\n", count); 

  printk("write_file, userbuf: %s", userbuf);
  printk("userbuf[0]: %c", userbuf[0]);
  printk("userbuf[1]: %c", userbuf[1]);
  
  if(copy_from_user(buf, userbuf, count)) { //Parameters: dest, source, size
    dev_err(&DS75->client->dev, "Bad copied value\n");
    return -EFAULT;
  }

  /*Käyttäjältä tulevat komennot:
    p0 - luetaan lämpötilarekisteriä 00
    p1 - luetaan configuration rekisteriä 01
    p2 - luetaan thyst-rekisteriä 10
    p3 - luetaan tos-rekisteriä 11
    HUOM!!! Tämän komennon lisäksi kirjoittaessa pointteri vaihtuu pysyvästi
   */

  //Tulkitaan käyttäjän lähettämä komento
  if(buf[0] == 'p') {
    if(buf[1] == '0') {
      //Eiks tässä riitä että p_byte = 0; ???
      p_byte &= ~(1 << 0);
      p_byte &= ~(1 << 1);
      //uutta
      /*
      p_byte &= ~(1 << 2);
      p_byte &= ~(1 << 3);
      p_byte &= ~(1 << 4);
      p_byte &= ~(1 << 5);
      p_byte &= ~(1 << 6);
      p_byte &= ~(1 << 7);
      */
      comfound = 1;
      DS75->reg_pointer = 0;
    }
    else if(buf[1] == '1') {
      p_byte |= 1 << 0;
      p_byte &= ~(1 << 1);
      printk("p1 -lokaatio");
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
    //Tässä siis kirjoitetaan DS75:en pointterirekisteriin uusi arvo ja sitten luetaan sen jälkeen normaalisti (siitä annetusta rekisteristä)
    if(comfound) {
      printk("p_byte: %d", p_byte);
      a_byte |= 1 << 7; //R/W -bit asentoon read
      DS75->client->addr = a_byte;
      smbret = i2c_smbus_read_i2c_block_data(DS75->client, p_byte, 1, transfer_buf); //i2c_client, u8 command, length, u8 *values)  komento on se address, values on ilmeisesti pointteri destiin. Command on tässä se, että mistä rekisteristä luetaan. Mihin address sitten tulee?
    //smbret:iin palautetaan luettujen tavujen määrä
    //TARVITAANKO TUOHON JOKU KONVERSIO JOKA OLI LIBERAL-KOODISSA?
    }
  }

  //Note: conf_reg is not to be reseted at any point after starting the device
  else if(buf[0] == 'r') { //Switching temperature resolution...
    if(buf[1] == '0') { //to 9-bit
      //6 = R1, 5 = R0
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
    //if(DS75->reg_pointer == 1) transfer_bytes = 1; //1 byte is enough for reading configuration register 
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
  printk("write_file, smbret: %d", smbret);
  
  printk("transfer_buf: %s\n", transfer_buf);

  if(copy_to_user(userbuf, transfer_buf, transfer_bytes)) {  //1. user-spacen target-bufferi, 2. kernel-spacen source bufferi 3. miten paljon kopioidaan)
    pr_info("Failed to return value to user space\n");
    return -EFAULT;
  }
  
  ///////////////
  short int r1;
  r1 = (short int)transfer_buf[0];
  short int r2;
  r2 = (short int)transfer_buf[1];

  for(i = 7; i != -1; i--) {
    printk("%c", (r1 & (1 << i)) ? '1' : '0');
  }
  printk("\n");

  for(i = 7; i != -1; i--) {
    printk("%c", (r2 & (1 << i)) ? '1' : '0');
  }
  printk("\n");
  /////////////
  
  //Tämä varmaan pois
  buf[count-1] = '\0';

  /*
  //LIBERAL-KOODIA
  // convert the string to an unsigned long
  ret = kstrtoul(buf, 0, &val);
  if (ret)
    return -EINVAL;

  dev_info(&DS75->client->dev, "the value is %lu\n", val);

  ret = i2c_smbus_write_byte(DS75->client, val);
  if (ret < 0)
    dev_err(&DS75->client->dev, "the device is not found\n");

  dev_info(&DS75->client->dev, 
	   "DS75_write_file exited on %s\n", DS75->name);
  */


  

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
	sprintf(DS75->name, "DS75%02d", counter++); //Ilmeisesti oktaalinumero desimaaliformaatissa
	dev_info(&client->dev, "DS75_probe is entered on %s\n", DS75->name);

	DS75->DS75_miscdevice.name = DS75->name;
	DS75->DS75_miscdevice.minor = MISC_DYNAMIC_MINOR;
	DS75->DS75_miscdevice.fops = &DS75_fops;

	//Device's default values for its registers
	DS75->conf_reg = 0;
	DS75->reg_pointer = 0;
	DS75->address = 0;

	/* Register misc device */
	return misc_register(&DS75->DS75_miscdevice);

	dev_info(&client->dev, "DS75_probe is exited on %s\n", DS75->name);

	return 0;
}

static int DS75_remove(struct i2c_client *client) {
  struct DS75_dev *DS75;

  /* Get device structure from bus device context */	
  DS75 = i2c_get_clientdata(client);

  dev_info(&client->dev, "DS75_remove is entered on %s\n", DS75->name);

  /* Deregister misc device */
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
MODULE_AUTHOR("Jari Sihvola");
MODULE_DESCRIPTION("DS75 temperature sensor driver");
