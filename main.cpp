
#include <string>
#include "mbed.h"
#include "NTPClient.h"
#include "BME280.h"
#include "BlockDevice.h"
#include "FATFileSystem.h"

#define MEASURE_PERIOD (30 * 1000) // 30 sec

BlockDevice *bd = BlockDevice::get_default_instance();
FATFileSystem fs("fs");

#if defined(TARGET_WIO_3G) || defined(TARGET_WIO_BG96)
DigitalOut GrovePower(GRO_POWR, 1);
#undef LED1
#define LED1 D20
#undef BUTTON1
#define BUTTON1 D19
#endif

BME280 sensor(I2C_SDA, I2C_SCL);
DigitalOut led(LED1);
InterruptIn button(BUTTON1);
FILE *fp = (FILE *)NULL;
time_t now;
EventQueue queue;

void button_handler(void);
void record_handler(void);

void button_handler() 
{
    fclose(fp);
    fp = NULL;
    printf("Safe to power off.\n");
    
    while(1) {
        led = !led;
        wait(0.2f);
    }
}

void record_handler()
{
    float t, h, p;
    char buf[64] = "\0";

    time(&now);
    strftime(buf, sizeof(buf), "%Y/%m/%d %a %H:%M:%S", localtime(&now));
    t = sensor.getTemperature();
    h = sensor.getHumidity();
    p = sensor.getPressure(), 

    printf("%s, %5.2f DegC, %5.2f %%, %5.2f hPa\n", buf, t, h, p);
    fprintf(fp, "%s, %5.2f, %5.2f, %5.2f\n", buf, t, h, p);
    led = !led;
}

int main(int argc, char* argv[])
{
    NetworkInterface* network = NULL;

    wait(1);
    printf("\n== Envoironment data recorder example ==\n\n");
    printf("Opening network interface...\r\n");

    network = NetworkInterface::get_default_instance();
    if (!network) {
        printf("Unable to open network interface.\r\n");
        return -1;
    }
    network->connect();
    const char *ip = network->get_ip_address();
    printf("IP address: %s\n", ip ? ip : "None");

    printf("Network interface opened successfully.\r\n");
    printf("\r\n");

    // sync the real time clock (RTC)
    NTPClient ntp(network);
    const char* serverAddress = "time.google.com";
    int port = 123;
    ntp.set_server(const_cast<char *>(serverAddress), port);
    now = ntp.get_timestamp();
    now += (3600 * 9); // Adjust to JST timezone
    set_time(now);
    printf("Time is now %s", ctime(&now));
    
    // Try to mount the filesystem
    printf("Mounting the filesystem... ");
    fflush(stdout);
    int err = fs.mount(bd);
    printf("%s\n", (err ? "Fail :(" : "OK"));
    if (err) {
        // Reformat if we can't mount the filesystem
        // this should only happen on the first boot
        printf("No filesystem found, formatting... ");
        fflush(stdout);
        err = fs.reformat(bd);
        printf("%s\n", (err ? "Fail :(" : "OK"));
        if (err) {
            error("error: %s (%d)\n", strerror(-err), err);
        }
    }

    fp = fopen("/fs/record.txt", "a");
    printf("%s\n", (!fp ? "Fail :(" : "OK"));
    if (!fp) {
        error("error: %s (%d)\n", strerror(errno), -errno);
    }

    sensor.initialize();

    button.rise(queue.event(button_handler));
    queue.call_every(MEASURE_PERIOD, record_handler);
    queue.dispatch();
}
