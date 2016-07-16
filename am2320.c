/*
 *  AM2320 I2C Driver
 *
 *  References 
 *  http://lxr.free-electrons.com/source/include/uapi/asm-generic/i2c.h
 *  http://lxr.free-electrons.com/source/include/uapi/asm-generic/errno-base.h
 *  http://lxr.free-electrons.com/source/Documentation/i2c/writing-clients
 *  https://www.kernel.org/doc/Documentation/timers/timers-howto.txt
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/sysfs.h>

#define AM2320_NAME "am2320"
#define AM2320_I2C_ADDRESS 0x5C

#define AM2320_UPDATE_INTERVAL_MILLIES 1000

#define AM2320_I2C_HIGH_HUMIDITY    0x00
#define AM2320_I2C_LOW_HUMIDITY     0x01
#define AM2320_I2C_HIGH_TEMPERATURE 0x02
#define AM2320_I2C_LOW_TEMPERATURE  0x03

#define AM2320_I2C_FUNC_READ        0x03

// Used for auto detection
static const unsigned short normal_i2c[] = {
    AM2320_I2C_ADDRESS,
    I2C_CLIENT_END
};

struct am2320_device {
    struct i2c_client *client;
    struct mutex lock;
    u8 tbuffer[8]; // Used as the transaction buffer
                   // On recieve
                  // sensor responds with Function code, Num bytes, Reg. 1, Reg. 2, Reg. 3, Reg. 4
                  // CRC 2 bytes
    unsigned long last_updated;     //In jiffies
    unsigned int update_interval;   //In milliseconds
    u8 valid;                       //Validity of data
    struct attribute_group *group;
};

struct am2320_data {
    s16 tempterature;
    s16 humidity;
};
//------------- Prototypes ----------------------------//
static int am2320_i2c_read_data(struct i2c_client *client, u8 func, u8 reg, u8 len);
static void am2320_update_data(struct i2c_client *client, struct am2320_data *data);
//-----------------------------------------------------//

//----------- Sensor sysfs ---------------------------------------------------//
static ssize_t humidity_input(struct device *dev, struct device_attribute *attr, 
        char *buf){
    struct i2c_client  *client  = to_i2c_client(dev);
    //struct am2320_device *am2320 = i2c_get_clientdata(client);
    struct am2320_data readings;
    am2320_update_data(client, &readings);

    return sprintf(buf, "%d\n", readings.humidity);
}
static ssize_t temp_input(struct device *dev, struct device_attribute *attr, 
        char *buf){
    struct i2c_client  *client  = to_i2c_client(dev);
    //struct am2320_device *am2320 = i2c_get_clientdata(client);
    struct am2320_data readings;
    am2320_update_data(client, &readings);

    return sprintf(buf, "%d\n", readings.tempterature);
}

static ssize_t temp_max(struct device *dev, struct device_attribute *attr, 
        char *buf){
    //struct i2c_client  *client  = to_i2c_client(dev);
    //struct am2320_device *am2320 = i2c_get_clientdata(client);

    return sprintf(buf, "100\n");
}

static ssize_t temp_min(struct device *dev, struct device_attribute *attr, 
        char *buf){
    //struct i2c_client  *client  = to_i2c_client(dev);
    //struct am2320_device *am2320 = i2c_get_clientdata(client);

    //return snprintf(buf, PAGE_SIZE - 1, "0\n");
    return sprintf(buf, "0\n");
}

static SENSOR_DEVICE_ATTR(temp1_max, S_IWUSR | S_IRUGO, temp_max, NULL,  100);
static SENSOR_DEVICE_ATTR(temp1_min, S_IWUSR | S_IRUGO, temp_min, NULL,  0);
static SENSOR_DEVICE_ATTR(temp1_input, S_IWUSR | S_IRUGO, temp_input, NULL,  0);
static SENSOR_DEVICE_ATTR(humidity1_input, S_IWUSR | S_IRUGO, humidity_input, NULL,  0);

//static struct attribute *am2320_attrs[] = {
static struct device_attribute *am2320_attrs[] = {
    &sensor_dev_attr_temp1_max.dev_attr.attr,
    &sensor_dev_attr_temp1_min.dev_attr.attr,
    &sensor_dev_attr_temp1_input.dev_attr.attr,
    &sensor_dev_attr_humidity1_input.dev_attr.attr,
    NULL
};

static const struct attribute_group am2320_group = {
    .attrs = am2320_attrs,
};
//------------------------------------------------------------------------------//

static int am2320_wakeup(struct i2c_client *client){
    i2c_smbus_read_byte_data(client, AM2320_I2C_HIGH_HUMIDITY);
    return 0;
}

static int am2320_i2c_detect(struct i2c_client *client, 
        struct i2c_board_info *info){

    //struct i2c_adapter *adapter = client->adapter;
    
    if(client->addr != AM2320_I2C_ADDRESS)
        return -ENODEV;

    dev_info(&client->dev, "Detected AM2320\n");

    return 0;
}

// Reads the data from the device, currently it reads only 4 bytes
// TODO Improve this function to read any number of registers
// TODO CRC check
static int am2320_i2c_read_data(struct i2c_client *client, u8 func, u8 reg, u8 len){
    int ret;
    struct am2320_device *am2320;
    
    am2320 = i2c_get_clientdata(client);
    
    am2320->tbuffer[0] = func;
    am2320->tbuffer[1] = reg;
    am2320->tbuffer[2] = len;
    
    //Wakeup device
    am2320_wakeup(client);
    mutex_lock(&am2320->lock);
    ret = i2c_master_send(client, am2320->tbuffer, 3);
    if(ret < 0){
        dev_err(&client->dev, "Failed to send commands\n");
        goto exit_lock;
    }
    // Delay as per datasheet
    usleep_range(2000, 3000);
    //ret = i2c_master_recv(device->client, device->tbuffer, len);
    ret = i2c_master_recv(client, am2320->tbuffer, 8);
    if(ret < 0){
        dev_err(&client->dev, "Failed to read data\n");
        goto exit_lock;
    }

    //am2320->last_updated = jiffies;
    //am2320->valid = 1;

    mutex_unlock(&am2320->lock);
    //Do a CRC here

    return 0;
    
exit_lock:
    mutex_unlock(&am2320->lock);
    return ret;
    
}

static void am2320_update_data(struct i2c_client *client, struct am2320_data *data){
    struct am2320_device *am2320;
    
    am2320 = i2c_get_clientdata(client);
    if(am2320_i2c_read_data(client, AM2320_I2C_FUNC_READ, AM2320_I2C_HIGH_HUMIDITY, 4) == 0){
        data->humidity     = (am2320->tbuffer[2] << 8) | (am2320->tbuffer[3]);
        data->tempterature = (am2320->tbuffer[4] << 8) | (am2320->tbuffer[5]);

        am2320->last_updated = jiffies;
        am2320->valid = 1;
    }
}
/*
static irqreturn_t am2320_interrupt_thread(int irq, void *data){
    struct am2320_device *am2320 = data;

    return IRQ_HANDLED;
}
*/
static int am2320_i2c_probe(struct i2c_client *client, 
        const struct i2c_device_id *id){

    struct i2c_adapter *adapter = client->adapter;
    struct am2320_device *am2320;
    //TODO Remove this
    //struct am2320_data readings;
    //struct device *hwmon_dev;
    
    int ret;
    int error;

    dev_info(&client->dev, "Probe AM2320\n");

    if(!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA) && 
            !i2c_check_functionality(adapter, I2C_FUNC_SMBUS_I2C_BLOCK))
        return -EIO;
        
    am2320 = devm_kzalloc(&client->dev, sizeof(struct am2320_device), GFP_KERNEL);
    if(!am2320){
        dev_err(&client->dev, "Cannot allocate memory\n");
        return -ENOMEM;
    }
    
    am2320->client = client;
    am2320->valid = 0;
    am2320->update_interval = AM2320_UPDATE_INTERVAL_MILLIES;
    am2320->last_updated = 0;

    //am2320->group = &am2320_group;

    i2c_set_clientdata(client, am2320);
    mutex_init(&am2320->lock);



    //Registering sysfs files
    device_create_file(&client->dev,  (const struct device_attribute *)&sensor_dev_attr_temp1_max);
    device_create_file(&client->dev,  (const struct device_attribute *)&sensor_dev_attr_temp1_min);
    device_create_file(&client->dev,  (const struct device_attribute *)&sensor_dev_attr_temp1_input);
    device_create_file(&client->dev,  (const struct device_attribute *)&sensor_dev_attr_humidity1_input);
        
    /*am2320_i2c_read_data(client, AM2320_I2C_FUNC_READ, AM2320_I2C_HIGH_HUMIDITY, 4);

    dev_info(&client->dev, "Read values = %x, %x, %x, %x, %x, %x, %x, %x\n", am2320->tbuffer[0], am2320->tbuffer[1], am2320->tbuffer[2], am2320->tbuffer[3], am2320->tbuffer[4], am2320->tbuffer[5], am2320->tbuffer[6], am2320->tbuffer[7]);*/
    //am2320_update_data(client, &readings);
    //dev_info(&client->dev, "READINGS = H=%d, T=%d\n", readings.humidity, readings.tempterature);

    /*hwmon_dev = hwmon_device_register_with_groups(&client->dev, 
                                                    client->name, am2320, am2320->group);*/


    return 0;
    /*
    error = request_threaded_irq(client->irq, NULL, 
            am2320_interrupt_thread,IRQF_TRIGGER_RISING | IRQF_ONESHOT,
            "am2320", am2320);

    if(error){
        dev_err(&client->dev, "Cannot get IRQ %d error %d", client->irq, error);
    }*/

err_free_res:

    return 0;
}


static int am2320_i2c_remove(struct i2c_client *client){
    struct am2320_device *am2320;

    dev_info(&client->dev, "Remove AM2320\n");
    
    am2320 = i2c_get_clientdata(client);
    
    return 0;
}



static const struct i2c_device_id am2320_id[] = {
    {AM2320_NAME, 0},
    {}
};

MODULE_DEVICE_TABLE(i2c, am2320_id);

static struct i2c_driver am2320_i2c_driver = {
    //.class = I2C_CLASS_HWMON,
    .driver = {
        .name = AM2320_NAME
    },
    .id_table = am2320_id,
    .probe    = am2320_i2c_probe,
    .remove   = am2320_i2c_remove,
    .detect   = am2320_i2c_detect,
    .address_list = normal_i2c
};

module_i2c_driver(am2320_i2c_driver);

MODULE_AUTHOR("D.N. Amerasinghe <nivanthaka@gmail.com>");
MODULE_DESCRIPTION("Am2320 I2C bus driver");
MODULE_LICENSE("GPL");
