#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/io.h>		/* Linux equivalent of machine/cpufunc.h - provides inb/outb/ioperm */

/* W83627 control registers */
/*
 *	You must set these - there is no sane way to probe for this board.
 *	You can use w83627=x to set these now.
 */
#define EFER		0x2e	/* Extended Function Enable Registers */
#define EFIR		0x2e	/* Ext Funct Index Reg (same as EFER) */
#define EFDR		0x2f	/* Extended Func Data Register */
#define	LDR	    	0x07	/* Logical device register */
#define bank_select	0x4e	/* W83627 Bank Select Register*/
#define sensor2_hb	0x50	/* W83627 Temperature Sensor 2 on Bank1 High Byte*/
#define sensor2_lb	0x51	/* W83627 Temperature Sensor 2 on Bank1 Low Byte*/

#define io		0x2e	/* standard SuperIO config address */
#define def_xe_siobase	0x290	/* The default SuperIO base address on the X-e box */
//#define siobase	0x295	/* standard SuperIO data address */
//#define siobase2	0xa05	/* XTM5 SuperIO data address */
#define gpiobase	0x480	/* standard ICH7, ICH6 and ICH5 address */
#define gpiobase2	0x4080	/* gpiobase for ICH2 in X-core model */
#define gpiobase3	0x1c00	/* gpiobase for C226 in M400/500 model */
#define parallel_port	0x378 	/* parallel port address in the firebox */

/* Program History
 *
 * Version 1.0 of this program is not significant other than it adds 64bit capability
 * Version 1.1 fixed a bug in get_w83627_addr_port left the chip in extended function mode
 *	 	       Added a function to check the value of the Superio base address and reset it
 *		       if neccessary on the X-e box. Testing has shown that it can change value
 *		       rendering it unreadabel at any address
 * Version 1.2 Added M400/500 support
 * Version 1.3 Added XTM8 support
 * Version 1.4 Added XTM800/1500 detection
 *             Added M370/470/570/670 support 
 * Version 1.5 Added M270 support
 * Version 1.6 Added M400 fan control for those who have not flashed the BIOS
 *             Added T70 detection 
 * Version 1.6_1 Fixed M400 fan control
 * Version 1.7 (Ubuntu/Linux port) - replaced FreeBSD /dev/io + kenv() with
 *             Linux ioperm()/sys/io.h and /sys/class/dmi/id sysfs reads.
 */

/*
lcdproc code starts here (ported to Linux ioperm() instead of /dev/io)
*/
/* Read a byte from port */
static inline int port_in (unsigned short port) {
        return inb(port);
}
/* Write a byte 'val' to port */
static inline void port_out (unsigned short port, unsigned char val) {
        /* NOTE: glibc's outb() takes (value, port) - the reverse order of
           FreeBSD's outb(port, value). The wrapper keeps the rest of the
           code (which calls port_out(port, val) everywhere) unchanged. */
        outb(val, port);
}
/* Get access to a specific port */
static inline int port_access (unsigned short port) {
	if (ioperm(port, 1, 1) == 0) {
		return 0; /* Success */
	} else {
		return -1; /* Failure */
	}
}

/* Close access to a specific port */
static inline int port_deny (unsigned short port) {
	if (ioperm(port, 1, 0) == 0) {
		return 0;
	} else {
		return -1;
	}
}

/* Get access to multiple ports at once */
static inline int port_access_multiple (unsigned short port, unsigned short count) {
	if (ioperm(port, count, 1) == 0) {
		return 0;
	} else {
		return -1;
	}
}

/* Close access to multiple ports at once */
static inline int port_deny_multiple (unsigned short port, unsigned short count) {
	if (ioperm(port, count, 0) == 0) {
		return 0;
	} else {
		return -1;
	}
}
/*
lcdproc code stops here
*/

/* Linux replacement for FreeBSD's kenv(KENV_GET, "smbios.system.product", ...).
   Reads a single-line sysfs DMI attribute, e.g. /sys/class/dmi/id/product_name
   or /sys/class/dmi/id/board_name, and strips the trailing newline. */
static int get_dmi_string(const char *path, char *buf, size_t buflen)
{
	FILE *f = fopen(path, "r");
	if (f == NULL) {
		return -1;
	}
	if (fgets(buf, buflen, f) == NULL) {
		fclose(f);
		return -1;
	}
	fclose(f);

	size_t len = strlen(buf);
	if (len > 0 && buf[len - 1] == '\n') {
		buf[len - 1] = '\0';
	}
	return (int) strlen(buf);
}

