//- -----------------------------------------------------------------------------------------------------------------------
// AskSin++
// 2016-10-31 papa Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------

#ifndef __MULTICHANNELDEVICE_H__
#define __MULTICHANNELDEVICE_H__

#include "Device.h"
#include "Defines.h"
#include "Led.h"
#include "Activity.h"

#if ARDUINO_ARCH_AVR
#include <avr/wdt.h>
#endif

namespace as {

extern void(* resetFunc) (void);

#define REPLAY_NO   0
#define REPLAY_ACK  1
#define REPLAY_NACK 2

template <class HalType, class ChannelType, int ChannelCount, class List0Type = List0>
class ChannelDevice : public Device<HalType, List0Type> {

    List0Type    list0;
    ChannelType* devchannels[ChannelCount];
    uint8_t      cfgChannel;
    GenericList  cfgList;

  public:

    typedef Device<HalType, List0Type> DeviceType;

    ChannelDevice (const DeviceInfo& i, uint16_t addr) : DeviceType(i, addr, list0, ChannelCount), list0(addr + this->keystore().size()), cfgChannel(0xff) {}

    virtual ~ChannelDevice () {}

    void registerChannel(ChannelType& ch, uint8_t num) {
      if ( num >= 1 && num <= ChannelCount) {
        devchannels[num - 1] = &ch;
      }
    }

    void layoutChannels () {
      uint16_t addr = list0.address() + list0.size();
      for ( uint8_t i = 0; i < this->channels(); ++i ) {
        devchannels[i]->setup(this, i + 1, addr);
        addr += devchannels[i]->size();
      }
    }

    void channels (uint8_t num) {
      DeviceType::channels(min(num, (uint8_t)ChannelCount));
    }

    uint8_t channels () const {
      return DeviceType::channels();
    }

    List0Type& getList0 () {
      return list0;
    }

    bool init (HalType& hal) {
      layoutChannels();
      this->keystore().init();
      this->setHal(hal);
      HMID id;
      this->getDeviceID(id);
      hal.init(id);
      hal.config(this->getConfigArea());
      return false;
    }

    void initDone () {
      // trigger initial config changed - to allow scan/caching of list data
      this->configChanged();
      for ( uint8_t cdx = 1; cdx <= channels(); ++cdx ) {
        channel(cdx).configChanged();
      }
      // trigger save of storage
      storage().store();
    }

    void firstinit () {
      this->keystore().defaults(); // init aes key infrastructure
      list0.defaults();
      for ( uint8_t i = 0; i < this->channels(); ++i ) {
        devchannels[i]->firstinit();
      }
    }

    void reset () {
      storage().reset();
      storage().store();
      firstinit ();
      resetFunc();
    }

    void bootloader () {
#if ARDUINO_ARCH_AVR
      wdt_enable(WDTO_250MS);
      while (1);
#endif
    }

    void startPairing () {
      this->sendDeviceInfo();
      this->led().set(LedStates::pairing);
      this->activity().stayAwake( seconds2ticks(15) );
    }

    ChannelType& channel(uint8_t ch) {
      return *devchannels[ch - 1];
    }

    bool pollRadio () {
      bool worked = DeviceType::pollRadio();
      for ( uint8_t i = 1; i <= this->channels(); ++i ) {
        ChannelType& ch = channel(i);
        if ( ch.changed() == true ) {
          this->sendInfoActuatorStatus(this->getMasterID(), this->nextcount(), ch);
          worked = true;
        }
      }
      return worked;
    }

    bool validSignature(Message& msg) {
      return true;
    }

    bool validSignature(uint8_t ch, Message& msg) {
      return true;
    }

