// Author: Ahmet Efe Karagoz & Yavuz Selim Al

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/mman.h>

#define BCM2711_PERI_BASE 0xFE000000
#define GPIO_BASE (BCM2711_PERI_BASE + 0x200000)
#define PWM_BASE (BCM2711_PERI_BASE + 0x20C000)
#define CLOCK_BASE (BCM2711_PERI_BASE + 0x101000)
#define BLOCK_SIZE (4 * 1024)

#define GPFSEL1 (*(gpio_base + 0x04/4))
#define GPPUD (*(gpio_base + 0x94/4))
#define GPPUDCLK0 (*(gpio_base + 0x98/4))

#define PWM_CTL (*(pwm_base + 0x00/4))
#define PWM_RNG1 (*(pwm_base + 0x10/4))
#define PWM_DAT1 (*(pwm_base + 0x14/4))
#define PWMCLK_CNTL (*(clk_base + 0xA0/4))
#define PWMCLK_DIV (*(clk_base + 0xA4/4))

volatile unsigned int *gpio_base;
volatile unsigned int *pwm_base;
volatile unsigned int *clk_base;


void reset_gpio12(){
    printf("Resetting GPIO12...\n");
    GPFSEL1 &= ~(0x7 << 6);  // Clear bits 6-8 for GPIO12 (set as input)
    printf("GPIO12 reset completed.\n");
}

void setup_gpio12_pwm0(){
    printf("Configuring GPIO12 for PWM...\n");
    GPFSEL1 &= ~(0x7 << 6);  // Clear bits 6-8 for GPIO12
    GPFSEL1 |= (0x4 << 6);   // Set alt function 0 (PWM0) for GPIO12

    GPPUD = 0x0;
    for(volatile int i = 0; i<150; i++);
    GPPUDCLK0 = (0x1 << 12); // Enable clock for GPIO12
    for(volatile int i = 0; i<150; i++);
    GPPUDCLK0 = 0x0;
    printf("Configuration is done.\n");
}

void setup_pwm(){
    printf("Adjusting PWM parameters...\n");

    // Stop PWM clock and waiting for busy flag doesn't work
    PWMCLK_CNTL = 0x5A000000 | (1 << 5);  // Stop PWM clock
    usleep(10);  

    // Set PWM clock divider
    PWMCLK_DIV = 0x5A000000 | (1 << 12); // Set clock divider to 32 (12.9 MHz / 32 = 600 kHz)
    usleep(10);  

    // Enable PWM clock
    PWMCLK_CNTL = 0x5A000011;  // Source=osc and enable
    usleep(10);       

    // Setup PWM
    PWM_CTL = 0x0; // Stop PWM
    usleep(10); 

    PWM_RNG1 = (int)13168/50; // Set range register
    usleep(10); 

    PWM_CTL = 0x81; // Start PWM with PWM1 enabled
    printf("Adjustments of PWM are done.\n");
}

void set_pwm_duty_cycle(float duty_cycle){
    if(duty_cycle < 0) duty_cycle = 0;
    if(duty_cycle > 100) duty_cycle = 100;

    float value = (duty_cycle * PWM_RNG1) / 100;
    PWM_DAT1 = (int)value;
    printf("Duty cycle is: %d%%\n", (int)duty_cycle);
}


int translate(char packet_data[1024]){

    if (strcmp(packet_data, "S5F") == 0) {
        PWM_DAT1 = 20;
    } else if (strcmp(packet_data, "S3F") == 0) {
        PWM_DAT1 =16;
    } else if (strcmp(packet_data, "S7F") == 0) {
        PWM_DAT1= 24;
    }
    else if (strcmp(packet_data, "S2F") == 0){ 
        set_pwm_duty_cycle(0);
        reset_gpio12();
        return 1;
    }
    
	/*
	S1F = 5 
	S5F = 7.5
	S6F = 8.0
	S7F = 8.5
	S8F = 9
	S9F = 9.5
	*/
}

int main(int argc , char *argv[]){
    printf("Esenlikler.\n\n");

    int socket_desc , new_socket , c , *new_sock;
	int mem_fd;
	struct sockaddr_in server , client;
	char *message;

    // Create socket
    socket_desc = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_desc == -1) printf("Could not create socket.");

    server.sin_addr.s_addr = inet_addr("192.168.1.97"); // IP adress of RPi Model 4B
	server.sin_family = AF_INET;
	server.sin_port = htons( 8887 );

    //Bind
	if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0){
		puts("Bind failed!");
		return 1;
	}
	puts("Bind done.");

	if ((mem_fd = open("/dev/mem", O_RDWR | O_SYNC)) < 0) {
        perror("Failed to open /dev/mem!");
        return -1;
    }

    gpio_base = (volatile unsigned int *)mmap(NULL, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, mem_fd, GPIO_BASE);
    if (gpio_base == MAP_FAILED) {
        perror("Failed to map the GPIO memory!");
        close(mem_fd);
        return -1;
    }
    printf("GPIO memory has mapped.\n");

    pwm_base = (volatile unsigned int *)mmap(NULL, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, mem_fd, PWM_BASE);
    if (pwm_base == MAP_FAILED) {
        perror("Failed to map the PWM memory!");
        munmap((void*)gpio_base, BLOCK_SIZE);
        close(mem_fd);
        return -1;
    }
    printf("PWM memory has mapped.\n");

    clk_base = (volatile unsigned int *)mmap(NULL, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, mem_fd, CLOCK_BASE);
    if (clk_base == MAP_FAILED) {
        perror("Failed to map the Clock memory.");
        munmap((void*)gpio_base, BLOCK_SIZE);
        munmap((void*)pwm_base, BLOCK_SIZE);
        close(mem_fd);
        return -1;
    }
    printf("Clock memory has mapped.\n");

    setup_gpio12_pwm0();
    setup_pwm();

    printf("GPIO12 is currently in use for PWM generation.\n");

    
	while ( 1 )
    {
		char packet_data[1024];	
		int duty_cycle;

		socklen_t len= sizeof(client); 
		int received_bytes = recvfrom( socket_desc, packet_data, sizeof(packet_data),0, (struct sockaddr*)&client, &len );
		if ( received_bytes > 0 )
	    printf("Incoming: %s\n",packet_data);
		int kapa = translate(packet_data);
        if (kapa == 1){ break;}
    }

	munmap((void*)gpio_base, BLOCK_SIZE);
    munmap((void*)pwm_base, BLOCK_SIZE);
    munmap((void*)clk_base, BLOCK_SIZE);
    close(mem_fd);

    return 0;
}