/*
Functions for interacting with the Winbond W83627 SuperIO chip 
*/
/* Function to set W83627 in extended function mode */
static void w83627_enter_ext_mode(void)
{
	int retval = 0;  
	retval = port_access(EFER);
 	 if (retval==0)
 	 {
 		port_out(EFER, 0x87);		/* Enter extended function mode */
 		port_out(EFER, 0x87);		/* Do it twice !! */
		// printf("Entered Extended Function Mode\n");
		retval = port_deny(EFER);
    		if (retval==0)
    		{}
    		else
    		{
     		printf("close fail\n");
    		}
  	  }
	 else
  	  {
   	  printf("open fail\n");
  	  }
}
/* Function to set W83627 in normal mode */
static void w83627_leave_ext_mode(void)
{
	int retval = 0;  
	retval = port_access(EFER);
 	 if (retval==0)
 	 {
 		port_out(EFER, 0xAA);		/* Leave extended function mode */
 		// printf("Left Extended Function Mode\n");
		retval = port_deny(EFER);
    		if (retval==0)
    		{}
    		else
    		{
     		printf("close fail\n");
    		}
  	  }
	 else
  	  {
   	  printf("open fail\n");
  	  }
}
/* Function to get config registers from w83627 */
int get_w83627_config(int reg1)
{
  int val1 = 0;
  int retval = 0;  
 
  retval = port_access_multiple(EFIR,2);
  if (retval==0)
  {
	port_out(EFIR, reg1);
	val1 = port_in(EFDR);	
    retval = port_deny_multiple(EFIR,2);
    if (retval==0)
    {}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }
  return val1;
}
/* Function to set config registers on w83627 */
void set_w83627_config(int reg1, int val1)
{
  int retval = 0;  
 
  retval = port_access_multiple(EFIR,2);
  if (retval==0)
  {
	port_out(EFIR, reg1);
	port_out((EFDR), val1);	
	// printf("Setting %x to %x\n",reg1, val1);
    retval = port_deny_multiple(EFIR,2);
    if (retval==0)
    {}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }
}
/* Function to get W83627 address port */
int get_w83627_addr_port(void)
{
	int sio_addr_port = 0x0;
	int hi_byte = 0x0;
	int lo_byte = 0x0; 
 
	w83627_enter_ext_mode();		/* Enter extended function mode */
	set_w83627_config(LDR, 0x0b);		/* Set logical device register to B */
	hi_byte = get_w83627_config(0x60);	/* Read SIO base address high byte */
	lo_byte = get_w83627_config(0x61);	/* Read SIO base address low byte */
	sio_addr_port = ((hi_byte * 0x100) + lo_byte + 0x05);	/* Calculate SIO data address */
	//printf("SuperIO address port is %x\n",sio_addr_port);	
	w83627_leave_ext_mode();		/* Leave extended function mode */
return sio_addr_port;
}
/* Function to set W83627 base address */
void set_w83627_base_addr(int sio_port)
{
	int hi_byte = 0x0;
	int lo_byte = 0x0; 
	hi_byte = (sio_port / 0x100);		/* Calculate high byte */
	lo_byte = (sio_port - (hi_byte * 0x100));	/* Calculate low byte */
	//printf("Calculated high byte is %x, low byte is %x\n", hi_byte, lo_byte);
 
	w83627_enter_ext_mode();
	set_w83627_config(LDR, 0x0b);		/* Set logical device register to B */
	set_w83627_config(0x60, hi_byte);	/* Set SIO base address high byte */
	set_w83627_config(0x61, lo_byte);	/* Set SIO base address low byte */
	printf("SuperIO base address set to %x\n",sio_port);	
	w83627_leave_ext_mode();
}
/* Function to check W83627 base address */
void check_siobase(void)
{
	if ((get_w83627_addr_port() - 0x05) != def_xe_siobase)
		{
		printf("SuperIO data address has changed! Resetting to default value (0x290)\n");
		set_w83627_base_addr(def_xe_siobase);
		}
	else
		{
		//printf("All is well!\n");
		}
}
/* Function to get data from w83627 */
int getw83627(int reg1)
{
  int val1 = 0;
  int retval = 0;  
  int siobase = get_w83627_addr_port();
  retval = port_access_multiple(siobase,2);
  if (retval==0)
  {
	port_out(siobase, reg1);
	val1 = port_in(siobase+1);	
    retval = port_deny_multiple(siobase,2);
    if (retval==0)
    {}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }
  return val1;
}
/* Function to set data on w83627 */
void setw83627(int reg1,int val1)
{
  int retval = 0; 
  int siobase = get_w83627_addr_port(); 
 
  retval = port_access_multiple(siobase,2);
  if (retval==0)
  {
	port_out(siobase, reg1);
	port_out((siobase+1), val1);	
    retval = port_deny_multiple(siobase,2);
    if (retval==0)
    {}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }
}
/* DIAGNOSTIC / READ-ONLY: dump every register (0x00-0x60) in a given W83627
   hardware-monitor bank. The only "write" performed is selecting the bank via
   bank_select (register 0x4e) - the same thing the existing "-f" read path
   already does - nothing else is ever written. Used to figure out which
   register actually holds fan speed / setpoints on a chip variant we don't
   have documentation for, by comparing dumps before/after a known change
   (e.g. changing the fan setting in the BIOS) instead of guessing blind. */
void dump_w83627_bank(int bank)
{
	int i;
	int val;

	setw83627(bank_select, bank);	/* Select the requested bank */

	printf("W83627 Bank %d register dump (read-only, 0x00-0x7f):\n", bank);
	printf("idx: ");
	for (i = 0x00; i <= 0x7f; i++) {
		printf("%02x ", i);
	}
	printf("\nval: ");
	for (i = 0x00; i <= 0x7f; i++) {
		val = getw83627(i);
		printf("%02x ", val);
	}
	printf("\n");

	setw83627(bank_select, 0x00);	/* Restore to bank 0 afterwards, for safety */
}
/* DIAGNOSTIC / READ-ONLY: read the actual measured fan tachometer speeds in
   RPM from Bank 6 (indices 0x50-0x59), per the official Nuvoton
   W83627DHG-P datasheet section 9.119-9.128. This is ground truth measured
   by the chip's own tachometer inputs, independent of what any fan output
   value register (0x01/0x03/0x11/0x61) reports - useful for confirming
   whether a write to an output register is having any physical effect at
   all. Only writes performed: selecting Bank 6 via bank_select (0x4e), and
   restoring Bank 0 afterwards - no fan/output registers are touched. */
