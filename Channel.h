//- -----------------------------------------------------------------------------------------------------------------------
// AskSin++
// 2016-10-31 papa Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------

#ifndef __CHANNEL_H__
#define __CHANNEL_H__

#include "Peer.h"
#include "Storage.h"

namespace as {

template <class HalType,class List0Type> class Device;
class ActionSetMsg;
class RemoteEventMsg;
class SensorEventMsg;

template<class HalType,class List1Type,class List4Type,int PeerCount,class List0Type=List0>
class Channel {
  Device<HalType,List0Type>*   dev;
  bool      change : 1; // the status is changed, we may need to send a status
  bool      inhi   : 1;
  uint8_t   num   ; // channels per device
  uint16_t  addr  ; // start address in eeprom

public:
  typedef List1Type List1;
  typedef List4Type List4;
  typedef Device<HalType,List0Type> DeviceType;

  public:
  Channel () : dev(0), change(false), inhi(false), num(0), addr(0) {}

  DeviceType& device () { return *dev; }

  const DeviceType& device () const { return *dev; }

  uint8_t number () const { return num; }

  uint16_t address () const { return addr; }

  uint8_t peers () const { return PeerCount; }

  bool changed () const { return change; }

  void changed (bool c) { change = c; }

  void inhibit (bool value) { inhi = value; }

  bool inhibit () const { return inhi; }

  void setup(Device<HalType,List0Type>* dev,uint8_t number,uint16_t addr) {
    this->dev = dev;
    this->num = number;
    this->addr = addr;
  }

  uint16_t size () const {
    uint16_t size = sizeof(Peer);
    size += List4::size();
    size *= PeerCount;
    size += List1::size();
    return size;
  }

  bool has (const Peer& p) const {
    for( uint8_t i=0; i<peers(); ++i ) {
      if( p == peer(i) ) {
        return true;
      }
    }
    return false;
  }

  uint8_t peerfor (const HMID& hmid) const {
    for( uint8_t i=0; i<peers(); ++i ) {
      if( hmid == peer(i) ) {
        return i;
      }
    }
    return 0xff;
  }

  Peer peer (uint8_t idx) const {
    Peer result;
    uint16_t paddr = peerAddress(idx);
    if( paddr != 0 ) {
      storage().getData(paddr,&result);
    }
    return result;
  }


  bool peer (const Peer& p) {
    deletepeer(p);
    uint8_t pidx = findpeer();
    if( pidx != 0xff ) {
      storage().setData(peerAddress(pidx),p);
     // getList3(pidx).single();
      return true;
    }
    return false;
  }

  bool peer (const Peer& p1,const Peer& p2) const {
    deletepeer(p1);
    deletepeer(p2);
    uint8_t pidx1 = findpeer();
    if( pidx1 != 0xff ) {
      storage().setData(peerAddress(pidx1),p1);
      uint8_t pidx2 = findpeer();
      if( pidx2 != 0xff ) {
        storage().setData(peerAddress(pidx2),p2);
        if( p1.odd() == true ) {
          //getList3(pidx1).odd();
          //getList3(pidx2).even();
        }
        else {
          //getList3(pidx2).odd();
          //getList3(pidx1).even();
        }
        return true;
      }
      else {
        // free already stored data
        deletepeer(p1);
      }
    }
    return false;
  }

  bool deletepeer (uint8_t idx) const {
    return storage().setData(peerAddress(idx),Peer());
  }

  uint8_t findpeer () const {
    for( int i=0; i<peers(); ++i ) {
      if( peer(i).valid()==false ) {
        return i;
      }
    }
    return 0xff;
  }

  bool deletepeer (const Peer& p) const {
    for( uint8_t i=0; i<peers(); ++i ) {
      if( peer(i) == p ) {
        deletepeer(i);
      }
    }
    return true;
  }

  void firstinit () {
    storage().clearData(address(),size());
    List1Type cl1 = getList1();
    cl1.defaults();
  }

  List1Type getList1 () const {
    // we start always with list1
    return List1Type(address());
  }

 /* List3Type getList3 (const Peer& p) const {
    uint16_t liststart = 0x00;
    if( hasList3() == true ) {
      for( uint8_t i=0; i<peers(); ++i ) {
        if( peer(i) == p ) {
          liststart = peerAddress(i) + sizeof(Peer);
          break;
        }
      }
    }
    return List3Type(liststart);
  }*/

  List4Type getList4 (const Peer& p) const {
    uint16_t liststart = 0x00;
    if( hasList4() == true ) {
      for( uint8_t i=0; i<peers(); ++i ) {
        if( peer(i) == p ) {
          liststart = peerAddress(i) + sizeof(Peer) ;
          break;
        }
      }
    }
    return List4Type(liststart);
  }

  /*List3Type getList3 (uint8_t pidx) const {
    uint16_t liststart = 0x00;
    if( hasList3() == true && pidx < peers() ) {
      liststart = peerAddress(pidx) + sizeof(Peer);
    }
    return List3Type(liststart);
  }*/

  List4Type getList4 (uint8_t pidx) const {
    uint16_t liststart = 0x00;
    if( hasList4() == true && pidx < peers() ) {
      liststart = peerAddress(pidx) + sizeof(Peer) ;
    }
    return List4Type(liststart);
  }

  static bool hasList3 () {
    return false;// List3Type::size() > 0;
  }

  static bool hasList4 () {
    return List4Type::size() > 0;
  }

  void stop () {}

  bool process (__attribute__((unused)) const ActionSetMsg& msg) {
    return false;
  }

  bool process (__attribute__((unused)) const ActionCommandMsg& msg) {
    return false;
  }
 
  bool process (__attribute__((unused)) const RemoteEventMsg& msg) {
    return false;
  }

  bool process (__attribute__((unused)) const SensorEventMsg& msg) {
    return false;
  }

  void patchStatus (__attribute__((unused)) Message& msg) {}

  void configChanged () {}

  protected:
  uint16_t peerAddress (uint8_t pidx) const {
    if( pidx < PeerCount ) {
      uint16_t offset = sizeof(Peer);
      offset += List4::size();
      offset *= pidx;
      offset += List1::size() ;
      return addr + offset;
    }
    return 0x00;
  }
};



}

#endif
