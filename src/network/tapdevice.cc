#include "tapdevice.hh"

int tun_alloc(char *dev, int flags) {
  struct ifreq ifr;
  int fd;
  int err;
  char *clone_dev = "/dev/net/tun";
  
  /* open the clone device */
  if( (fd = open(clone_dev, O_RDWR)) < 0 ) {
  	return fd;
  }
  
  /* preparation of the struct ifr, of type "struct ifreq" */
  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = flags;   /* IFF_TUN or IFF_TAP, plus maybe IFF_NO_PI */
  
  /* if a device name was specified, put it in the structure */
  if (*dev) {
  	strncpy(ifr.ifr_name, dev, IFNAMSIZ);
  }
  
  /* try to create the device */
  if( (err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0 ) {
  	close(fd);
  	return err;
  }
  
  /* if the operation was successful, write back the name of the
   * interface to the variable "dev", so the caller can know
   * it. Note that the caller MUST reserve space in *dev */
  strcpy(dev, ifr.ifr_name);
  
  /* return file descriptor to tap device */
  return fd;
}

int setup_tap( void ) {
  char tap_name[ IFNAMSIZ ];
  strcpy( tap_name, "tap0" );
  int tap_fd = tun_alloc( tap_name, IFF_TAP | IFF_NO_PI );
  if ( tap_fd < 0 ) {
    perror( "Allocating interface" );
    exit( 1 );
  }
  return tap_fd;
}