    bool process(Message& msg) {
      uint8_t answer = REPLAY_NO;
      ChannelType* ch = 0;
      HMID devid;
      this->getDeviceID(devid);
      if ( msg.to() == devid || this->isBroadcastMsg(msg) ) {
        // we got a message - we do not answer before 100ms
        this->radio().setSendTimeout(); // use default value from radio
        // ignore repeated messages
        if ( this->isRepeat(msg) == true ) {
          return false;
        }
        // start processing the message
        uint8_t mtype = msg.type();
        uint8_t mcomm = msg.command();
        uint8_t msubc = msg.subcommand();
        if ( mtype == AS_MESSAGE_CONFIG ) {
          // PAIR_SERIAL
          if ( msubc == AS_CONFIG_PAIR_SERIAL && this->isDeviceSerial(msg.data()) == true ) {
            this->led().set(LedStates::pairing);
            this->activity().stayAwake( seconds2ticks(20) ); // 20 seconds
            this->sendDeviceInfo(msg.from(), msg.length());
          }
          // CONFIG_PEER_ADD
          else if ( msubc == AS_CONFIG_PEER_ADD ) {
            const ConfigPeerAddMsg& pm = msg.configPeerAdd();
            bool success = false;
            if ( this->hasChannel(pm.channel()) == true ) {
              if ( validSignature(pm.channel(), msg) == true ) {
                ch = &channel(pm.channel());
                if ( pm.peers() == 1 ) {
                  success = ch->peer(pm.peer1());
                }
                else {
                  success = ch->peer(pm.peer1(), pm.peer2());
                }
              }
            }
            if ( success == true ) {
              ch->configChanged();
              storage().store();
              answer = REPLAY_ACK;
            }
            else {
              answer = REPLAY_NACK;
            }
          }
          // CONFIG_PEER_REMOVE
          else if ( msubc == AS_CONFIG_PEER_REMOVE ) {
            const ConfigPeerRemoveMsg& pm = msg.configPeerRemove();
            bool success = false;
            if ( this->hasChannel(pm.channel()) == true ) {
              if ( validSignature(pm.channel(), msg) == true ) {
                ch = &channel(pm.channel());
                success = ch->deletepeer(pm.peer1());
                if ( pm.peers() == 2 ) {
                  success &= ch->deletepeer(pm.peer2());
                }
              }
            }
            if ( success == true ) {
              ch->configChanged();
              storage().store();
              answer = REPLAY_ACK;
            }
            else {
              answer = REPLAY_NACK;
            }
          }
          // CONFIG_PEER_LIST_REQ
          else if ( msubc == AS_CONFIG_PEER_LIST_REQ ) {
            const ConfigPeerListReqMsg& pm = msg.configPeerListReq();
            if ( this->hasChannel(pm.channel()) == true ) {
              this->sendInfoPeerList(msg.from(), msg.count(), channel(pm.channel()));
            }
          }
          // CONFIG_PARAM_REQ
          else if (msubc == AS_CONFIG_PARAM_REQ ) {
            const ConfigParamReqMsg& pm = msg.configParamReq();
            GenericList gl = findList(pm.channel(), pm.peer(), pm.list());
            if ( gl.valid() == true ) {
              this->sendInfoParamResponsePairs(msg.from(), msg.count(), gl);
            }
            else {
              answer = REPLAY_NACK;
            }
          }
          // CONFIG_STATUS_REQUEST
          else if (msubc == AS_CONFIG_STATUS_REQUEST ) {
            // this is an answer to a request - so we need no ack
            this->sendInfoActuatorStatus(msg.from(), msg.count(), channel(msg.command()), false);
          }
          // CONFIG_START
          else if ( msubc == AS_CONFIG_START ) {
            const ConfigStartMsg& pm = msg.configStart();
            if ( validSignature(pm.channel(), msg) == true ) {
              cfgChannel = pm.channel();
              cfgList = findList(cfgChannel, pm.peer(), pm.list());
              // TODO setup alarm to disable after 2000ms
              answer = REPLAY_ACK;
            }
            else {
              answer = REPLAY_NACK;
            }
          }
          // CONFIG_END
          else if ( msubc == AS_CONFIG_END ) {
            if ( cfgList.address() == list0.address() ) {
              this->led().set(LedStates::nothing);
              this->configChanged();
            }
            else {
              // signal list update to channel
              channel(cfgChannel).configChanged();
            }
            cfgChannel = 0xff;
            storage().store();
            // TODO cancel alarm
            this->sendAck(msg, Message::WKMEUP);
          }
          else if ( msubc == AS_CONFIG_WRITE_INDEX ) {
            const ConfigWriteIndexMsg& pm = msg.configWriteIndex();
            if ( validSignature(pm.channel(), msg) == true ) {
              if ( cfgChannel == pm.channel() && cfgList.valid() == true ) {
                this->writeList(cfgList, pm.data(), pm.datasize());
              }
              answer = REPLAY_ACK;
            }
            else {
              answer = REPLAY_NACK;
            }
          }
          else if ( msubc == AS_CONFIG_SERIAL_REQ ) {
            this->sendSerialInfo(msg.from(), msg.count());
          }
          else {
            answer = REPLAY_NACK;
          }
        }
        else if ( mtype == AS_MESSAGE_ACTION ) {
          if ( mcomm == AS_ACTION_RESET || mcomm == AS_ACTION_ENTER_BOOTLOADER ) {
            if ( validSignature(msg) == true ) {
              this->sendAck(msg);
              if ( mcomm == AS_ACTION_ENTER_BOOTLOADER ) {
                bootloader();
              }
              else {
                reset();
              }
            }
          }

        }
        else if ( mtype == AS_MESSAGE_HAVE_DATA ) {
          answer = REPLAY_ACK;
        }

        // default - send Nack if answer is requested
        else {
          answer = REPLAY_NACK;
        }
      }
      else {
        return false;
      }
      // send ack/nack
      if ( msg.ackRequired() == true && msg.to() == devid ) {
        if ( answer == REPLAY_ACK ) {
          if ( ch != 0 ) this->sendAck(msg, *ch);
          else this->sendAck(msg);
        }
        else if ( answer == REPLAY_NACK ) {
          this->sendNack(msg);
        }
      }
      // we always stay awake after valid communication
      this->activity().stayAwake(millis2ticks(500));
      return true;
    }

    GenericList findList(uint8_t ch, const Peer& peer, uint8_t numlist) {
      if (numlist == 0) {
        return list0;
      } else if (this->hasChannel(ch) == true) {
        ChannelType& c = channel(ch);
        if (numlist == 1) {
          return c.getList1();
        } else if (c.hasList4() && numlist == 4) {
          return c.getList4(peer);
        }
      }
      return GenericList();
    }

    void sendPeerEvent (Message& msg, const ChannelType& ch) {
      // we send only to peers if there is no config message pending
      if ( cfgChannel != 0xff ) {
        DeviceType::sendPeerEvent(msg, ch);
      }
    }
};


template <class HalType, class ChannelType, int ChannelCount, class List0Type = List0>
class MultiChannelDevice : public ChannelDevice<HalType, ChannelType, ChannelCount, List0Type> {

    ChannelType cdata[ChannelCount];

  public:

    typedef ChannelDevice<HalType, ChannelType, ChannelCount, List0Type> DeviceType;

    MultiChannelDevice (const DeviceInfo& i, uint16_t addr) : ChannelDevice<HalType, ChannelType, ChannelCount, List0Type>(i, addr) {
      for ( uint8_t i = 0; i < ChannelCount; ++i ) {
        this->registerChannel(cdata[i], i + 1);
      }
    }

    virtual ~MultiChannelDevice () {}
};

}

#endif
