#ifndef PKT_CLASSIFIER_HH
#define PKT_CLASSIFIER_HH

#include<string>

class PktClassifier {
  private :
    const std::string _payload;
    std::string _ip_header;
    std::string _udp_header;
    std::string _tcp_header;
    static const uint16_t TCP_PROTOCOL_NUM = 6;
    static const uint16_t UDP_PROTOCOL_NUM = 17;
    static const uint16_t ICMP_PROTOCOL_NUM = 1;

  public :
    std::string  get_ip_header( void ) const;
    std::string get_tcp_header( std::string ip_payload ) const;
    std::string get_udp_header( std::string ip_payload ) const;
    PktClassifier( std::string payload);

};


#endif
