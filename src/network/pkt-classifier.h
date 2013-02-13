#ifndef PKT_CLASSIFIER_HH
#define PKT_CLASSIFIER_HH

#include<string>

typedef u_int8_t  protocol_t;
typedef u_int16_t source_t;
typedef u_int16_t dest_t;

class PktClassifier {
  private :
    const std::string _payload;

  public :
    static const uint16_t TCP_PROTOCOL_NUM = 6;
    static const uint16_t UDP_PROTOCOL_NUM = 17;
    static const uint16_t ICMP_PROTOCOL_NUM = 1;

    protocol_t  get_ip_header( void ) const;
    std::string get_tcp_header( std::string ip_payload ) const;
    std::string get_udp_header( std::string ip_payload ) const;
    PktClassifier( std::string payload);

};


#endif
