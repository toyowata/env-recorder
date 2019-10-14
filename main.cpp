
#include <string>
#include "mbed.h"
#include "NTPClient.h"
#include "BME280.h"
#include "BlockDevice.h"
#include "FATFileSystem.h"

BlockDevice *bd = BlockDevice::get_default_instance();
FATFileSystem fs("fs");

static volatile bool isPushed = false;

#if defined(TARGET_WIO_3G) || defined(TARGET_WIO_BG96)
DigitalOut GrovePower(GRO_POWR, 1);
#undef LED1
#define LED1 D20
#undef BUTTON1
#define BUTTON1 D19
#endif

BME280 sensor(I2C_SDA, I2C_SCL);
DigitalOut led(LED1);
InterruptIn btn1(BUTTON1);

void handleButtonRise() 
{
    isPushed = true;
}

int main(int argc, char* argv[])
{
    btn1.rise(&handleButtonRise);
    wait_ms(500);
#if 0
    NetworkInterface* network = NULL;

    printf("Envoironment data recorder test\n\n");
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
    time_t now = ntp.get_timestamp();
    now += (3600 * 9); // JST
    set_time(now);
    printf("Time is now %s", ctime(&now));
#endif
    
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
    
    FILE *f = fopen("/fs/record.txt", "a");
    printf("%s\n", (!f ? "Fail :(" : "OK"));
    if (!f) {
        error("error: %s (%d)\n", strerror(errno), -errno);
    }

    float t, h, p;
    char buf[64] = "\0";
    sensor.initialize();

    while(1) {
//        time(&now);
//        strftime(buf, sizeof(buf), "%Y/%m/%d %a %H:%M:%S", localtime(&now));
        t = sensor.getTemperature();
        h = sensor.getHumidity();
        p = sensor.getPressure(), 

        printf("%s, %5.2f DegC, %5.2f %%, %5.2f hPa\n", buf, t, h, p);
        fprintf(f, "%s, %5.2f, %5.2f, %5.2f\n", buf, t, h, p);
        if (err < 0) {
            printf("Fail :(\n");
            error("error: %s (%d)\n", strerror(errno), -errno);
        }
        led = !led;
        if (isPushed) {
            break;
        }
        wait(3);
    }

    fclose(f);
    printf("Safe to power off.\n");
    while(1) {
        led = !led;
        wait(0.4f);
    }
}