void dump_w83627_fan_rpms(void)
{
	int hb, lb;

	setw83627(bank_select, 0x06);	/* Select Bank 6 */

	printf("W83627 measured fan speeds (Bank 6, read-only):\n");

	hb = getw83627(0x50); lb = getw83627(0x51);
	printf("  SYSFANIN  : %d RPM\n", (hb << 8) | lb);

	hb = getw83627(0x52); lb = getw83627(0x53);
	printf("  CPUFANIN0 : %d RPM\n", (hb << 8) | lb);

	hb = getw83627(0x54); lb = getw83627(0x55);
	printf("  AUXFANIN0 : %d RPM\n", (hb << 8) | lb);

	hb = getw83627(0x56); lb = getw83627(0x57);
	printf("  CPUFANIN1 : %d RPM\n", (hb << 8) | lb);

	hb = getw83627(0x58); lb = getw83627(0x59);
	printf("  AUXFANIN1 : %d RPM\n", (hb << 8) | lb);

	setw83627(bank_select, 0x00);	/* Restore to bank 0 afterwards, for safety */
}
/*
Functions for interacting with the ITE IT8718F SuperIO chip 
*/
/* Function to set IT87 in MBPnP mode */
static void it87_enter_mbpnp_mode(void)
{
	int retval = 0;  
	retval = port_access(EFER);
 	 if (retval==0)
 	 {
 		port_out(EFER, 0x87);
 		port_out(EFER, 0x01);
 		port_out(EFER, 0x055);
 		port_out(EFER, 0x055);
		// printf("Entered MBPnP Mode\n");
		retval = port_deny(EFER);
    		if (retval==0)
    		{}
    		else
    		{
     		printf("close fail\n");
    		}
  	  }
	 else
  	  {
   	  printf("open fail\n");
  	  }
}
/* Function to set IT87 in normal mode */
static void it87_leave_mbpnp_mode(void)
{
	int retval = 0;  
     retval = port_access_multiple(EFER,2);
 	 if (retval==0)
 	 {
 		port_out(EFER, 0x02);		/* Leave extended function mode */
 		port_out(EFDR, 0x02); 
		// printf("Left MBPnP Mode\n");
		retval = port_deny_multiple(EFER,2);
    		if (retval==0)
    		{}
    		else
    		{
     		printf("close fail\n");
    		}
  	  }
	 else
  	  {
   	  printf("open fail\n");
  	  }
}
/* Function to get config registers from IT87 */
int get_it87_config(int reg1)
{
  int val1 = 0;
  int retval = 0;  
 
  retval = port_access_multiple(EFIR,2);
  if (retval==0)
  {
	port_out(EFIR, reg1);
	val1 = port_in(EFDR);	
    retval = port_deny_multiple(EFIR,2);
    if (retval==0)
    {}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }
  return val1;
}
/* Function to set config registers on IT87 */
void set_it87_config(int reg1, int val1)
{
  int retval = 0;  
 
  retval = port_access_multiple(EFIR,2);
  if (retval==0)
  {
	port_out(EFIR, reg1);
	port_out((EFDR), val1);	
	// printf("Setting %x to %x\n",reg1, val1);
    retval = port_deny_multiple(EFIR,2);
    if (retval==0)
    {}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }
}
/* Function to get IT87 address port */
int get_it87_addr_port(void)
{
	int sio_addr_port = 0x0;
	int hi_byte = 0x0;
	int lo_byte = 0x0; 
 
	it87_enter_mbpnp_mode();		/* Enter MNPnP mode */
	set_it87_config(LDR, 0x04);		/* Set logical device register to B */
	hi_byte = get_it87_config(0x60);	/* Read SIO base address high byte */
	lo_byte = get_it87_config(0x61);	/* Read SIO base address low byte */
	sio_addr_port = ((hi_byte * 0x100) + lo_byte + 0x05);	/* Calculate SIO data address */
	//printf("SuperIO address port is %x\n",sio_addr_port);	
	it87_leave_mbpnp_mode();		/* Leave MBPnP mode */
return sio_addr_port;
}
/* Function to get data from it87 */
int get_it87(int reg1)
{
  int val1 = 0;
  int retval = 0;  
  int siobase = get_it87_addr_port();
  retval = port_access_multiple(siobase,2);
  if (retval==0)
  {
	port_out(siobase, reg1);
	val1 = port_in(siobase+1);	
    retval = port_deny_multiple(siobase,2);
    if (retval==0)
    {}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }
  return val1;
}
/* Function to set data on it87 */
void set_it87(int reg1,int val1)
{
  int retval = 0; 
  int siobase = get_it87_addr_port(); 
 
  retval = port_access_multiple(siobase,2);
  if (retval==0)
  {
	port_out(siobase, reg1);
	port_out((siobase+1), val1);	
    retval = port_deny_multiple(siobase,2);
    if (retval==0)
    {}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }
}
/* Function to set gpio led register on ICH6 for Watchguard Xe box*/
void setledXe(int val1)
{
  int retval = 0;  
 
  retval = port_access(gpiobase+0x0f);
  if (retval==0)
  {
	port_out((gpiobase+0x0f),val1);	
    retval = port_deny(gpiobase+0x0f);
    if (retval==0)
    {}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }
}
/* Function to set gpio flash register on ICH6 for Watchguard Xe box*/
void setflashXe(int val1)
{
  int retval = 0;  
 
  retval = port_access(gpiobase+0x1b);
  if (retval==0)
  {
	port_out((gpiobase+0x1b),val1);	
    retval = port_deny(gpiobase+0x1b);
    if (retval==0)
    {}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }
}
/* Function to set gpio led register on ICH5 for Watchguard X-Peak box*/
void setledXp(int val1)
{
  int retval = 0;  
 
  retval = port_access(gpiobase+0x39);
  if (retval==0)
  {
	port_out((gpiobase+0x39),val1);	
    retval = port_deny(gpiobase+0x39);
    if (retval==0)
    {}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }
}
/* Function to set gpio led register on ICH2 for Watchguard X-Core box*/
void setledXc(int val1, int val2)
{
  int retval = 0;  
 
  retval = port_access(gpiobase2+0x0f);
  if (retval==0)
  {
	port_out((gpiobase2+0x0f),val1);	
    retval = port_deny(gpiobase2+0x0f);
    if (retval==0)
    {}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }
  retval = port_access(gpiobase2+0x0e);
  if (retval==0)
  {
	port_out((gpiobase2+0x0e),val2);	
    retval = port_deny(gpiobase2+0x0e);
    if (retval==0)
    {}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }
}
/* Function to set the SDECLCD backlight*/
void setbacklight(int val1)
{
  int retval = 0;  
 
  retval = port_access(parallel_port+0x02);
  if (retval==0)
  {
	port_out((parallel_port+0x02),val1);	
    retval = port_deny(parallel_port+0x02);
    if (retval==0)
    {}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }
}
/* Function to get cpu temperature value*/
int getcputemp(void)
{
	int temp_hb = 0;
	int temp_lb = 0;
	setw83627(bank_select, 0x1);		/* Select bank 1 */
	temp_hb = getw83627(sensor2_hb);	/* Read Sensor 2 high byte register */
	temp_lb = getw83627(sensor2_lb);	/* Read Sensor 2 low byte register */
	setw83627(bank_select, 0x0);		/* Reset to bank 0 */
	return temp_hb;				/* Return only high byte, 1 degree resolution is fine */
}
/* Function to set the IT87 fan control at sane defaults*/
void default_XTM8_fan(void)
{
    if ((get_it87(0x61)) == 0x3c){		/* Has the fans been setup yet? */
    set_it87(0x63, 0x38);		/* Set CPU fans minimum PWM value */
    set_it87(0x6b, 0x38);		/* Set System fan minimum PWM value */
    set_it87(0x61, 0x28);		/* Set CPU start fan slope start temp 40C*/
    set_it87(0x69, 0x23);		/* Set System start fan slope start temp 35C*/
    set_it87(0x64, 0x08);		/* Set temp/PWM slope to 1 */
    set_it87(0x6c, 0x08);		/*  */
    set_it87(0x65, 0x62);		/* Set hysteresis to 2 */
    set_it87(0x6d, 0x62);		/*  */
    printf("IT87 Fanctl set to sane defaults\n");
    }
}
/* Function to set the configure the XTM8 LED GPIOs correctly*/
void config_XTM8_led(void)
{
    it87_enter_mbpnp_mode();
    set_it87_config(LDR, 0x07);             /* Set logical device register to 7 */
    if ((get_it87_config(0x27)) ==0)		/* Is GPIO3 still disabled? */
            {
			set_it87_config(0x27,0x03);	/* Set pins as GPIO, GP30 and GP31 */
			set_it87_config(0xba,0x03);	/* Enable internal pull-up */
			set_it87_config(0xca,0x03);	/* Set as output */
			set_it87_config(0xf8,0x18);	/* Set GPIO blink 1 to pin 19 */
			set_it87_config(0xf9,0x01);	/* Set GPIO blink 1 always low (bit 0) */
			set_it87_config(0xfa,0x19);	/* Set GPIO blink 2 to pin 18 */
			set_it87_config(0xfb,0x01);	/* Set GPIO blink 2 always low (bit 0) */			
            printf("IT87 GPIO pins configured\n");
            }
    it87_leave_mbpnp_mode();
}
 
