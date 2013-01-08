#include <unistd.h>
#include <string>
#include <assert.h>
#include <list>

#include "partial-packet.pb.h"
#include "sproutconn.h"
#include "select.h"
#include <sys/time.h>
#include "tapdevice.hh"

using namespace std;
using namespace Network;
using namespace packet;

int main( int argc, char *argv[] )
{
  char *ip;
  int port;

  Network::SproutConnection *net;

  bool server = true;

  /* Connect to tap0 and setup Sprout tunnel */
  int tap_fd = setup_tap();

  /* Queue incoming packets from tap0 */
  std::queue<string> ingress_queue;

  if ( argc > 1 ) {
    /* client */

    server = false;

    ip = argv[ 1 ];
    port = atoi( argv[ 2 ] );

    net = new Network::SproutConnection( "4h/Td1v//4jkYhqhLGgegw", ip, port );
  } else {
    net = new Network::SproutConnection( NULL, NULL );
  }

  fprintf( stderr, "Port bound is %d\n", net->port() );

  Select &sel = Select::get_instance();
  sel.add_fd( net->fd() );
  sel.add_fd( tap_fd );

  const int fallback_interval = 50;

  /* wait to get attached */
  if ( server ) {
    while ( 1 ) {
      int active_fds = sel.select( -1 );
      if ( active_fds < 0 ) {
	perror( "select" );
	exit( 1 );
      }

      if ( sel.read( net->fd() ) ) {
	net->recv();
      }

      if ( net->get_has_remote_addr() ) {
	break;
      }
    }
  }

  uint64_t time_of_next_transmission = timestamp() + fallback_interval;

  fprintf( stderr, "Looping...\n" );  
  unsigned long int cumulative_bytes = 0;

  std::string limbo;
  /* loop */
  while ( 1 ) {
    int bytes_to_send = net->window_size();

    /* actually send, maybe */
    if ( ( bytes_to_send > 0 ) || ( time_of_next_transmission <= timestamp() ) ) {
      do {
	if ( !ingress_queue.empty() ) {
	  if (bytes_to_send == 0) {
		net->send( string(0,'x'), fallback_interval );
		break;
	  }
	  string packet( ingress_queue.front() );
	  if ( bytes_to_send < packet.size() ) {
		packet::PartialPacket par_pkt;
		par_pkt.set_payload( packet.substr( 0, bytes_to_send ) );
		par_pkt.set_bytes_to_follow( packet.size() - bytes_to_send );
		net->send( par_pkt.SerializeAsString(), fallback_interval );
		ingress_queue.front() = packet.substr( bytes_to_send, packet.size() - bytes_to_send );
		fprintf( stderr, "SENDING snipped packet of size %d, leftover %lu \n", bytes_to_send, ingress_queue.front().size() );
		break;
	  } else {
	  	bytes_to_send -= packet.size();
	  }
	  assert( bytes_to_send  >= 0 );
	  int time_to_next = 0;
	  if ( bytes_to_send == 0 || ingress_queue.empty() ) {
	  	time_to_next = fallback_interval;
	  }
	  packet::PartialPacket par_pkt;
	  par_pkt.set_payload( packet );
	  par_pkt.set_bytes_to_follow( 0 );
	  net->send( par_pkt.SerializeAsString(), time_to_next );
	  fprintf( stderr, "SENDING whole packet is size %lu \n", packet.size() );
	  ingress_queue.pop();
        }
        else {
	  net->send( string(0,'x'), fallback_interval );
	  bytes_to_send = 0; /* Wasted forecast */
        }
      } while ( bytes_to_send > 0 );

      time_of_next_transmission = std::max( timestamp() + fallback_interval,
					    time_of_next_transmission );
    }

    /* wait */
    int wait_time = time_of_next_transmission - timestamp();
    if ( wait_time < 0 ) {
      wait_time = 0;
    } else if ( wait_time > 10 ) {
      wait_time = 10;
    }

    int active_fds = sel.select( wait_time );
    if ( active_fds < 0 ) {
      perror( "select" );
      exit( 1 );
    }

    /* receive */
    struct timeval tv;
    if ( sel.read( net->fd() ) ) {
      string packet( net->recv() );
      if (packet.size() == 0 ) {
          continue;
      }

      packet::PartialPacket rx;
      rx.ParseFromString( packet );

      if ( rx.bytes_to_follow() != 0 ) {
	limbo += rx.payload() ;
	fprintf( stderr, "Still accumulating limbo, limbo size : %lu bytes to follow is %u \n", limbo.size(), rx.bytes_to_follow() );
      }
      else {
	limbo += rx.payload() ;
	write( tap_fd, limbo.c_str(), limbo.size() );
	fprintf(stderr, "Received %lu bytes, written out of limbo %lu bytes\n", rx.payload().size(), limbo.size());
	limbo = "";
      }

      gettimeofday( &tv, NULL );
      cumulative_bytes += packet.size();
      fprintf( stderr, "Rx packet : %lu bytes, cumulative : %lu bytes at time %f \n", packet.size(), cumulative_bytes, (double) tv.tv_sec+tv.tv_usec/1.0e6 );
    }

    /* read from tap0 */
    if ( sel.read( tap_fd ) ) {
      char buffer[1600];
      int nread = read( tap_fd, (void*) buffer, sizeof(buffer) );
      string packet( buffer, nread );
      ingress_queue.push( packet );
    }
  }
}
