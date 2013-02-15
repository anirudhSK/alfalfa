#include <unistd.h>
#include <string>
#include <assert.h>
#include <list>
#include <sys/time.h>

#include "sproutconn.h"
#include "select.h"
#include "tapdevice.hh"
#include "ingress-queue.h"
#include "tracked-packet.h"
#include "codel.h"
#include "queue-gang.h"
#include "packetsocket.hh"

using namespace std;
using namespace Network;

int get_qdisc( const char* qdisc_str )
{
  if ( strcmp( qdisc_str, "CoDel" ) == 0 ) {
    return IngressQueue::QDISC_CODEL;
  } else if ( strcmp( qdisc_str, "Sprout" ) == 0 ) {
    return IngressQueue::QDISC_SPROUT;
  } else {
    fprintf( stderr, "Invalid QDISC \n" );
    exit(-1);
  }
}

int main( int argc, char *argv[] )
{
  char *ip;
  int port;

  Network::SproutConnection *net;

  bool server = true;

  /* CoDel vs Sprout */
  int qdisc = 0;

  /* Interface to the ethernet */
  const char* interface_name;

  if ( argc == 5 ) {
    /* client */

    server = false;

    ip = argv[ 1 ];
    port = atoi( argv[ 2 ] );

    qdisc = get_qdisc( argv[ 3 ] );
    interface_name = argv[ 4 ];

    net = new Network::SproutConnection( "4h/Td1v//4jkYhqhLGgegw", ip, port );
  } else if ( argc == 3 ) {
    /* server */

    qdisc = get_qdisc( argv[ 1 ] );
    interface_name = argv[ 2 ];

    net = new Network::SproutConnection( NULL, NULL );
  } else {
    fprintf( stderr, "Invalid number of arguments \n" );
    exit(-1);
  }

  PacketSocket eth_socket( interface_name, string(), string() );
  fprintf( stderr, "Port bound is %d\n", net->port() );
  printf( "qdisc is %d \n", qdisc );   

  /* Queue incoming packets from ethernet interface */
  QueueGang ingress_queues = QueueGang( qdisc );

  Select &sel = Select::get_instance();
  sel.add_fd( net->fd() );
  sel.add_fd( eth_socket.fd() );

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

  /* loop */
  while ( 1 ) {
    int bytes_to_send = net->window_size();
    bool sent = false;

    while ( (bytes_to_send > 0) && (!ingress_queues.empty()) ) {
      /* close window */
      string packet_to_send = ingress_queues.get_next_packet();
      bytes_to_send -= packet_to_send.size();

      int time_to_next = 0;
      if ( ingress_queues.empty() || (bytes_to_send <= 0) ) {
	time_to_next = fallback_interval;
      }

      net->send( packet_to_send, time_to_next);
      sent = true;
    }

    if ( (!sent) && (time_of_next_transmission <= timestamp()) ) {
      /* send fallback packet */
      net->send( "", fallback_interval );
      sent = true;
    }

    if ( sent ) {
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

      /* write into ethernet interface */
      eth_socket.send_raw( packet );

      gettimeofday( &tv, NULL );
      cumulative_bytes += packet.size();
    }


    /* read from ethernet interface */
    if ( sel.read( eth_socket.fd() ) ) {
      vector<string> recv_strings = eth_socket.recv_raw();
      assert ( recv_strings.size() <= 1 );
      string packet = ( recv_strings.size() == 0 ) ? "" : recv_strings.at( 0 );
      if ( qdisc == IngressQueue::QDISC_SPROUT ) {
        const unsigned int cum_window = 1440 * 10 + 2 * net->window_predict();
        ingress_queues.set_qlimit( cum_window ); 
      }
      ingress_queues.enque( packet );
    }
  }
}