/* Function to display correct program usage */
void usage(void)
{
printf("WGXepc Version 1.7 (Linux/Ubuntu port)\n");
printf("WGXepc can accept two arguments:\n");
printf(" -f (CPU fan) will return the current and minimum fan speed or if followed\n");
printf("    by a number in hex, 00-FF, will set it. Not supported on the XTM800/1500\n");
printf("    (firmware overrides the fan registers, see source comments).\n");
printf(" -f2 (System fan) will return the current and minimum fan speed or if followed\n");
printf("    by a number in hex, 00-FF, will set it. Not supported on the XTM800/1500.\n");
printf(" -l (led) will set the arm/disarm led state to the second argument:\n");
printf("    red, green, red_flash, green_flash, red_flash_fast, green_flash_fast, off\n");
printf(" -b (backlight) will set the lcd backlight to the second argument:\n");
printf("    on or off. Do not use with LCD driver.\n");
printf(" -t (temperature) shows the current CPU temperature reported by the\n");
printf("    SuperIO chip. X-e box only.\n");
printf(" -dump <bank> (diagnostic, read-only) dumps all W83627 hardware-monitor\n");
printf("    registers in the given bank (hex, e.g. 0, 1, 2, 3) for troubleshooting.\n");
printf(" -rpm (diagnostic, read-only) shows the measured fan speeds in RPM from the\n");
printf("    W83627 tachometer inputs (Bank 6).\n");
printf("Not all functions are supported by all models\n");
printf("Note: this program requires root privileges (run with sudo) because it\n");
printf("accesses raw I/O ports via ioperm().\n");
}
/* function to determine which Firebox model we are using */
/* 1=X-Core, 2=X-Peak, 3=X-E, 4=XTM5 */
int getmodel(void)
{
int retval = 0;
char product[64];  
/* Look for unique X-e registers */ 
  retval = port_access(0x480);
  if (retval==0)
  {
	if (port_in(0x480)==0x81)
		{
		printf("Found Firebox X-E\n");
		return 3;
		}
    retval = port_deny(0x480);
    if (retval==0){}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }
/* Look for unique X-peak registers */ 
  retval = port_access(0x482);
  if (retval==0)
  {
	if (port_in(0x482)==0xa0)
		{
		printf("Found Firebox X-Peak\n");
		return 2;
		}
    retval = port_deny(0x482);
    if (retval==0){}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }
/* Look for unique X-core registers */ 
  retval = port_access(0x4082);
  if (retval==0)
  {
	if (port_in(0x4082)==0x20)
		{
		printf("Found Firebox X-Core\n");
		return 1;
		}
    retval = port_deny(0x4082);
    if (retval==0){}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }
/* Look for unique XTM5 registers */ 
  retval = port_access(0x480);
  if (retval==0)
  {
	if (port_in(0x480)==0xc1)
		{
		printf("Found Firebox XTM5\n");
		return 4;
		}
    retval = port_deny(0x480);
    if (retval==0){}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }
/* Look for unique XTM8 registers */ 
  retval = port_access(0x480);
  if (retval==0)
  {
	if (port_in(0x480)==0xc3)
		{
		printf("Found Firebox XTM8\n");
		return 5;
		}
    retval = port_deny(0x480);
    if (retval==0){}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }
/* Look for unique M400/500 registers */ 
  retval = port_access(0x1c00);
  if (retval==0)
  {
	if (port_in(0x1c00)==0xc3)
		{
		printf("Found Firebox M400/500\n");
		return 6;
		}
    retval = port_deny(0x1c00);
    if (retval==0){}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }
/* Look for unique XTM800/1500 registers */ 
  retval = port_access(0x500);
  if (retval==0)
  {
	if (port_in(0x500)==0xc7)
		{
		printf("Found Firebox XTM800/1500\n");
		return 7;
		}
    retval = port_deny(0x500);
    if (retval==0){}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }
/* Look for M370/470/570/670 product ID (Linux: /sys/class/dmi/id/product_name) */ 
  if (get_dmi_string("/sys/class/dmi/id/product_name", product, sizeof(product)) > 0) 
     {
        //printf("Product is %s\n",product);
        if (strcmp(product,"NCB-4210WG")==0)
            {
            printf("Found Firebox M370/470/570/670.\n");
            return 8;
            }           
     }
/* Look for unique M270 registers */ 
  retval = port_access_multiple(0x80,8);
  if (retval==0)
  {
	if ((port_in(0x80)==0xaa)&&(port_in(0x84)==0x00)&&(port_in(0x88)==0x00))
		{
		printf("Found Firebox M270\n");
		return 9;
		}
    retval = port_deny_multiple(0x80,8);
    if (retval==0){}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }
/* Look for T70 product ID (Linux: /sys/class/dmi/id/board_name) */ 
  if (get_dmi_string("/sys/class/dmi/id/board_name", product, sizeof(product)) > 0) 
     {
        //printf("Product is %s\n",product);
        if (strcmp(product,"T70")==0)
            {
            printf("Found Firebox T70.\n");
            return 10;
            }           
     }
/* Check for VBox ;) */
  if (get_dmi_string("/sys/class/dmi/id/product_name", product, sizeof(product)) > 0) 
     {
        //printf("Product is %s\n",product);
        if (strcmp(product,"VirtualBox")==0)
            {
            printf("Found VirtualBox.\n");
            return 42;
            }           
     }
  
return 0;
}
/*
main function 
sets PWM and GPIO registers on SuperIO chip and ICH chip.
*/
int main(int argc, char *argv[])
{
  int fanspeed = 0x00;		/* Current fan speed reading */
  int fanspeed_min = 0x00;	/* Minimum/target fan speed, read or set from argv */
  int cpu_temp = 0x00;		/* CPU temperature reading */
  int system_temp = 0x00;	/* System temperature reading */
  int reg_test = 0x00;		/* Scratch register value (unused, kept from original) */
  int model;			/* Detected Firebox model, see getmodel() return values */

  if (geteuid() != 0) {
	fprintf(stderr, "This program needs to run as root (it uses ioperm() for raw I/O port access).\n");
	fprintf(stderr, "Try: sudo %s ...\n", argv[0]);
	return 1;
  }
  
  model = getmodel();
  
if (argc ==1) /*Check that correct number of arguments have been given */
	{usage();
	return 0;
	}

/* -rpm : read-only ground-truth check of the actual measured fan speeds
   (Bank 6, index 0x50-0x59), independent of whatever a fan output/PWM
   register claims. Used to determine whether writes to output registers
   like 0x03 are having any physical effect at all, regardless of what the
   register itself reads back afterwards. */
if (strcmp(argv[1],"-rpm")==0)
	{
	dump_w83627_fan_rpms();
	return 0;
	}

/* -dump <bank_hex> : read-only diagnostic dump of a W83627 hardware-monitor
   bank, used to identify undocumented registers (e.g. real fan control
   registers on chip variants we don't have a datasheet for). Available
   regardless of detected model since it only touches the W83627 chip
   directly. */
if (strcmp(argv[1],"-dump")==0)
	{
	if (argc < 3)
		{
		printf("Usage: %s -dump <bank in hex, e.g. 0, 1, 2, 3>\n", argv[0]);
		return 0;
		}
	int bank = strtol(argv[2],NULL,16);
	dump_w83627_bank(bank);
	return 0;
	}

switch (model)		/* Dispatch on detected Firebox model; each case implements
			   the -b/-f/-f2/-l/-t arguments for that model's hardware */
{
case 10:	// T70
	
/* Handle "-b" (backlight) argument */
if (strcmp(argv[1],"-b")==0) 
    {
	printf("There is no LCD on the T70\n");
	return 0;	
	}
else	
/* Handle "-f" (CPU fan) argument */
if (strcmp(argv[1],"-f")==0) 
    {
	printf("There is no fan on the T70\n");
	return 0;	
	}
else	
/* Handle "-l" (LED) argument */
if (strcmp(argv[1],"-l")==0) 
    {
	printf("There is no LED control on the T70 yet\n");
	return 0;	
	}
else
    {
	usage();
	}
	
break;
case 9:		// M270
	
/* Handle "-b" (backlight) argument */
if (strcmp(argv[1],"-b")==0) 
    {
	printf("The M270 has no LCD\n");
	return 0;	
	}
else	
if (strcmp(argv[1],"-f")==0) /* The fan uses the CPUfan output on Bank2 */
    {
	if (argc==2)  /* Are we looking for the fan speed or setting it */
	{
    setw83627(bank_select,0x02); /* Set Bank2 */
	fanspeed = getw83627(0x09);	/* Current system fan speed register */
	fanspeed_min = getw83627(0x27); /* T1 fan speed */
	printf("Current fanspeed is %x, minimum fanspeed is %x\n",fanspeed ,fanspeed_min);
	}
	else
	{
    setw83627(bank_select,0x02); /* Set Bank2 */
	setw83627(0x21,0x1e);        /* Set T1 point at 30C */
	setw83627(0x22,0x2d);        /* Set T2 point at 45C */
	fanspeed_min = strtol(argv[2],NULL,16); /*Convert argument to hex integer*/
	setw83627(0x27,fanspeed_min);
	printf("Minimum fanspeed set to %x at 30°C or less\n",fanspeed_min);
	} 
	}
else	
/* Handle "-l" (LED) argument */
if (strcmp(argv[1],"-l")==0)
	{
			if (argc ==2) /*Check that correct number of arguments have been given */
			{
			usage();
			return 0;
			}
                                        /* The NCT6776F in the M270 is compatible with the W83627 */
			w83627_enter_ext_mode();	/* Enter extended function mode */
			set_w83627_config(LDR,0x07);		/* Set logial device register to device 7, the LED is on GPIO7 bits 2 and 3*/
			
		if (strcmp(argv[2],"red")==0)
			{
			set_w83627_config(0xe1,0xf9);		/* Set bit 3 high */
			}
	else	if (strcmp(argv[2],"green")==0)
			{
			set_w83627_config(0xe1,0xf5);		/* Set bit 2 high */
			}
	else	if (strcmp(argv[2],"off")==0)
			{
			set_w83627_config(0xe1,0xf1);		/* Set bits 2 and 3 low */			
			}
	else	if (strcmp(argv[2],"red_flash")==0)
			{
			printf("Flashing is not available on the M270\n");
			}
	else	if (strcmp(argv[2],"green_flash")==0)
			{
			printf("Flashing is not available on the M270\n");	
			}
	else	if (strcmp(argv[2],"red_flash_fast")==0)
			{
			printf("Flashing is not available on the M270\n");
			}
	else	if (strcmp(argv[2],"green_flash_fast")==0)
			{
			printf("Flashing is not available on the M270\n");	
			}	
	else{
	usage();
	}
	w83627_leave_ext_mode();
	}
	else{
	usage();
	}
	
	break;
case 8:		// M370/470/570/670
	
/* Handle "-b" (backlight) argument */
if (strcmp(argv[1],"-b")==0) 
    {
	printf("The M370/470/570/670 has no LCD\n");
	return 0;	
	}
else	
if (strcmp(argv[1],"-f")==0) /* All three fans use the CPUfan output on Bank2 */
    {
	if (argc==2)  /* Are we looking for the fan speed or setting it */
	{
    setw83627(bank_select,0x02); /* Set Bank2 */
	fanspeed = getw83627(0x09);	/* Current system fan speed register */
	fanspeed_min = getw83627(0x27); /* T1 fan speed */
	printf("Current fanspeed is %x, minimum fanspeed is %x\n",fanspeed ,fanspeed_min);
	}
	else
	{
    setw83627(bank_select,0x02); /* Set Bank2 */
	setw83627(0x21,0x1e);        /* Set T1 point at 30C */
	fanspeed_min = strtol(argv[2],NULL,16); /*Convert argument to hex integer*/
	setw83627(0x27,fanspeed_min);
	printf("Minimum fanspeed set to %x at 30°C or less\n",fanspeed_min);
	} 
	}
else	
/* Handle "-l" (LED) argument */
if (strcmp(argv[1],"-l")==0)
	{
			if (argc ==2) /*Check that correct number of arguments have been given */
			{
			usage();
			return 0;
			}
                                        /* The NCT6776F in the M370/470/570/670 is compatible with the W83627 */
			w83627_enter_ext_mode();	/* Enter extended function mode */
			set_w83627_config(LDR,0x07);		/* Set logial device register to device 7, the LED is on GPIO7 bits 2 and 3*/
			
		if (strcmp(argv[2],"red")==0)
			{
			set_w83627_config(0xe1,0xfb);		/* Set bit 3 high */
			}
	else	if (strcmp(argv[2],"green")==0)
			{
			set_w83627_config(0xe1,0xf7);		/* Set bit 2 high */
			}
	else	if (strcmp(argv[2],"off")==0)
			{
			set_w83627_config(0xe1,0xf3);		/* Set bits 2 and 3 low */			
			}
	else	if (strcmp(argv[2],"red_flash")==0)
			{
			printf("Flashing is not available on the M370/470/570/670\n");
			}
	else	if (strcmp(argv[2],"green_flash")==0)
			{
			printf("Flashing is not available on the M370/470/570/670\n");	
			}
	else	if (strcmp(argv[2],"red_flash_fast")==0)
			{
			printf("Flashing is not available on the M370/470/570/670\n");
			}
	else	if (strcmp(argv[2],"green_flash_fast")==0)
			{
			printf("Flashing is not available on the M370/470/570/670\n");	
			}	
	else{
	usage();
	}
	w83627_leave_ext_mode();
	}
	else{
	usage();
	}
	
	break;
case 7:		// XTM800/1500
	
/* Handle "-b" (backlight) argument */
if (strcmp(argv[1],"-b")==0) 
    {
	if (argc==2)  /* Can only set the backlight */
	{
	usage();
	return 0;	
	}
	if (strcmp(argv[2],"on")==0)
		{
		setbacklight(0x00);   /* Strobe bit on control port is inverted */
		}
	else	if (strcmp(argv[2],"off")==0)
		{
		setbacklight(0x01);
		}
	else 	{
		usage();
		return 0;
		}
	}
else	
/* Handle "-f" (CPU fan) argument */
if (strcmp(argv[1],"-f")==0) 
    {
	/* Investigated in depth via the official Nuvoton W83627DHG-P
	   datasheet and live hardware testing (see -dump/-rpm below):
	   - Index 0x03 (Bank 0) is genuinely "CPUFANOUT0 Output Value
	     Select", and register 0x04 genuinely controls its mode
	     (Manual/Thermal Cruise/Fan Speed Cruise/SMART FAN III) - this
	     part matches the datasheet exactly and was confirmed to work:
	     switching modes via 0x04 reliably takes effect.
	   - However, writes to the actual output value register 0x03 do
	     NOT take effect on this unit, even in confirmed Manual Mode,
	     even read back within the same process immediately after
	     writing. A "-rpm" ground-truth tachometer check (Bank 6)
	     confirms the physical fan speed never changes either.
	   - Register 0x04 itself was later observed to revert on its own,
	     back to its original value, without this tool doing so. That
	     points to a BIOS/SMI background routine continuously
	     reasserting these registers, which explains why manually
	     written values never hold - not a bug in this code, but active
	     firmware-level enforcement outside what userspace can override.
	   Rather than keep guessing at undocumented behavior on production
	   hardware, this is left unimplemented, same as upstream. Use
	   "-dump 0" and "-rpm" to inspect the raw registers and measured
	   speeds directly if you want to investigate further; the BIOS fan
	   settings are a more promising place to look for actual control. */
	printf("CPU fan control on the XTM800/1500 was investigated but writes to the\n");
	printf("output register do not affect the physical fan speed on this unit -\n");
	printf("see the source comments for details. Use -dump/-rpm to inspect further.\n");
	return 0;	
	}
else	
/* Handle "-f2" (system fan) argument */
if (strcmp(argv[1],"-f2")==0) 
    {
	/* Same underlying issue as -f: writes to the output value register
	   (0x01, "SYSFANOUT Output Value Select") do not take effect,
	   confirmed both by immediate readback within the same process and
	   by ground-truth tachometer measurements ("-rpm", SYSFANIN) never
	   changing. SYSFANOUT started in a different mode than CPUFANOUT0
	   (Manual rather than SMART FAN III) so this was tested separately
	   rather than assumed - the result was identical.
	   Additional finding: register 0x04 (FAN Configuration Register I)
	   was observed to revert on its own from a value set by an earlier
	   test back to its original default, without any command from this
	   tool doing so. That points to something outside userspace - most
	   likely a BIOS/SMI background routine - continuously reasserting
	   these registers, which would also explain why the output value
	   registers never hold a manually written value either. This isn't
	   something to keep probing at from here; see -dump/-rpm to inspect
	   further, or look at BIOS fan settings for actual control. */
	printf("System fan control on the XTM800/1500 was investigated but writes to the\n");
	printf("output register do not affect the physical fan speed on this unit -\n");
	printf("see the source comments for details. Use -dump/-rpm to inspect further.\n");
	return 0;	
	}
else	
/* Handle "-l" (LED) argument */
if (strcmp(argv[1],"-l")==0) 
    {
	printf("There is no LED control on the XTM800/1500 yet\n");
	return 0;	
	}
else
    {
	usage();
	}
	
break;
case 6:		// M400/500
	
/* Handle "-b" (backlight) argument */
if (strcmp(argv[1],"-b")==0) 
    {
	printf("The M400/500 has no LCD\n");
	return 0;	
	}
else	
if (strcmp(argv[1],"-f")==0) /* Both fans use the CPU fan output on Bank2 */
    {
	if (argc==2)  /* Are we looking for the fan speed or setting it */
	{
    setw83627(bank_select,0x02); /* Set Bank2 */
	fanspeed = getw83627(0x09);	/* Current system fan speed register */
	fanspeed_min = getw83627(0x27); /* T1 fan speed */
	printf("Current fanspeed is %x, minimum fanspeed is %x\n",fanspeed ,fanspeed_min);
	}
	else
	{
    setw83627(bank_select,0x02); /* Set Bank2 */
	setw83627(0x21,0x2d);        /* Set T1 point at 45C */
	setw83627(0x22,0x37);        /* Set T2 point at 55C */
	setw83627(0x28,0x3c);        /* Set T2 fan speed to 0x3c */
	fanspeed_min = strtol(argv[2],NULL,16); /*Convert argument to hex integer*/
	setw83627(0x27,fanspeed_min);
	printf("Minimum fanspeed set to %x at 45°C or less\n",fanspeed_min);
	} 
	}
else	
/* Handle "-l" (LED) argument */
if (strcmp(argv[1],"-l")==0)
	{
			if (argc ==2) /*Check that correct number of arguments have been given */
			{
			usage();
			return 0;
			}
                                        /* The NCT6776F in the M400/500 is compatible with the W83627 */
			w83627_enter_ext_mode();	/* Enter extended function mode */
			set_w83627_config(LDR,0x07);		/* Set logial device register to device 7, the LED is on GPIO7 bits 2 and 3*/
			
		if (strcmp(argv[2],"red")==0)
			{
			set_w83627_config(0xe1,0x3b);		/* Set bit 3 high */
			}
	else	if (strcmp(argv[2],"green")==0)
			{
			set_w83627_config(0xe1,0x37);		/* Set bit 2 high */
			}
	else	if (strcmp(argv[2],"off")==0)
			{
			set_w83627_config(0xe1,0x33);		/* Set bits 2 and 3 low */			
			}
	else	if (strcmp(argv[2],"red_flash")==0)
			{
			printf("Flashing is not available on the M400/500\n");
			}
	else	if (strcmp(argv[2],"green_flash")==0)
			{
			printf("Flashing is not available on the M400/500\n");	
			}
	else	if (strcmp(argv[2],"red_flash_fast")==0)
			{
			printf("Flashing is not available on the M400/500\n");
			}
	else	if (strcmp(argv[2],"green_flash_fast")==0)
			{
			printf("Flashing is not available on the M400/500\n");	
			}	
	else{
	usage();
	}
	w83627_leave_ext_mode();
	}
	else{
	usage();
	}
	
	break;
case 5:		// XTM8
	
/* Handle "-b" (backlight) argument */
if (strcmp(argv[1],"-b")==0) 
    {
	if (argc==2)  /* Can only set the backlight */
	{
	usage();
	return 0;	
	}
	if (strcmp(argv[2],"on")==0)
		{
		setbacklight(0x00);   /* Strobe bit on control port is inverted */
		}
	else	if (strcmp(argv[2],"off")==0)
		{
		setbacklight(0x01);
		}
	else 	{
		usage();
		return 0;
		}
	}
else
/* Handle "-f" (CPU fan) argument */
if (strcmp(argv[1],"-f")==0) 
    {
	if (argc==2)  /* Are we looking for the fan speed or setting it */
	{
	//fanspeed = get_it87(0x01);	/* Current system fan speed register */
	fanspeed_min = get_it87(0x63); /* Minimum FANCTL_1 speed in SmartGuardian mode */
	printf("Current CPU fanspeed is ???, minimum is %x\n",fanspeed_min);
	}
	else
	{
    default_XTM8_fan();
	//fanspeed = get_it87(0x01);
	fanspeed_min = strtol(argv[2],NULL,16); /*Convert argument to hex integer*/
	set_it87(0x063,fanspeed_min);  /* Minimum FANCTL_1 speed in SmartGuardian mode */
	printf("Minimum CPU fanspeed set to %x\n",fanspeed_min);
	} 				
	}
else	
/* Handle "-f2" (system fan) argument */
if (strcmp(argv[1],"-f2")==0) 
    {
	if (argc==2)  /* Are we looking for the fan speed or setting it */
	{
	//fanspeed = get_it87(0x01);	/* Current system fan speed register */
	fanspeed_min = get_it87(0x6b); /* Minimum FANCTL_2 speed in SmartGuardian mode */
	printf("Current System fanspeed is ???, minimum is %x\n",fanspeed_min);
	}
	else
	{
    default_XTM8_fan();
	//fanspeed = get_it87(0x01);
	fanspeed_min = strtol(argv[2],NULL,16); /*Convert argument to hex integer*/
	set_it87(0x06b,fanspeed_min);  /* Minimum FANCTL_2 speed in SmartGuardian mode */
	printf("Minimum System fanspeed set to %x\n",fanspeed_min);
	} 				
	}
else	
/* Handle "-l" (LED) argument */
if (strcmp(argv[1],"-l")==0)
	{
			if (argc ==2) /*Check that correct number of arguments have been given */
			{
			usage();
			return 0;
			}
            config_XTM8_led(); /* Check and set the GPIO pins for LED operation */
            it87_enter_mbpnp_mode();
    		if (strcmp(argv[2],"red")==0)
			{
			set_it87_config(0xb2,0x01);	/* Invert Red, high */
			set_it87_config(0xf9,0x01);	/* Red blink low */
			set_it87_config(0xfb,0x01);	/* Green blink low */	
			}
	else	if (strcmp(argv[2],"green")==0)
			{
			set_it87_config(0xb2,0x02);	/* Invert Green, high */
			set_it87_config(0xf9,0x01);	/* Red blink low */
			set_it87_config(0xfb,0x01);	/* Green blink low */
			}
	else	if (strcmp(argv[2],"off")==0)
			{
			set_it87_config(0xb2,0x00);	/* None inverted */
			set_it87_config(0xf9,0x01);	/* Red blink low */
			set_it87_config(0xfb,0x01);	/* Green blink low */			
			}
	else	if (strcmp(argv[2],"red_flash")==0)
			{
			set_it87_config(0xb2,0x01);	/* Invert Red, high */
			set_it87_config(0xf9,0x01);	/* Red blink low */
			set_it87_config(0xfb,0x02);	/* Green blink 1Hz */
			}
	else	if (strcmp(argv[2],"green_flash")==0)
			{
			set_it87_config(0xb2,0x02);	/* Invert Green, high */
			set_it87_config(0xf9,0x02);	/* Red blink 1Hz */
			set_it87_config(0xfb,0x01);	/* Green blink low */
			}
	else	if (strcmp(argv[2],"red_flash_fast")==0)
			{
			set_it87_config(0xb2,0x01);	/* Invert Red, high */
			set_it87_config(0xf9,0x01);	/* Red blink low */
			set_it87_config(0xfb,0x00);	/* Green blink 4Hz */
			}
	else	if (strcmp(argv[2],"green_flash_fast")==0)
			{
			set_it87_config(0xb2,0x02);	/* Invert Green, high */
			set_it87_config(0xf9,0x00);	/* Red blink 4Hz */
			set_it87_config(0xfb,0x01);	/* Green blink low */	
			}	
	else{
	usage();
	}
	it87_leave_mbpnp_mode();
	}
else
/* Handle "-t" (temperature) argument */
if (strcmp(argv[1],"-t")==0) 
   {
	cpu_temp = get_it87(0x29);
	system_temp = get_it87(0x2a);
	printf("CPU temp:%d, System temp:%d  \n",cpu_temp, system_temp);
	}
else{
	usage();
	}
break;
case 4:		// XTM5
	
/* Handle "-b" (backlight) argument */
if (strcmp(argv[1],"-b")==0) 
    {
	if (argc==2)  /* Can only set the backlight */
	{
	usage();
	return 0;	
	}
	if (strcmp(argv[2],"on")==0)
		{
		setbacklight(0x00);   /* Strobe bit on control port is inverted */
		}
	else	if (strcmp(argv[2],"off")==0)
		{
		setbacklight(0x01);
		}
	else 	{
		usage();
		return 0;
		}
	}
else
/* Handle "-f" (CPU fan) argument */
if (strcmp(argv[1],"-f")==0) 
    {
	printf("The CPU fans are not yet controllable on the XTM5\n");
	return 0;	
	}
else	
/* Handle "-f2" (system fan) argument */
if (strcmp(argv[1],"-f2")==0) 
    {
	if (argc==2)  /* Are we looking for the fan speed or setting it */
	{
	fanspeed = getw83627(0x01);	/* Current system fan speed register */
	fanspeed_min = getw83627(0x08); /* Minimum syetem fan speed in Thermal Cruise mode */
	printf("Current fanspeed is %x, minimum fanspeed is %x\n",fanspeed ,fanspeed_min);
	}
	else
	{
	fanspeed = getw83627(0x01);
	fanspeed_min = strtol(argv[2],NULL,16); /*Convert argument to hex integer*/
	setw83627(0x08,fanspeed_min);
	printf("Minimum fanspeed set to %x\n",fanspeed_min);
	if (fanspeed_min>fanspeed)	/* To increase the fan speed to the new setting we */
		{			/* have to restart Thermal Cruise mode */
		printf("Restarting Thermal Cruise mode\n");			
		setw83627(0x04,0x08);	/* Set the sytem fan mode to fan cruise */
		//setw83627(0x01,fanspeed);	/* Keep current speed    */
		setw83627(0x04,0x04);	/* Set is back to Thermal Cruise    */
		}			/* Reg 04h bits 3-2 control the system fan mode */
	} 				/* 10 is fan, 01 is Thermal Cruise */
	//get_w83627_addr_port();
	}
else	
/* Handle "-l" (LED) argument */
if (strcmp(argv[1],"-l")==0)
	{
			if (argc ==2) /*Check that correct number of arguments have been given */
			{
			usage();
			return 0;
			}
			w83627_enter_ext_mode();	/* Enter extended functio mode */
			set_w83627_config(0x07,0x08);		/* Set logial device register to device 8 (GPIO2) */
			if ((get_w83627_config(0x30)) ==0){		/* Is SuperIO chip GPIO2 enabled? */
			printf("Enabling GPIO2\n");
			set_w83627_config(0x30,0x01);
			}
			if ((get_w83627_config(0xf0)) ==0xff){		/* Is GPIO2 set as output? */
			printf("Setting GPIO2 pins as output\n");
			set_w83627_config(0xf0,0xcf);			/* 1100 1111, 0xcf */
			}
		if (strcmp(argv[2],"red")==0)
			{
			set_w83627_config(0xf1,0x20);		/* Set bit 5 high */
			}
	else	if (strcmp(argv[2],"green")==0)
			{
			set_w83627_config(0xf1,0x10);		/* Set bit 4 high */
			}
	else	if (strcmp(argv[2],"off")==0)
			{
			set_w83627_config(0xf1,0x00);		/* Set bits 4 and 5 low */			
			}
	else	if (strcmp(argv[2],"red_flash")==0)
			{
			printf("Flashing is not available on the XTM5\n");
			}
	else	if (strcmp(argv[2],"green_flash")==0)
			{
			printf("Flashing is not available on the XTM5\n");	
			}
	else	if (strcmp(argv[2],"red_flash_fast")==0)
			{
			printf("Flashing is not available on the XTM5\n");
			}
	else	if (strcmp(argv[2],"green_flash_fast")==0)
			{
			printf("Flashing is not available on the XTM5\n");	
			}	
	else{
	usage();
	}
	w83627_leave_ext_mode();
	}
	else{
	usage();
	}
	
	break;
case 3:		// X-e
	
/* Handle "-b" (backlight) argument */
if (strcmp(argv[1],"-b")==0) 
    {
	if (argc==2)  /* Can only set the backlight */
	{
	usage();
	return 0;	
	}
	if (strcmp(argv[2],"on")==0)
		{
		setbacklight(0x00);   /* Strobe bit on control port is inverted */
		}
	else	if (strcmp(argv[2],"off")==0)
		{
		setbacklight(0x01);
		}
	else 	{
		usage();
		return 0;
		}
	}
else	
/* Handle "-f" (CPU fan) argument */
if (strcmp(argv[1],"-f")==0) 
    {
	check_siobase();
	if (argc==2)  /* Are we looking for the fan speed or setting it */
	{
	fanspeed = getw83627(0x5a);
	printf("Fanspeed is %x\n",fanspeed);
	}
	else
	{
	fanspeed = strtol(argv[2],NULL,16); /*Convert argument to hex integer*/
	setw83627(0x5a,fanspeed);
	printf("Fanspeed set to %x\n",fanspeed);
	}
	}
else	
/* Handle "-l" (LED) argument */
if (strcmp(argv[1],"-l")==0)
	{
			if (argc ==2) /* No color argument given */
			{
			usage();
			return 0;
			}
			if (strcmp(argv[2],"red")==0)
			{
			setledXe(0x0b);		/* Solid red LED */
			setflashXe(0x00);	/* No flashing */
			}
	else	if (strcmp(argv[2],"green")==0)
			{
			setledXe(0x13);		/* Solid green LED */
			setflashXe(0x00);	/* No flashing */
			}
	else	if (strcmp(argv[2],"red_flash")==0)
			{
			setledXe(0x0b);		/* Red LED */
			setflashXe(0x08);	/* Enable flashing */
			}
	else	if (strcmp(argv[2],"green_flash")==0)
			{
			setledXe(0x13);		/* Green LED */
			setflashXe(0x10);	/* Enable flashing */		
			}
	else	if (strcmp(argv[2],"red_flash_fast")==0)
			{
			printf("Fast flashing is not available on the X-e\n");
			}
	else	if (strcmp(argv[2],"green_flash_fast")==0)
			{
			printf("Fast flashing is not available on the X-e\n");	
			}
	else	if (strcmp(argv[2],"off")==0)
			{
			setledXe(0x03);		/* LED off */
			setflashXe(0x00);	/* No flashing */
			}
	else{
	usage();
	}
	}
else
/* Handle "-t" (temperature) argument */
if (strcmp(argv[1],"-t")==0) 
   {
	check_siobase();
	cpu_temp = getcputemp();
	printf("SuperIO sensor 2 reads:\n%d \n",cpu_temp);
	}
	else{
	usage();
	}
	break;
	
case 2:		// X-Peak
	
/* Handle "-b" (backlight) argument */
if (strcmp(argv[1],"-b")==0) 
    {
	if (argc==2)  /* Can only set the backlight */
	{
	usage();
	return 0;	
	}
	if (strcmp(argv[2],"on")==0)
		{
		setbacklight(0x00);   /* Strobe bit on control port is inverted */
		}
	else	if (strcmp(argv[2],"off")==0)
		{
		setbacklight(0x01);
		}
	else 	{
		usage();
		return 0;
		}
	}
else	
/* Handle "-f" (CPU fan) argument */
if (strcmp(argv[1],"-f")==0) 
    {
	printf("Fanspeed is not available on the X-Peak\n");
	}
else	
/* Handle "-l" (LED) argument */
if (strcmp(argv[1],"-l")==0)
	{
			if (argc ==2) /* No color argument given */
			{
			usage();
			return 0;
			}
			if (strcmp(argv[2],"red")==0)
			{
			setledXp(0x0d);		/* Solid red LED */
			}
	else	if (strcmp(argv[2],"green")==0)
			{
			setledXp(0x0e);		/* Solid green LED */
			}
	else	if (strcmp(argv[2],"red_flash")==0)
			{
			printf("Flashing is not available on the X-Peak\n");
			}
	else	if (strcmp(argv[2],"green_flash")==0)
			{
			printf("Flashing is not available on the X-Peak\n");	
			}
	else	if (strcmp(argv[2],"red_flash_fast")==0)
			{
			printf("Flashing is not available on the X-Peak\n");
			}
	else	if (strcmp(argv[2],"green_flash_fast")==0)
			{
			printf("Flashing is not available on the X-Peak\n");	
			}	
	else	if (strcmp(argv[2],"off")==0)
			{
			setledXp(0x0c);		/* LED off */
			}
	else{
	usage();
	}
	}
	else{
	usage();
	}
	break;
	
case 1:		// X-Core
	
/* Handle "-b" (backlight) argument */
if (strcmp(argv[1],"-b")==0) 
    {
	if (argc==2)  /* Can only set the backlight */
	{
	usage();
	return 0;	
	}
	if (strcmp(argv[2],"on")==0)
		{
		setbacklight(0x00);   /* Strobe bit on control port is inverted */
		}
	else	if (strcmp(argv[2],"off")==0)
		{
		setbacklight(0x01);
		}
	else 	{
		usage();
		return 0;
		}
	}
else	
/* Handle "-f" (CPU fan) argument */
if (strcmp(argv[1],"-f")==0) 
    {
	printf("Fanspeed is not available on the X-Core\n");
	}
else	
/* Handle "-l" (LED) argument */
if (strcmp(argv[1],"-l")==0)
	{
			if (argc ==2) /* No color argument given */
			{
			usage();
			return 0;
			}
			if (strcmp(argv[2],"red")==0)
			{
			setledXc(0x09, 0xbf);		/* Solid red, no flash */
			}
	else	if (strcmp(argv[2],"green")==0)
			{
			setledXc(0x19, 0xbf);		/* Solid green, no flash */
			}
	else	if (strcmp(argv[2],"red_flash")==0)
			{
			setledXc(0x09, 0x3f);		/* Red, slow flash */
			}
	else	if (strcmp(argv[2],"green_flash")==0)
			{
			setledXc(0x19, 0x3f);		/* Green, slow flash */
			}
	else	if (strcmp(argv[2],"red_flash_fast")==0)
			{
			setledXc(0x01, 0xbf);		/* Red, fast flash */
			}
	else	if (strcmp(argv[2],"green_flash_fast")==0)
			{
			setledXc(0x11, 0xbf);		/* Green, fast flash */
			}	
	else	if (strcmp(argv[2],"off")==0)
			{
			setledXc(0x01, 0x3f);		/* LED off */
			}
	else{
	usage();
	}
	}
	else{
	usage();
	}
	break;
	
case 0:
	printf("Firebox not detected.\n");
	printf("If this is a Firebox it's either one we don't know about or it's running a bios we haven't seen.\n");
	printf("Hit the pfSense forums to let us know more.\n");
	break;
}
				
return 0;
}
