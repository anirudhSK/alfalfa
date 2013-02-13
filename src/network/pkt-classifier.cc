#include "pkt-classifier.h"
#include <netinet/ip.h>
#include <netinet/ether.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

protocol_t PktClassifier::get_ip_header( void ) const
{
  /* Seek to the beginning of the IP header */
  const char* packet = _payload.c_str();
  const struct ip* ip_hdr;
  ip_hdr = ( struct ip*) ( packet + sizeof( struct ether_header ) );
  auto hdr_len  = ip_hdr->ip_hl;
  auto protocol = ip_hdr->ip_p;

  return protocol;
}


std::string PktClassifier::get_udp_header( std::string ip_payload ) const
{
  auto udp_hdr_with_data = ip_payload.c_str();
  const struct udphdr* udp_hdr = (struct udphdr*) ( udp_hdr_with_data );
  auto sport = ntohs( udp_hdr -> source );
  auto dport = ntohs( udp_hdr -> dest );
  printf(" UDP source port %u, dst port %u \n", sport, dport );
  return "";
}

std::string PktClassifier::get_tcp_header( std::string ip_payload ) const
{
  auto tcp_hdr_with_data = ip_payload.c_str();
  const struct tcphdr* tcp_hdr = (struct tcphdr*) ( tcp_hdr_with_data );
  auto sport = ntohs( tcp_hdr -> source );
  auto dport = ntohs( tcp_hdr -> dest );
  printf(" TCP source port %u, dst port %u \n", sport, dport );
  return "";
}

PktClassifier::PktClassifier( std::string payload ) :
  _payload( payload )
{}
