#include "ax25_payload.h"

namespace AX25 {
  
Payload::Payload(byte *rxPayload, int payloadLength)
  : rptCallsCount_(0)
{
  isValid_ = fromBinary(rxPayload, payloadLength);
}

Payload::Payload(String inputText)
  : rptCallsCount_(0)
{
  isValid_ = fromString(inputText);
}

void Payload::Dump() 
{
  Serial.println();
  Serial.print("valid: "); Serial.println(isValid_);
  Serial.println("src: " + srcCall_.ToString());
  Serial.println("dst: " + dstCall_.ToString());
  Serial.print("rpt: ");
  for (int i = 0; i < rptCallsCount_; i++) {
    Serial.print(rptCalls_[i].ToString() + " ");
  }
  Serial.println();
  Serial.println("info: " + info_);
}

int Payload::ToBinary(byte *txPayload, int bufferLength) const
{
  byte *txPtr = txPayload;
  byte *txEnd = txPayload + bufferLength;

  // destination address
  if (!dstCall_.ToBinary(txPtr, CallsignSize)) return 0;
  txPtr += CallsignSize;
  if (txPtr >= txEnd) return 0;

  // source address
  if (!srcCall_.ToBinary(txPtr, CallsignSize)) return 0;
  txPtr += CallsignSize;
  if (txPtr >= txEnd) return 0;
  
  // digipeater addresses
  for (int i = 0; i < rptCallsCount_; i++) {
    if (!rptCalls_[i].ToBinary(txPtr, CallsignSize)) return 0;
    txPtr += CallsignSize;
    if (txPtr >= txEnd) return 0;
  }
  *(txPtr - 1) |= 1;
  
  // control + protocol id
  if ((txPtr + 2) >= txEnd) return 0;
  *(txPtr++) = AX25Ctrl::UI;
  *(txPtr++) = AX25Pid::NoLayer3;

  // information field
  for (int i = 0; i < info_.length(); i++) {
    char c = info_.charAt(i);
    if (c == '\0') break;
    *(txPtr++) = c;
    if (txPtr >= txEnd) break;
  }

  return (int)(txPtr-txPayload);
}

String Payload::ToString(String customComment)
{
  String txt = srcCall_.ToString() + String(">") + dstCall_.ToString();

  for (int i = 0; i < rptCallsCount_; i++) {
    if (rptCalls_[i].IsValid()) {
      txt += String(",") + rptCalls_[i].ToString();
    }
  }

  txt += String(":") + info_;

  if (info_.startsWith("=")) {
    txt += customComment;
  }

  return txt + String("\n");
}

bool Payload::Digirepeat(const String &ownCallsign)
{
  for (int i = 0; i < rptCallsCount_; i++) {    
    if (rptCalls_[i].Digirepeat(ownCallsign)) {
      return true;
    }
  }
  return false;
}

bool Payload::fromBinary(const byte *rxPayload, int payloadLength)
{
  const byte *rxPtr = rxPayload;
  const byte *rxEnd = rxPayload + payloadLength;
  
  if (payloadLength < CallsignSize) return false;
  
  // destination address
  dstCall_ = AX25::Callsign(rxPtr, CallsignSize);
  rxPtr += CallsignSize;
  if (rxPtr >= rxEnd) return false;

  // source address
  srcCall_ = AX25::Callsign(rxPtr, CallsignSize);
  rxPtr += CallsignSize;
  if (rxPtr >= rxEnd) return false;

  rptCallsCount_ = 0;

  // digipeater addresses
  for (int i = 0; i < RptMaxCount; i++) {
    if ((rxPayload[(i + 2) * CallsignSize - 1] & 1) == 0) {
      rptCalls_[i] = AX25::Callsign(rxPtr, CallsignSize);
      rptCallsCount_++;
      rxPtr += CallsignSize;
      if (rxPtr >= rxEnd) return false;
    }
    else {
      break;
    }
  }

  // control + protocol id
  if ((rxPtr + 2) >= rxEnd) return false;
  if (*(rxPtr++) != AX25Ctrl::UI) return false;
  if (*(rxPtr++) != AX25Pid::NoLayer3) return false;

  // information field
  while (rxPtr < rxEnd) {
    info_ += String((char)*(rxPtr++));
  }

  return true;
}

bool Payload::fromString(String inputText)
{
  int rptIndex = inputText.indexOf('>');
  int infoIndex = inputText.indexOf(':');

  // bad format
  if (rptIndex == -1 || infoIndex == -1) return false;

  info_ = inputText.substring(infoIndex + 1);
  srcCall_ = inputText.substring(0, rptIndex);
  String paths = inputText.substring(rptIndex + 1, infoIndex);
  
  rptCallsCount_ = 0;
  int index = 0;
  while (rptCallsCount_ <= RptMaxCount) {
    int nextIndex = paths.indexOf(',', index);
    String pathItem = paths.substring(index, nextIndex == -1 ? paths.length() : nextIndex);
    if (index == 0) {
      dstCall_ = pathItem;
    }
    else {
      rptCallsCount_++;
      rptCalls_[rptCallsCount_ - 1] = pathItem;
    }
    if (nextIndex == -1) break;
    index = nextIndex + 1;
  }
  
  return true;
}

} // AX25
