#include <stdint.h>
#include "sercom_glue.h"
#include <SERCOM.h>
#include <variant.h>

#define S ((SERCOM*)s)

GluedSercom *sercom_getPERIPH_SPI1() {
  return &PERIPH_SPI1;
}

GluedSercom *sercom_getSercom3() {
  return &sercom3;
}

void sercom_disableSPI(GluedSercom *s) { S->disableSPI(); }
void sercom_enableSPI(GluedSercom *s) { S->enableSPI(); }
void sercom_enableWIRE(GluedSercom *s) { S->enableWIRE(); }
void sercom_initMasterWIRE(GluedSercom *s,uint32_t a) { S->initMasterWIRE(a); }
void sercom_initSPI(GluedSercom *s,int a,int b,int c,int d) { S->initSPI((SercomSpiTXPad)a,(SercomRXPad)b,(SercomSpiCharSize)c,(SercomDataOrder)d); }
void sercom_initSPIClock(GluedSercom *s,int a,uint32_t b) { S->initSPIClock((SercomSpiClockMode)a,b); }
uint8_t sercom_isAddressMatch(GluedSercom *s) { return S->isAddressMatch(); }
uint8_t sercom_isDataReadyWIRE(GluedSercom *s) { return S->isDataReadyWIRE(); }
uint8_t sercom_isMasterReadOperationWIRE(GluedSercom *s) { return S->isMasterReadOperationWIRE(); }
uint8_t sercom_isRestartDetectedWIRE(GluedSercom *s) { return S->isRestartDetectedWIRE(); }
uint8_t sercom_isSlaveWIRE(GluedSercom *s) { return S->isSlaveWIRE(); }
uint8_t sercom_isStopDetectedWIRE(GluedSercom *s) { return S->isStopDetectedWIRE(); }
void sercom_prepareAckBitWIRE(GluedSercom *s) { S->prepareAckBitWIRE(); }
void sercom_prepareCommandBitsWire(GluedSercom *s,uint8_t a) { S->prepareCommandBitsWire(a); }
uint8_t sercom_sendDataMasterWIRE(GluedSercom *s,uint8_t a) { return S->sendDataMasterWIRE(a); }
uint8_t sercom_sendDataSlaveWIRE(GluedSercom *s,uint8_t a) { return S->sendDataSlaveWIRE(a); }
void sercom_setBaudrateSPI(GluedSercom *s,uint8_t a) { S->setBaudrateSPI(a); }
void sercom_setClockModeSPI(GluedSercom *s,uint8_t a) { S->setClockModeSPI((SercomSpiClockMode)a); }
uint8_t sercom_startTransmissionWIRE(GluedSercom *s,uint8_t a,uint8_t b) { return S->startTransmissionWIRE(a,(SercomWireReadWriteFlag)b); }
uint8_t sercom_transferDataSPI(GluedSercom *s,uint8_t a) { return S->transferDataSPI(a); }
