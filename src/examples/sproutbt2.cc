#include <unistd.h>
#include <string>
#include <assert.h>
#include <list>

#include "packet-fragment.pb.h"
#include "sproutconn.h"
#include "select.h"
#include <sys/time.h>
#include "tapdevice.hh"
#include "reassembly.hh"

using namespace std;
using namespace Network;

int main( int argc, char *argv[] )
{
  char *ip;
  int port;

  Network::SproutConnection *net;

  bool server = true;

  /* Connect to tap0 and setup Sprout tunnel */
  int tap_fd = setup_tap();

  /* Queue incoming packets from tap0 */
  uint32_t count = 0;
  std::queue<fragment::PacketFragment> ingress_queue;

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

  const int fallback_interval = 10;

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

  /* book keeping for reassembly */
  Reassembly reassembler;

  /* loop */
  while ( 1 ) {
    uint32_t bytes_to_send = net->window_size();

    /* Action 1 : actually send, maybe */
    if ( ( bytes_to_send > 0 ) || ( time_of_next_transmission <= timestamp() ) ) {
      do {
        if ( !ingress_queue.empty() ) {
          /* Queue has packets */
          /* Get HOL packet fragment */
          fragment::PacketFragment par_pkt = ingress_queue.front();

          if (bytes_to_send == 0) {
            /* Check "window" i.e. bytes_to_send is empty, simply send fallback packet */
            net->send( string(0,'x'), fallback_interval );
            break;

          } else if ( bytes_to_send < par_pkt.payload().size() ) {
            /* Window is not zero, but we don't have sufficient bytes to send HOL packet */
            /* Fragment par_pkt into 2 packets : a new "to_send" pkt and a modified HOL pkt */

            /* new to_send */
            fragment::PacketFragment to_send ;
            to_send.set_id( par_pkt.id() );         /* same id on all fragments */
            to_send.set_length( par_pkt.length() ); /* same length on all fragments */
            to_send.set_payload( par_pkt.payload().substr( 0, bytes_to_send ) );/* fragment */
            to_send.set_offset( par_pkt.offset() ); /* offset on first fragment */
            to_send.set_more_frags( true );         /* Definitely set more_frags */
            net->send( to_send.SerializeAsString(), fallback_interval );

            /* modified HOL packet */
            ingress_queue.front().set_offset( par_pkt.offset() + bytes_to_send );
            ingress_queue.front().set_payload( par_pkt.payload().substr( bytes_to_send, par_pkt.payload().size() - bytes_to_send ) );

	    fprintf( stderr, "SENDING snipped packet, seqnum %u of size %lu, leftover %lu \n",
                     to_send.id(), to_send.payload().size(), par_pkt.payload().size() - bytes_to_send );
	    break;

	  } else {
            /* Window has sufficient bytes to send the whole packet, loop around to get more if possible */
            bytes_to_send -= par_pkt.payload().size();
            assert( bytes_to_send  >= 0 );

            /* By default, the next packet will follow this one immediately */
            int time_to_next = 0;

            /* But if our window is exhausted, or queue is empty, fallback to fallback_interval */
            if ( bytes_to_send == 0 || ingress_queue.empty() ) {
              time_to_next = fallback_interval;
	    }

            /* In any case, send current packet after clearing more_frags */
            par_pkt.set_more_frags( false ); /* clear the more fragments flag on the last packet */
	    net->send( par_pkt.SerializeAsString(), time_to_next );
	    fprintf( stderr, "SENDING whole packet of seqnum %u, size %lu \n", par_pkt.id(), par_pkt.payload().size() );
	    ingress_queue.pop();
	  }

        } else {
          /* Queue is empty, simply send fallback packet and break */
	  net->send( string(0,'x'), fallback_interval );
	  bytes_to_send = 0; /* Wasted forecast */
          break;
        }
      } while ( bytes_to_send > 0 );

      time_of_next_transmission = std::max( timestamp() + fallback_interval,
					    time_of_next_transmission );
    }

    /* Action 2 : Calculate wait time  */
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

    /* Action 3 : receive on network */
    struct timeval tv;
    if ( sel.read( net->fd() ) ) {
      string packet( net->recv() );
      if (packet.size() == 0 ) {
          continue;
      }

      fragment::PacketFragment rx;
      rx.ParseFromString( packet );

      /* Add to reassembler */
      reassembler.add_fragment( rx );

      /* Is reassembly over ? */
      std::string assembled_packet = reassembler.ready_to_deliver( rx.id() );

      if (  assembled_packet != "" ) {
        fprintf( stderr, "REASSEMBLED Packet with seqnum %u with length %u \n", rx.id(), rx.length() );
        if ( write( tap_fd, assembled_packet.c_str(), rx.length() ) < 0 ) {
          perror( "tap device ");
          exit( 1 );
        }
        gettimeofday( &tv, NULL );
        cumulative_bytes += packet.size();
      }
    }

    /* Action 4: read from tap0 */
    if ( sel.read( tap_fd ) ) {
      char buffer[1600];
      int nread = read( tap_fd, (void*) buffer, sizeof(buffer) );
      string packet( buffer, nread );
      fragment::PacketFragment par_pkt;
      par_pkt.set_length( packet.size() ); /* length of original datagram */
      par_pkt.set_id( count++ ); /* sequence number of original  datagram */
      par_pkt.set_offset( 0 );   /* offset is 0 to begin with */
      par_pkt.set_more_frags( true ); /* copied from IPv4 */
      par_pkt.set_payload( packet ); /* The actual data */
      ingress_queue.push( par_pkt );
    }
  }
}
