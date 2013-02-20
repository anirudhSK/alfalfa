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
#include "pkt-classifier.h"

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

void skype_delays( std::string packet, uint64_t ts, std::string action )
{
  static PktClassifier skype_classifier;
  std::string hash_str = skype_classifier.pkt_hash( packet );
  uint8_t flow_id = skype_classifier.get_flow_id( packet ); 
  const MACAddress destination_address( packet.substr( 0, 6 ) );
  assert( hash_str != "" );
  if ( destination_address.is_broadcast() ){
    /* if pkt is broadcast, return, theres lots of network chatter */
    return;

  } else if ( flow_id != PktClassifier::UDP_PROTOCOL_NUM ) {
    /* Its not Skype, because the flow type isn't UDP */
    return;

  } else {
    printf( " Skype Packet %s %lu, size : %lu hash : ", action.c_str(), ts, packet.size() );
    for( uint32_t i=0; i < hash_str.size(); i++) {
      printf("%02x",(unsigned char)hash_str[i]);
    }
    printf( "\n");
  }
}

uint64_t get_absolute_us()
{
  struct timeval tv;
  gettimeofday( &tv, NULL );
  return (uint64_t)tv.tv_sec*1000000 + (uint64_t)tv.tv_usec;
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

  /* Client's MAC address */
  const char* client_mac;

  if ( argc == 6 ) {
    /* client */

    server = false;

    ip = argv[ 1 ];
    port = atoi( argv[ 2 ] );

    qdisc = get_qdisc( argv[ 3 ] );
    interface_name = argv[ 4 ];

    client_mac = argv[ 5 ];
    net = new Network::SproutConnection( "4h/Td1v//4jkYhqhLGgegw", ip, port );
  } else if ( argc == 4 ) {
    /* server */

    qdisc = get_qdisc( argv[ 1 ] );
    interface_name = argv[ 2 ];

    client_mac = argv[ 3 ];
    net = new Network::SproutConnection( NULL, NULL );
  } else {
    fprintf( stderr, "Invalid number of arguments \n" );
    exit(-1);
  }

  PacketSocket eth_socket ;
  if (server) {
    eth_socket = PacketSocket( interface_name, string(), client_mac );
  } else {
    eth_socket = PacketSocket( interface_name, client_mac, string() );
  }
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
    if ( sel.read( net->fd() ) ) {
      string packet( net->recv() );

      /* write into ethernet interface */
      if ( packet.size() != 0 ) {
        eth_socket.send_raw( packet );
        skype_delays( packet, get_absolute_us(), "SENT OUT " );
      }
    }


    /* read from ethernet interface */
    if ( sel.read( eth_socket.fd() ) ) {
      vector<string> recv_strings = eth_socket.recv_raw();
      assert ( recv_strings.size() <= 1 );
      if ( qdisc == IngressQueue::QDISC_SPROUT ) {
        const unsigned int cum_window = 1440 * 10 + 2 * net->window_predict();
        ingress_queues.set_qlimit( cum_window ); 
      }
      for ( auto it = recv_strings.begin(); it != recv_strings.end(); it++ ) {
        if ( ( *it).size() != 0 ) {
          skype_delays( *it, get_absolute_us(), "RECEIVED IN " );
          ingress_queues.enque( *it );
        }
      }
    }
  }
}
