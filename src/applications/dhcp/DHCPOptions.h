// DHCP Option field
//
// Juan-Carlos Maureira
// INRIA 2008.
//

#ifndef __DHCPOPTIONS__
#define __DHCPOPTIONS__

#include <map>
#include <sstream>
#include "Byte.h"

enum op_code {
   DHCP_MSG_TYPE = 53,
   CLIENT_ID     = 61,
   HOSTNAME      = 12,
   REQUESTED_IP  = 50,
   PARAM_LIST    = 55,
   SUBNET_MASK   = 1,
   ROUTER        = 3,
   DNS           = 6,
   NTP_SRV       = 42,
   RENEWAL_TIME  = 58,
   REBIND_TIME   = 59,
   LEASE_TIME    = 51,
   SERVER_ID     = 54,
};

class DHCPOption {
	public:
    typedef std::map<op_code,Byte> DHCPOptionsMap;
  private:
		DHCPOptionsMap options;

  public:
    void set(op_code code,std::string data) {
       this->options[code] = Byte(data);
    }

    void set(op_code code,int data) {
       this->options[code] = Byte(data);
    }

    void add(op_code code,int data) {
       if (this->options.find(code) == this->options.end() ) {
         this->options[code] = Byte(data);
       } else {
         this->options[code].concat(Byte(data));
       }
    }

    Byte get(op_code code) {
       DHCPOptionsMap::iterator it = this->options.find(code);
       if (it!=this->options.end()) {
          return it->second;
       }
       return 0;
    }

    DHCPOptionsMap::iterator begin() {
      return(this->options.begin());
    }

    DHCPOptionsMap::iterator end() {
      return(this->options.end());
    }

    friend std::ostream& operator << (std::ostream& os, DHCPOption& obj) {
       for(DHCPOptionsMap::iterator it=obj.begin();it!=obj.end();it++) {
          os << "        " << it->first << " = " << it->second << endl;
       }
       return os;
    }
};
#endif
