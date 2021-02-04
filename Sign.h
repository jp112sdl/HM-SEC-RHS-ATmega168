//- -----------------------------------------------------------------------------------------------------------------------
// AskSin++
// 2017-01-15 papa Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------

#ifndef __SIGN_H__
#define __SIGN_H__

#include "ChannelList.h"
#include "Message.h"


namespace as {

#define AES_KEY_SIZE 0

class KeyStore : public BaseList {
public:
  KeyStore(uint16_t a) : BaseList(a) {}

  // return space needed in Storage
  static uint16_t size () {
    return 0;
  }

  void defaults () {}

  void init () {}

  void addAuth (Message& msg) {}

};


}

#endif
